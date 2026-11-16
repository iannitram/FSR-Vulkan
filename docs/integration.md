# Integrating `uscale`

`uscale` is the shared C11 upscaling and frame-generation API for `-universal`
games. The game owns Vulkan: its instance, physical and logical devices,
queues, command buffers, images, synchronization and presentation stay in the
renderer. `uscale` chooses a backend and records work into the command buffer
you provide; it does not submit work or present.

## Add the library with CMake

`universal/` is the CMake source directory for the library. Pin the submodule
or FetchContent revision in your game so builds use a known API.

### Existing submodule or checkout

If the game has this repository checked out at `gpu/third_party/uscale`:

```cmake
add_subdirectory(gpu/third_party/uscale/universal uscale-build)
target_link_libraries(game PRIVATE uscale::uscale)
```

If the checkout is the submodule itself, add its `universal` directory:

```cmake
add_subdirectory(gpu/third_party/fsr-vulkan/universal uscale-build)
target_link_libraries(game PRIVATE uscale::uscale)
```

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(uscale_source
    GIT_REPOSITORY https://github.com/iannitram/FSR-Vulkan.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  universal)
FetchContent_MakeAvailable(uscale_source)
target_link_libraries(game PRIVATE uscale::uscale)
```

`SOURCE_SUBDIR` needs CMake 3.18 or newer in the game's project. Older
`FetchContent_Populate` recipes are deprecated from CMake 3.30 on.

For a reproducible game build, replace `main` with the reviewed commit or tag
you have pinned. Both forms define the CMake target `uscale::uscale`. The
library needs CMake 3.16 and a C11 compiler. When Vulkan development files are
found, `USCALE_WITH_VULKAN` defaults on and the target includes the Vulkan
implementation; set it to `OFF` only for clients that use the platform
independent API in `uscale.h` alone. `USCALE_BUILD_TESTS` defaults on for a
standalone library checkout; a game embedding the library may set it `OFF`
before adding the directory.

The small cross-platform consumer in
[`universal/examples/minimal`](../universal/examples/minimal/) shows a complete
configure, link and call. From the repository root:

```sh
cmake -S universal/examples/minimal -B build-minimal
cmake --build build-minimal --config Release
./build-minimal/uscale_minimal
```

On multi-configuration generators, run
`build-minimal/Release/uscale_minimal.exe` on Windows. The example uses the
platform independent API so it can build on all three CI platforms without a
Vulkan SDK or GPU.

## Call the platform independent API

Include `uscale.h` and use `uscale_render_extent` to choose the active render
size. `uscale_version()` can be compared with `USCALE_VERSION` when checking
the linked runtime. Structures with `struct_size` fields should be initialized
with `sizeof(the_structure)` so newer library versions can extend them.

```c
#include "uscale.h"

uscale_extent render_extent;
uscale_result result = uscale_render_extent(
    USCALE_QUALITY_QUALITY,
    (uscale_extent){ 1920, 1080 },
    &render_extent);
if (result != USCALE_OK) {
    /* Handle or log uscale_result_name(result). */
}
```

The quality presets are `USCALE_QUALITY_NATIVE_AA`, `QUALITY`, `BALANCED`,
`PERFORMANCE` and `ULTRA_PERFORMANCE`. The selected size is the size at which
the renderer should produce its color, depth and motion images.

## Connect the Vulkan renderer

For actual upscaling, include `uscale_vulkan.h` as well. Configure the Vulkan
half (the default when Vulkan is found) and link `uscale::uscale`. Use the
renderer’s existing Vulkan handles and function pointers; do not create a
second device for the library. FSR 3 requires Vulkan 1.2 or later and the
logical device feature `shaderStorageImageWriteWithoutFormat` enabled.

The call sequence is:

1. Fill `uscale_device_caps` with `uscale_query_caps`, passing the physical
   device, `vkGetInstanceProcAddr`, instance and optional FSR 4 asset folder.
2. Fill `uscale_context_create_info` with `struct_size`, `USCALE_VERSION`, the
   Vulkan handles, preferred backend, quality, maximum render extent, display
   extent and optional FSR 4 asset folder. Call `uscale_context_create` and
   check its result. `USCALE_BACKEND_AUTO` selects the best available path;
   `uscale_context_backend` reports the actual backend and reason.
3. Each frame, call `uscale_render_extent` for the current display extent. Use
   that render extent to render color, depth and motion. Fill
   `uscale_upscale_info` with `struct_size`, the game command buffer, image
   handles/views/formats/extents/layouts, active render extent, pixel jitter,
   frame time, sharpness and camera values. Call `uscale_record_upscale` while
   recording the frame's commands.
4. If frame generation is enabled, record it in two steps each real frame,
   after the scene is rendered (see [Frame generation](#frame-generation)):
   `uscale_record_framegen_prepare`, then `uscale_record_framegen_dispatch`,
   which returns the present plan.
5. After the game's fence for a frame signals, call
   `uscale_retire_frame(context, frame_id)`. Destroy the context with
   `uscale_context_destroy` when the renderer shuts down.

## Frame generation

FSR 3.1.6 frame generation and upstream's presenter policy. The game keeps
its swapchain: the library never acquires, presents or owns it.

Create the context with `enable_frame_generation = 1` and
`framegen_color_format` set to `VK_FORMAT_R8G8B8A8_UNORM` or
`VK_FORMAT_R16G16B16A16_SFLOAT` (anything else is
`USCALE_ERROR_UNSUPPORTED`). The field was added in 0.2: it is read only when
`struct_size` covers it and `api_version` is 0.2 or later, so a caller built
against 0.1 gets a working context without frame generation. When the device
offers `VK_KHR_compute_shader_derivatives`, enable it with its
`computeDerivativeGroupLinear` feature, because upstream's frame-generation shaders use it.

Images:

- the real frame (`present_color`): display size, the context's colour format,
  sampled and a transfer source;
- `depth`: render size, `VK_FORMAT_R32_SFLOAT`, sampled (a colour image, not a
  D32 depth image; copy or write depth into it);
- `motion`: render size, `R16G16_SFLOAT`, sampled, the same UV convention as
  `uscale_upscale_info`;
- `generated`: display size, the context's colour format, sampled and storage,
  distinct from every input;
- `distortion`: optional `R16G16_SFLOAT`, sampled; `image = VK_NULL_HANDLE` for none.

Each real frame:

1. Render the scene into the real frame.
2. Fill `uscale_framegen_prepare_info` (the real frame, depth, motion, render
   extent, jitter, frame time, camera near/far/FOV, `view_space_to_meters`,
   camera vectors, luminance range, transfer function, `frame_id`, `reset`)
   and call `uscale_record_framegen_prepare`. `frame_id` is above 0 and goes
   up by one each real frame.
3. Acquire two swapchain images, the generated target first and then the real
   target, and describe the outcome in `uscale_acquired_images` (`paired = 0`
   when only the first image was acquired).
4. Fill `uscale_framegen_dispatch_info` with the same real frame, the
   `generated` image, frame time, the same `frame_id`, `reset` and the acquire
   outcome, then call `uscale_record_framegen_dispatch`. It reuses the
   prepare's camera, luminance and transfer values.
5. Record, submit and present `out_plan->slots` in order. Two slots are always
   the generated target then the real target. When `use_generated = 1`, copy or
   draw the `generated` image into that slot's swapchain image; otherwise draw
   the real frame. On a `reset` frame, slot 0 shows the real scene but must
   still be presented to release its image. One slot means present normally.
6. After the frame's fence signals, call `uscale_retire_frame(context, frame_id)`.

```c
uscale_framegen_prepare_info prep = { sizeof(prep) };
/* ... fill prep ... */
uscale_record_framegen_prepare(ctx, &prep);

uscale_framegen_dispatch_info disp = { sizeof(disp) };
/* ... fill disp, disp.frame_id = prep.frame_id, disp.acquired = ... */
uscale_present_plan plan;
if (uscale_record_framegen_dispatch(ctx, &disp, &plan) == USCALE_ERROR_INVALID_ARGUMENT &&
    plan.slot_count == 0) {
    /* bad input or an acquire outcome that cannot be planned: recreate the swapchain */
}
for (uint32_t i = 0; i < plan.slot_count; ++i) {
    /* draw plan.slots[i].use_generated ? generated : real frame into
       plan.slots[i].image_index, wait on plan.slots[i].image_available,
       submit and present */
}
```

Rules:

- A dispatch must follow its own prepare. A dispatch with no prepare for that
  `frame_id`, or a stale one, is `USCALE_ERROR_INVALID_ARGUMENT` and records
  nothing.
- Every float must be finite; `frame_time_ms`, `camera_near`, `camera_fov_y`
  and `view_space_to_meters` must be above 0, `camera_far` above
  `camera_near`, and `0 <= min_luminance <= max_luminance`. A value outside
  these rules is `USCALE_ERROR_INVALID_ARGUMENT` and records nothing.
- An acquire outcome upstream cannot plan, for example duplicate indices, is
  `USCALE_ERROR_INVALID_ARGUMENT`: recreate the swapchain.
- If upstream fails after recording started, the call returns that error and
  still fills the plan with slot 0 showing the real scene, so both acquired
  images can be presented. Retire that frame before the next prepare, as you
  must for any prepare that has no successful dispatch.
- Built without `USCALE_WITH_FFX`, or on a context created without frame
  generation, both calls return `USCALE_ERROR_UNSUPPORTED`.

Keep image layouts accurate: each `uscale_image.layout` is the layout when
the recorded work begins. The application remains responsible for barriers,
submitting the command buffer, fence management, and presenting the real and
generated frames. Check every `uscale_result`; a requested backend can fall
back to FSR 3, and backend creation can report unsupported or missing runtime
conditions. Vendor runtimes and FSR 4 assets are not bundled with this
library; see [licences and runtime notes](licences.md).
