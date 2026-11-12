# Changelog (fork)

Newest first. One line per change of stage, release or decision that the next session needs. Upstream has no CHANGELOG; this file is ours and sits beside upstream's files.

- 2026-11-12 (#10) Fill the context lifecycle: create picks the backend with uscale_select_backend, falls back to FSR 3 when its create fails, record calls route through the backend table, destroy frees the backend and frame generation; tested with fake backend ops, no GPU.
- 2026-11-12 (#16) Fill `docs/licences.md` from the NVIDIA RTX SDKs licence, Intel's XeSS licence and AMD's FidelityFX licence: a game may redistribute the DLSS and XeSS runtimes with notices, XeSS has no Linux runtime, the DLSS headers cannot be committed, and no terms were found for the v07 FSR 4 asset files; SPEC's DLSS risk line points to it.
- 2026-11-12 (#9) Fill uscale_query_caps from the physical device: vendor id, integer dot product, FP16, MoltenVK and FSR 4 asset folder; vendor runtimes stay 0; caps test on lavapipe. Stage 2 (#3-#8) marked done on the roadmap.
- 2026-11-12 (#11) Map uscale frames to the upstream FSR 3 dispatch contract without GPU calls; document jitter/UV motion conventions and verify every field in pure tests with FFX off or on.
- 2026-11-12 (#8) Require a real Vulkan device on Mesa lavapipe in Linux CI; add a device smoke test with CTest skip support elsewhere.
- 2026-11-12 (#17) Load vendor runtimes by ordered name list on Windows, Linux and macOS, report missing runtimes, resolve symbols and close handles; test with a build-made shared library on every CI platform.
- 2026-11-12 (#7) Wire USCALE_WITH_FFX to only upstream's FSR targets through EXCLUDE_FROM_ALL, share the root setup with the frame-generation regression, and build/check it in Linux CI.
- 2026-11-12 (#28) Tag and retire FSR 3.1.6 frame-generation bridge allocations and reject frame id 0, from Uncharted-Universal@7cec4936 and MIT peers mark3317/UniversalScalers-Vulkan@a0508e1f, vladtepes22/FSR-Vulkan@ad11f682; add a GPU allocation lifetime regression.
- 2026-11-11 Roadmap map: the roadmap gained a features block (13 features, from its feature table, stage 7 and the upstream sync); every open issue gets one feature label and a milestone, 1.0 or After 1.0 (my ruling: "Yes, board order").
- 2026-11-02 My ruling: FSR 4 uses AMD's official files from the game's folder or AMD's download, never from our repo; D5 and M6 stand (approval line).
- 2026-11-02 My rulings: using this library is optional for the -universal games (D2 softened); DLSS, XeSS and FSR 4 run only through the vendor's own runtime on the player's machine, nothing recovered or extracted in our repo (approval line, M7, the DLSS licence note; #16, #18, #20, #22).
- 2026-11-02 Roadmap -> Dev Loop: 25 issues filed (#3-#27) from ROADMAP.md stages 2-7; the 4 Easy ones (#3-#6) carry a Qwen brief and locked tests, checked red on the stubs and green on a reference with GCC, Clang and MSVC. #1 stays open, blocked by #7.
- 2026-11-02 Parked -> Spec -> Roadmap: spec approved (shared upscaler and frame-generation library for the -universal games; 1.0 = Uncharted-Universal through it on Linux, Windows and Mac; DLSS kept fully; 16 peers). Kind: mirror -> fork built our way. ADR 0001: vendor paths through vendor runtimes, recovered pipelines never built.
- 2026-11-01 Synced to FireBurn/UniversalScalers-Vulkan 69fecd7 (fork main merged in, nothing dropped); model-extraction tools rule added to AGENTS.fork.md (owner ruling).
- 2026-10-26 Normalized to pipeline v2 at Parked (reference mirror of FireBurn/FSR-Vulkan).
- 2026-10-20 Experimental FSR 4 profiling switches (BB_FSR4_PROFILE, BB_FSR4_STATS) merged into this fork's main.
