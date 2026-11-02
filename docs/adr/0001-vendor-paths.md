# 0001 How FSR 4, XeSS and DLSS reach the GPU

**Date:** 2026-11-02. **Status:** accepted with the spec.

## Context

Upstream ships FSR 4.1.2, XeSS 2.0.2 and DLSS ("d4r") paths built from models extracted or recovered from vendor binaries, and the tools that extract them. My ruling of 2026-11-01 forbids running those tools or building or shipping their output, the recovered XeSS and DLSS pipelines included. My rulings of 2026-11-02 keep FSR 4, XeSS and DLSS fully in 1.0.

## Decision

- **FSR 3** (upscale and frame generation): upstream's FSR 3.1.x sources, built from AMD's MIT source in the tree.
- **FSR 4:** upstream's v07 provider (`ffx-vulkan::fsr4-v07-vulkan`), fed an asset folder the game supplies (Uncharted-Universal fetches AMD's files on the player's machine, its issue #18). We ship no assets and never use the extracted 4.1.2 set.
- **XeSS:** Intel's own XeSS runtime, found on the player's machine and loaded at run time. We ship none of it.
- **DLSS:** NVIDIA's own DLSS runtime (`nvngx_dlss`), found on the player's machine and loaded at run time, on NVIDIA RTX GPUs. We ship none of it until `docs/licences.md` records what NVIDIA's licence allows.
- Our CMake links only the upstream targets above; `xess-recovered`, `dlss-contract` and the d4r shaders are never compiled.

## Consequences

- Every vendor path has a fallback to FSR 3 with a reason string, because a runtime or asset set may be missing.
- On Mac only FSR 3 (and FSR 4 if MoltenVK exposes integer dot product) can run: NVIDIA and Intel ship no Mac runtime.
- Upstream's own unified API (`ffx_vk_unified_sr.h`) routes to the recovered paths, so we do not wrap it; our API is `universal/include/uscale.h`.
