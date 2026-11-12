# FSR 3.1.6 frame-generation allocation regression (#28)

Build the test through our CMake harness with a C++20 compiler, Vulkan headers
and loader import library, and a Vulkan 1.2 compute device (a real GPU or
lavapipe). No vendor model files or shader compilation tools are needed.

```sh
cmake -S universal -B build-uscale -DCMAKE_BUILD_TYPE=Release -DUSCALE_WITH_VULKAN=ON -DUSCALE_TEST_FSR3_FRAMEGEN=ON
cmake --build build-uscale --config Release --parallel
ctest --test-dir build-uscale -C Release --output-on-failure
ctest --test-dir build-uscale -C Release -R uscale_vk_framegen_retire_test -V
```

The option adds upstream with `EXCLUDE_FROM_ALL` and upstream tests disabled,
then links only `ffx-vulkan::fsr3-vk-framegeneration-3.1.6`. Recovered targets
are not built. The normal uscale build still needs only C11.

The test reuses upstream's valid image and prepare/dispatch fixtures without
editing them. It runs the actual optical-flow and frame-interpolation scheduler
at 64x64 render / 128x128 display resolution. Test-only Vulkan ABI wrappers count
successful `vkAllocateMemory` / `vkFreeMemory` calls and live descriptor pools
(the bridge allocates one set per pool). Calls forward to the real device
functions obtained through `vkGetDeviceProcAddr`. The public memory-usage query
alone would miss the leaked staged constants and descriptors.

After one warm-up frame it generates 1,002 more frames, retaining allocations
for batches of three. Each submission completes with `vkQueueWaitIdle` before
retirement. The test checks the first batch's allocation bound on every later
frame, verifies partial retirement preserves newer IDs, and checks that every
fully retired batch returns to exactly the same allocation count and byte
total, with zero live descriptor pools. Teardown must leave zero tracked
allocations. An otherwise-valid prepare with frame id 0 must return
`FFX_VK_FSR3_3_1_6_FRAMEGEN_ERROR_INVALID_ARGUMENT` without allocating, followed
by a successful frame id 1 on the same context.

## Measured Windows run

2026-11-11, Windows 11, MSVC 19.44.35227, NVIDIA GeForce RTX 3090 Ti,
NVIDIA driver 610.88, Vulkan 1.4.341:

| Measurement | Result |
|---|---|
| Generated frames | 1,003 (1 warm-up + 1,002 measured) |
| Resident warm baseline, including application images | 53 allocations / 704,128 bytes |
| Peak with three retained frames | 92 allocations / 707,392 bytes |
| Peak transient memory above baseline | 39 allocations / 3,264 bytes |
| Peak live descriptor pools / sets | 102 / 102 |
| After every full retirement | 53 allocations / 704,128 bytes; 0 pools / sets |
| Growth after retirement | 0 allocations / 0 bytes |
| Frame id 0 | Refused |

CMake/CTest: all 7 tests passed. The default pure build also passed all 5 tests
before Vulkan was enabled. Our targets build with `/W4 /WX`; upstream targets
retain their existing warning settings and emit existing SDK warnings.
The final GPU regression took 3.24 seconds; the complete suite took 3.45 seconds.
As negative controls, individually removing BeginFrame, RetireFrame, or the
frame-id-zero guard made the regression fail (CTest exit 8 in each case).
The fix was restored and rebuilt before the final passing suite.

This host initially lacked the Vulkan development headers and `vulkan-1.lib`.
For this run only, Khronos Vulkan-Headers tag `v1.4.341` was downloaded into
ignored `build-deps/`, and MSVC `dumpbin` plus `lib` created an import library
from the installed system loader's exports there. Configuration additionally
set `Vulkan_INCLUDE_DIR` to `build-deps/Vulkan-Headers-1.4.341/include` and
`Vulkan_LIBRARY` to `build-deps/vulkan-1.lib` (absolute paths). Nothing was
installed outside the worktree or committed from that dependency directory.

The caller audit found only upstream's API smoke fixture calling this 3.1.6
`RecordPrepare` entry point in `examples/`, `universal/`, and `tests/`; its valid
frames are already numbered 1 and 2. No existing caller needed correction.

Not verified here: lavapipe/Linux, macOS, Vulkan validation layers (not
installed), image quality, or game-resolution performance. The byte count
measures explicit device-memory allocations; descriptor driver-internal memory
is represented by the live pool/set count rather than estimated in bytes.
