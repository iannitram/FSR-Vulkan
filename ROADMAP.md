# Roadmap to 1.0

**1.0** (`SPEC.md`): Uncharted-Universal upscales and frame-generates through this library's C API on Linux, Windows and Mac (MoltenVK), with FSR 3, FSR 4, XeSS and DLSS each working where the hardware can run it and FSR 3 everywhere else.

Our work is the delta in `SPEC.md` (D1-D8); upstream's plan is its `UPSTREAM.md`. The code map is `docs/code-map.md`; how FSR 4, XeSS and DLSS reach the GPU is `docs/adr/0001-vendor-paths.md`. Issue numbers are in the stage tables; an issue's `Blocked by` line gives the order.

## Features (what 1.0 has)

| Feature | Linux | Windows | Mac (MoltenVK) | Musts |
|---|---|---|---|---|
| Stable C API `uscale.h` + `uscale_vulkan.h`, versioned | yes | yes | yes | M1, M2 |
| Backend choice with fallback and a reason | yes | yes | yes | M3 |
| FSR 3 upscaling (upstream FSR 3.1.x) | yes | yes | yes | M4 |
| FSR 3 frame generation (upstream 3.1.6 + presenter policy) | yes | yes | yes | M5 |
| FSR 4 (upstream v07 provider, game-supplied assets) | yes | yes | if MoltenVK exposes integer dot product | M6 |
| DLSS (NVIDIA's runtime on the player's machine, RTX only) | yes | yes | no runtime: falls back | M7 |
| XeSS (Intel's runtime on the player's machine) | if Intel ships a Linux runtime | yes | no runtime: falls back | M8 |
| FSR 4 profiling switches `BB_FSR4_PROFILE`, `BB_FSR4_STATS` | yes | yes | yes | M9 |
| CI on all three, recovered pipelines never built | yes | yes | yes | M10 |
| Integration guide + example consumer | yes | yes | yes | M11 |
| Install package with no vendor files, licence notes | yes | yes | yes | M12 |

## Dev stages

Each stage's issues can run in parallel unless a `Blocked by` line says otherwise. Platform-specific issues carry a `platform:` label.

### 1. Skeleton (done, 2026-11-02)

`universal/` with both headers, one stub file per module, the CTest harness (`tests/uscale_test.h`, every `tests/*_test.c` is a test), and `fork-ci.yml` on Linux, Windows and macOS. The pure half builds everywhere; the Vulkan half builds where the Vulkan headers are found (Linux CI today).

### 2. Core API and build wiring (all platforms)

| Id | Issue | Size | Kind |
|---|---|---|---|
| 2.01 | Result and backend names | Easy | pure-fn |
| 2.02 | Render size per quality preset | Easy | pure-fn |
| 2.03 | Backend choice and fallback | Easy | pure-fn |
| 2.04 | Profiling flags from the environment | Easy | pure-fn |
| 2.05 | `USCALE_WITH_FFX`: link upstream's FSR targets, never the recovered ones | Medium | wiring |
| 2.06 | Linux CI job on a software Vulkan device (lavapipe) | Medium | wiring |

### 3. FSR 3 path (Linux first, then Windows and Mac through stage 4)

| Id | Issue | Size | Kind |
|---|---|---|---|
| 3.01 | `uscale_query_caps` from the physical device | Medium | data-shape |
| 3.02 | Context lifecycle and the backend table, with fallback | Medium | state |
| 3.03 | FSR 3 bridge: map `uscale_upscale_info` onto upstream's FSR 3 dispatch | Medium | data-shape |
| 3.04 | FSR 3 upscale backend runs on lavapipe | Hard | wiring |
| 3.05 | FSR 3 frame generation and presenter policy | Hard | state |

### 4. Platforms

| Id | Issue | Size | Kind |
|---|---|---|---|
| 4.01 | Windows: the Vulkan half and the FSR targets build with MSVC in CI | Hard | wiring |
| 4.02 | Mac: the Vulkan half and the FSR targets build on MoltenVK in CI | Hard | wiring |

### 5. Vendor paths (ADR 0001)

| Id | Issue | Size | Kind |
|---|---|---|---|
| 5.01 | Licence check: DLSS, XeSS, FSR 4 assets -> `docs/licences.md` | Medium | docs |
| 5.02 | Vendor runtime loader (`dlopen` / `LoadLibrary`) | Medium | wiring |
| 5.03 | FSR 4 backend: upstream v07 provider with the game's asset folder | Hard | wiring |
| 5.04 | XeSS: find Intel's runtime and report it in the caps | Medium | wiring |
| 5.05 | XeSS backend | Hard | wiring |
| 5.06 | DLSS: find NVIDIA's runtime and report it in the caps | Medium | wiring |
| 5.07 | DLSS backend | Hard | wiring |

