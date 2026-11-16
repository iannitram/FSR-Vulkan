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
    GIT_SHALLOW    TRUE)
FetchContent_GetProperties(uscale_source)
if(NOT uscale_source_POPULATED)
    FetchContent_Populate(uscale_source)
endif()
add_subdirectory(${uscale_source_SOURCE_DIR}/universal
                 ${uscale_source_BINARY_DIR}/universal)
target_link_libraries(game PRIVATE uscale::uscale)
```

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
4. If frame generation is enabled, fill `uscale_framegen_info` with the real
   display-size color image and a distinct generated image, increasing
   `frame_id` by one for every real frame, then call
   `uscale_record_framegen`.
5. After the game's fence for a frame signals, call
   `uscale_retire_frame(context, frame_id)`. Destroy the context with
   `uscale_context_destroy` when the renderer shuts down.

Keep image layouts accurate: each `uscale_image.layout` is the layout when
the recorded work begins. The application remains responsible for barriers,
submitting the command buffer, fence management, and presenting the real and
generated frames. Check every `uscale_result`; a requested backend can fall
back to FSR 3, and backend creation can report unsupported or missing runtime
conditions. Vendor runtimes and FSR 4 assets are not bundled with this
library; see [licences and runtime notes](licences.md).
