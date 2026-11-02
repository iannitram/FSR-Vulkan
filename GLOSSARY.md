# Glossary

Our words for this library (ours, not upstream's).

- **uscale**: our library and its C API (`universal/include/uscale.h`, `uscale_vulkan.h`); the CMake target `uscale::uscale`.
- **Backend**: one way to upscale: FSR 3, FSR 4, XeSS or DLSS (`uscale_backend`). AUTO means "pick the best available".
- **Fallback**: running FSR 3 instead of the backend asked for, because that one cannot run here; always comes with a **reason** string.
- **Caps** (device capabilities): what the GPU and the player's machine offer (`uscale_device_caps`).
- **Render size / display size**: the size the game renders at, and the size of the upscaled output (`uscale_extent`).
- **Quality preset**: the ratio between display and render size (`uscale_quality`).
- **Frame generation**: making an extra frame between two real ones (upstream FSR 3.1.6).
- **Vendor runtime**: NVIDIA's DLSS or Intel's XeSS library, found on the player's machine and loaded at run time; we never ship it.
- **FSR 4 assets**: AMD's FSR 4 model files that the game supplies in a folder; we never ship them.
- **Recovered pipelines**: upstream's XeSS and DLSS paths built from extracted models; never built or shipped here (ruling 2026-11-01).
- **Upstream**: FireBurn/UniversalScalers-Vulkan, synced daily.
- **Profiling switches**: `BB_FSR4_PROFILE` and `BB_FSR4_STATS`, our FSR 4 timing and statistics options.