### 6. Integration

| Id | Issue | Size | Kind |
|---|---|---|---|
| 6.01 | Integration guide `docs/integration.md` + `universal/examples/minimal` | Medium | docs |
| 6.02 | Install rules, `find_package(uscale)` and the no-vendor-files install check | Medium | wiring |

### 7. Device checks (`needs-owner-device`)

| Id | Issue | Size | Kind |
|---|---|---|---|
| 7.01 | AMD handheld, Linux: FSR 3, frame generation, FSR 4, profiling | Medium | wiring |
| 7.02 | RTX 3090 Ti desktop, Windows and Linux: FSR 3, frame generation, DLSS, XeSS | Medium | wiring |
| 7.03 | Mac mini, MoltenVK: FSR 3 and frame generation | Medium | wiring |

### 8. Design pass and 1.0

The library has no UI, so the design pass (R04) does not apply. 1.0 is tagged when stages 2-7 are merged and Uncharted-Universal reports it upscales and frame-generates through `uscale` on all three platforms.

## Interlinks

- **Uncharted-Universal** (first consumer, private): its spec lists FSR 4 as its issue #18 (after its 1.0). It pins upstream today as the submodule `gpu/third_party/fsr-vulkan` (pointing at upstream's old name). This repo's 6.01 and 6.02 give it what it needs: move the submodule to this fork, link `uscale::uscale` from its `gpu/` build, and call it from the renderer. That work is Uncharted-Universal's; when 3.04 and 6.01 merge, its issue #18 is unblocked.
- **shadPS4 fork** (iannitram/shadPS4): the graphics core of the PS4 `-universal` games. The renderer hands colour, depth and motion images and its command buffer to `uscale`; the hook lives in each game repo's `gpu/` copy or the shadPS4 fork, not here.
- **Other `-universal` repos** (Wipeout-Universal, Ratchet-Clank-Universal, TheLastGuardian-Universal, SotC-Universal on shadPS4; totk-universal, Mario-Odyssey-Universal, ACNH-Universal on their own Vulkan renderers): every one shares this plan and links the same `uscale` target, so a fix here reaches them all.
- **Upstream** (FireBurn/UniversalScalers-Vulkan): synced daily (R21). D1, D6 and D7 are worth offering upstream, each with my yes (R36).

## Open-source candidates (peers, `SPEC.md` Prior art)

| Project | Licence | Where it fits |
|---|---|---|
| FireBurn/UniversalScalers-Vulkan | MIT | the base: FSR 3, frame generation, presenter policy, FSR 4 v07 provider |
| GPUOpen FidelityFX-SDK | no licence file | reference for matching FSR inputs and results (ideas only) |
| intel/xess | NOASSERTION | XeSS runtime API for 5.04-5.05; licence in 5.01 |
| NVIDIA/DLSS | NOASSERTION | NGX Vulkan API for 5.06-5.07; licence in 5.01 |
| NVIDIA-RTX/Streamline | NOASSERTION | how one API fronts several upscalers (ideas for 3.02) |
| optiscaler/OptiScaler | GPL-3.0 | input mapping across upscalers (ideas for 3.03, 5.05, 5.07) |
| KhronosGroup/MoltenVK | Apache-2.0 | the Mac target (4.02, 7.03) |
| HansKristian-Work/vkd3d-proton | LGPL-2.1 | FSR 4 and FP8 handling on Vulkan (ideas for 5.03) |
| jp7677/dxvk-nvapi | MIT | how DLSS reaches Vulkan on Linux (5.06) |
| lsfg-vk, vkBasalt, beamng-fsr4-linux, gamescope | various | layer and compositor approaches: ideas only, a Won't for 1.0 |

## Mac, PC and Linux (R04)

- **Linux:** GCC and Clang; Vulkan loader from the distro; CI on GitHub's Ubuntu runner, GPU tests on lavapipe (2.06); device checks on the AMD handheld (SteamOS) and the RTX 3090 Ti desktop.
- **Windows:** MSVC; the Vulkan SDK in CI (4.01); vendor runtimes found beside the game's executable or in the driver store; device check on the RTX 3090 Ti desktop.
- **Mac:** Apple Clang; MoltenVK through the Vulkan loader (4.02); DLSS and XeSS report "no runtime" and fall back; device check on the Mac mini.
