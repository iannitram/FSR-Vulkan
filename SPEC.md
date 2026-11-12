# Spec

**1.0 means:** Uncharted-Universal upscales and frame-generates through this library's C API on Linux, Windows and Mac (Mac through MoltenVK), with FSR 3, FSR 4, XeSS and DLSS each working on the hardware that can run it and falling back to FSR 3 everywhere else.

**Approved by me:** 2026-11-02, in the Project Manager session's question tool. Rulings, verbatim:

- "Shared upscaler for games (Recommended)": this fork becomes our shared upscaler and frame-generation library that every `-universal` game plugs into (every `-universal` repo shares one plan), on Mac, PC and Linux. We build on upstream and keep our profiling switches. The kind changes from mirror to a fork built our way; the daily upstream sync continues (R21).
- "Uncharted-Universal (Recommended)": 1.0 is Uncharted-Universal upscaling and frame-generating through this library on Linux, Windows and Mac (Mac through MoltenVK). The integration inside Uncharted-Universal is that repo's job; this repo provides what it needs (a stable C API and an integration guide).
- "Keep DLSS fully": DLSS is a full part of 1.0 alongside FSR 3, FSR 4 and XeSS.
- "Approve all 16 (Recommended)": the prior-art list below.
- Later the same day, "Make it optional (Recommended)" ("I think each will become custom over time. This doesn't need to be a rule."): a `-universal` game may plug into this library, and that is preferred, but no game must; a game may keep and customise its own FSR copy as long as FSR 3.1 works there. 1.0 stays Uncharted-Universal through this library.
- Same day, DLSS: "Official runtimes only (Recommended)". DLSS, XeSS and FSR 4 run only through the vendor's own runtime already on the player's machine. No recovered or extracted code or assets go in our repo.
- Same day, FSR 4 assets: "Yes, that's fine (Recommended)". FSR 4 uses AMD's official files, which come from the game's folder or AMD's download, never from our repo; D5 and M6 stand.

The last three are my own rulings, so the spec stays approved.

Standing ruling this spec keeps: the model-extraction rule of 2026-11-01 (`AGENTS.fork.md`): we never run upstream's extraction tools and never build or ship their output, the "recovered" XeSS and DLSS pipelines included. So in this spec FSR 4, XeSS and DLSS reach the GPU through each vendor's own runtime or assets, found or supplied on the player's machine, never through files we extracted or ship (`docs/adr/0001-vendor-paths.md`).

A project is Spec'd only when the line above has a date and a link. Change a Must or a Won't after approval and the approval line goes back to "not yet" until I approve again.

## Upstream's roadmap and our delta (R18)

Upstream: [FireBurn/UniversalScalers-Vulkan](https://github.com/FireBurn/UniversalScalers-Vulkan). It has no ROADMAP file; its plan and status are [`UPSTREAM.md`](https://github.com/FireBurn/UniversalScalers-Vulkan/blob/main/UPSTREAM.md) ("Upstream inventory, provenance, and implementation plan", "Verification gates") and the feature matrix in its [`README.md`](https://github.com/FireBurn/UniversalScalers-Vulkan/blob/main/README.md). We take its FSR 3.1.x upscaler, FSR 3.1.6 frame generation, the frame-generation presenter policy, the FSR 4 v07 provider and its capability probe as they are, synced daily.

Our delta, what I want that upstream will not do or has not done:

| # | Delta | Where |
|---|---|---|
| D1 | FSR 4 profiling switches `BB_FSR4_PROFILE` (GPU time per pass) and `BB_FSR4_STATS` (pipeline statistics), kept through every sync | send upstream (an offer needs my yes, R36) |
| D2 | One stable, versioned C API (`universal/include/uscale.h`, `uscale_vulkan.h`) that any `-universal` game can link (optional, preferred), with the same calls on every backend and platform | keep on our fork |
| D3 | Backend choice with a written fallback: the API picks DLSS, FSR 4, XeSS or FSR 3 from the device and the runtimes it finds, and says why | keep on our fork |
| D4 | DLSS through NVIDIA's own DLSS runtime and XeSS through Intel's own XeSS runtime, loaded at run time, never shipped by us; upstream's recovered pipelines are not built | keep on our fork |
| D5 | FSR 4 through upstream's v07 provider with an asset set the game supplies (Uncharted-Universal's fetch, its issue #18), never the extracted 4.1.2 set | keep on our fork |
| D6 | Windows build and CI (MSVC) | send upstream (an offer needs my yes, R36) |
| D7 | Mac build and CI on MoltenVK | send upstream (an offer needs my yes, R36) |
| D8 | Integration guide and example for the `-universal` games (`docs/integration.md`) | keep on our fork |

Our issues and code map cover only this delta.

## Musts

Each Must is something a person can do and see. Each one gets its own issue, and each issue names the check that proves it. A check marked **needs-owner-device** runs on a real machine with that GPU or OS, not in CI.

| # | Must | Done looks like | Check |
|---|---|---|---|
| M1 | WHEN a game includes `uscale.h` and `uscale_vulkan.h` and links `uscale`, IT SHALL create a context, query the render size, record an upscale and destroy the context with the same calls on every backend | the example in `universal/examples/minimal` builds and runs | CI: `uscale_api_test` and the example build on Linux, Windows and macOS |
| M2 | WHEN the API reports its version, IT SHALL give a major.minor.patch that changes major only on an ABI break | `uscale_version()` matches the header macros | CI: `uscale_version_test` |
| M3 | WHEN a game asks for an upscaler, THE LIBRARY SHALL pick one from the device (AUTO: DLSS, then FSR 4, then XeSS, then FSR 3) or honour the one asked for, and SHALL fall back to FSR 3 with a reason string when the one asked for cannot run | the reason string names the backend and why | CI: `uscale_select_test` |
| M4 | WHEN FSR 3 is chosen, THE LIBRARY SHALL upscale through upstream's FSR 3 upscaler on Linux, Windows and Mac | upscaled frames, no Vulkan validation errors | CI: dispatch test on a software Vulkan device (lavapipe) on Linux; **needs-owner-device**: the AMD handheld (Linux), the RTX 3090 Ti desktop (Windows), the Mac mini (MoltenVK) |
| M5 | WHEN frame generation is on, THE LIBRARY SHALL generate frames through upstream's FSR 3.1.6 frame generation and its presenter policy, with the game owning the swapchain | generated-then-real frames presented in order at 2x | CI: frame-generation dispatch test on lavapipe; **needs-owner-device**: the same three machines |
| M6 | WHEN the game supplies an FSR 4 asset folder and the GPU has integer dot product and FP16, THE LIBRARY SHALL upscale through FSR 4 (upstream's v07 provider); with no assets or no support it SHALL fall back to FSR 3 | FSR 4 named as the active backend | CI: asset-check and fallback tests; **needs-owner-device**: the AMD handheld |
| M7 | WHEN NVIDIA's own DLSS runtime is already on the player's machine and the GPU is an NVIDIA RTX, THE LIBRARY SHALL upscale through that runtime only; elsewhere it SHALL fall back with a reason. No recovered or extracted DLSS code or assets go in our repo (my ruling, 2026-11-02: "Official runtimes only") | DLSS named as the active backend | CI: loader and fallback tests (no runtime present); **needs-owner-device**: the RTX 3090 Ti desktop, Windows and Linux |
| M8 | WHEN Intel's XeSS runtime is present, THE LIBRARY SHALL upscale through XeSS; elsewhere it SHALL fall back with a reason | XeSS named as the active backend | CI: loader and fallback tests; **needs-owner-device**: the RTX 3090 Ti desktop (XeSS's DP4a path), Windows |
| M9 | WHEN `BB_FSR4_PROFILE` or `BB_FSR4_STATS` is set, THE LIBRARY SHALL keep upstream's FSR 4 profiling switches working through every sync | per-pass GPU times or pipeline statistics printed | CI: `uscale_profile_env_test`; **needs-owner-device**: the AMD handheld |
| M10 | WHEN the library builds, IT SHALL build and pass its tests on Linux, Windows and macOS in CI, without compiling upstream's recovered XeSS or DLSS targets | three green CI jobs | CI: `fork-ci.yml`, plus a check that `xess-recovered` and `dlss-contract` are not in the build |
| M11 | WHEN a `-universal` game's developer reads `docs/integration.md`, THEY SHALL be able to add the library with CMake (`add_subdirectory` or `FetchContent` of this repo) and call it without reading upstream's code | a fresh CMake project links `uscale` from the guide alone | CI: the example consumer build |
| M12 | WHEN the library is packaged, IT SHALL contain no NVIDIA, AMD or Intel binaries or model files, and `docs/licences.md` SHALL say what each vendor path lets us ship | the licence file and a check over the install tree | CI: `uscale_install_check` |

## Won'ts

Things 1.0 will not do, so nobody builds them by accident.

- Run upstream's model-extraction tools, or build or ship their output, the recovered XeSS and DLSS pipelines (`xess-recovered`, `dlss-contract`, the d4r shaders) and the extracted FSR 4.1.2 set included (ruling of 2026-11-01).
- Ship any vendor runtime or model file (DLSS, XeSS, FSR 4 assets) in this repo or its packages.
- Own the swapchain, present or pace frames for the game: the game keeps its window, queues and present; the library records work into the game's command buffers.
- DirectX, or any path that needs Wine or Proton.
- The game-side integration: hooking the library into Uncharted-Universal's `gpu/` (shadPS4) is that repo's job (its issue #18).
- Android and iOS (1.0 is Linux, Windows and Mac).
- A Vulkan layer that injects upscaling into games that do not call us (lsfg-vk, vkBasalt style): ideas only.
- Upstream's other contracts (ray regeneration, radiance cache, ML frame generation): kept in the tree, not part of our API.
- PRs or issues upstream without my yes (R36).

## Risks to check

- **DLSS licence (answered in `docs/licences.md`, 5.01).** I chose to keep DLSS fully. The check says which files a game may redistribute (the `nvngx_dlss` runtime), what notice it requires, and whether an MIT library may load it from the player's machine. My ruling of 2026-11-02, "Official runtimes only (Recommended)", settles how DLSS reaches the player: the DLSS backend loads only NVIDIA's runtime already on the player's machine, and we ship nothing of NVIDIA's, whatever the licence check finds. The check says what notice that needs.
- **XeSS licence.** Intel's XeSS SDK is under its own licence (NOASSERTION on GitHub); the same check applies, and whether its Vulkan path runs on Linux.
- **FSR 4 assets.** AMD's FSR 4 source and models carry no licence file; the game fetches them (Uncharted-Universal #18) and we ship none.
- **Mac.** DLSS and XeSS have no Mac runtime, so on Mac 1.0 means FSR 3 upscaling and frame generation (and FSR 4 if MoltenVK exposes integer dot product). Uncharted-Universal's spec draws a Metal renderer on Apple (its ADR 0006); on Mac the game must reach this library through MoltenVK, a cross-repo point for that repo to settle.

## Prior art

Projects doing the same thing, approved by me at Spec and reviewed weekly. Their code comes in only when their license allows it, with credit; otherwise only their ideas. This library is MIT, so GPL and no-licence peers are ideas only.

peer: FireBurn/UniversalScalers-Vulkan | MIT | our upstream (formerly FireBurn/FSR-Vulkan); daily sync, credit for everything we take
peer: GPUOpen-LibrariesAndSDKs/FidelityFX-SDK | no licence file | AMD's FSR 3/4 source and ffx-api: the reference our FSR paths must match (ideas only)
peer: optiscaler/OptiScaler | GPL-3.0 | the most mature DLSS/XeSS/FSR/frame-gen bridge; input mapping and upscaler swap (ideas only)
peer: intel/xess | NOASSERTION | Intel's XeSS SDK: Vulkan path and redistribution terms
peer: NVIDIA-RTX/Streamline | NOASSERTION | one API in front of several upscalers; frame pacing and swapchain hooks
peer: NVIDIA/DLSS | NOASSERTION | DLSS SDK Vulkan integration and what its licence lets us ship
peer: PancakeTAS/lsfg-vk | no licence file | Lossless Scaling frame gen as a Vulkan layer on Linux, no game help needed (ideas only)
peer: zgauthier2000/fsr4-shader-performance-overrides | GPL-2.0 | FSR 4.1.2 sped up on RDNA2/3 by shader overrides (ideas only)
peer: HansKristian-Work/vkd3d-proton | LGPL-2.1 | how Proton exposes AMD's FSR 4 extension on Vulkan, incl. FP8 handling
peer: KhronosGroup/MoltenVK | Apache-2.0 | our Mac target: which Vulkan features the upscalers need that Metal translation lacks
peer: Nukem9/dlssg-to-fsr3 | GPL-3.0 | DLSS frame-gen calls mapped to FSR 3 frame gen (idle since 2025-03; ideas only)
peer: artur-graniszewski/DLSS-Enabler | MIT | DLSS on non-NVIDIA cards
peer: jp7677/dxvk-nvapi | MIT | NVAPI on Linux; how DLSS reaches Vulkan through Proton
peer: ValveSoftware/gamescope | NOASSERTION | compositor upscaling (FSR 1, NIS) on Steam Deck-class hardware
peer: Mob1K7105/beamng-fsr4-linux | GPL-2.0 | Vulkan layer swapping a native Linux game's FSR 3 for FSR 4; close to our use case (ideas only)
peer: DadSchoorse/vkBasalt | Zlib | minimal Vulkan post-processing layer: swapchain-hook template (idle since 2023)

## Problem

Every `-universal` game port needs upscaling and frame generation, and each one was set to wire FSR in by itself inside its own renderer (Uncharted-Universal pins upstream as a submodule under `gpu/`). That means the same work, the same bugs and the same licence questions in every repo, and no DLSS or XeSS anywhere.

## Solution

One library, this fork, that every game links through one small C API. The game hands over its colour, depth and motion images and its command buffer; the library picks the best upscaler the player's machine can run, upscales, and optionally generates frames, on Linux, Windows and Mac. Upstream keeps doing the shader work; we keep the API stable and the vendor paths clean.

## User stories

1. As a `-universal` game developer, I want one CMake target and one header, so that I add upscaling and frame generation once and every backend comes with it.
2. As a player on an AMD handheld, I want FSR 4 when my game has its assets and FSR 3 otherwise, so that the game looks as good as the device allows.
3. As a player on an NVIDIA RTX PC, I want DLSS when it is installed, so that I get NVIDIA's own upscaler.
4. As a player on a Mac, I want FSR 3 upscaling and frame generation through MoltenVK, so that the port runs smoothly there too.
5. As a game developer, I want to know why a backend was not chosen, so that I can tell the player.

## Implementation decisions

- The API is C (C11), prefix `uscale_`, in two headers: `uscale.h` (no Vulkan types: versions, results, backends, quality, extents, device capabilities, backend choice, profiling flags) and `uscale_vulkan.h` (context, capability query, dispatch, frame generation, taking the game's Vulkan handles). Structs carry `struct_size` so they can grow without breaking the ABI.
- The library records into the game's command buffer and never submits or presents.
- Backends are modules behind one internal table: FSR 3 (upstream FSR 3.1.x upscaler), FSR 4 (upstream v07 provider plus game-supplied assets), XeSS (Intel runtime loaded with `dlopen` / `LoadLibrary`), DLSS (NVIDIA runtime loaded the same way). Frame generation is upstream FSR 3.1.6 plus its presenter policy.
- `universal/CMakeLists.txt` builds standalone; with `USCALE_WITH_FFX=ON` it adds the repo root with `EXCLUDE_FROM_ALL` and links only the FSR 3, frame-generation, presenter and FSR 4 v07 targets, so upstream's recovered targets are never compiled.
- Platforms: Linux (GCC, Clang), Windows (MSVC), macOS (Apple Clang, MoltenVK through the Vulkan loader).

## Testing decisions

- Pure parts (names, render size, backend choice, profiling flags, version) are tested with plain C tests registered in CTest; they need no GPU and run on all three CI jobs.
- GPU paths are tested on Linux CI against Mesa's software Vulkan device (lavapipe) for "records and runs without validation errors", and on real machines through `needs-owner-device` issues for picture and speed.
- Upstream's own tests stay upstream's; our CI does not build the targets the extraction rule excludes.

## Further notes

- The game repos pin this library; Uncharted-Universal's submodule still points at upstream's old name. Moving it to this fork and calling `uscale` is Uncharted-Universal's work, unblocked by this repo's 1.0 (its issue #18 for FSR 4).
- The library has no UI, so the design pass (R04) does not apply.
