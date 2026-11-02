# Rules for agents in this repo

Everything you need to work here is in this repo. The full pipeline and rules live in the pipeline repo (private): `PIPELINE.md`, `RULES.md`, `RUBRIC.md`, `roles/`; this is the short copy.

1. **Find the stage** in `PIPELINE.md`. The spec is `SPEC.md`, the plan `ROADMAP.md`, the code map `docs/code-map.md`.
2. **Work from an issue.** Take one that is not `in-progress` and not blocked; add `in-progress` before you start. Its body is your brief; an Easy issue has a "Qwen brief" section with the expected results and the code map snippet, and a "Tests (locked)" file you commit first and never edit.
3. **Your own platform's issues first.** Mac, Windows and Linux all matter: say which you built on.
4. **Prove it**: build it, run the issue's tests and lint, then open the PR with that output in its body (R40; the reviewer checks the expected results).
5. **Label the PR** `by:<machine>` and `model:<model>`, and write `Closes #N`.
6. **A failed job** gets a comment on the issue saying what happened, and the issue moves up one size (Easy -> Medium -> Hard).
7. **Use the project's words** from `GLOSSARY.md` and the decisions in `docs/adr/`.
8. **No private details** in code, commits or PR text: no names, e-mail addresses, home paths or machine names. Commit as the no-reply identity.

## This project

A fork of FireBurn/UniversalScalers-Vulkan (formerly FireBurn/FSR-Vulkan, MIT), a portable Vulkan library for upscaling and frame generation. We build our shared upscaler and frame-generation library for every `-universal` game on top of it: a stable C API in `universal/` that the games link, with FSR 3, FSR 4, XeSS and DLSS behind it, on Linux, Windows and Mac (MoltenVK). See `SPEC.md`.

- **Stage:** Dev Loop (since 2026-11-02). Build only what an issue asks for.
- **Our code lives in our own files**: `universal/` (the C API, its backends and tests), `.github/workflows/fork-ci.yml`, `docs/` (code map, integration guide, ADRs, licences), `SPEC.md`, `ROADMAP.md`, `GLOSSARY.md`, `PIPELINE.md`, `AGENTS.fork.md`, `CHANGELOG.fork.md`, `CLAUDE.md`.
- **Never edit upstream-owned files** (sources, `include/`, `src/`, `tests/`, `tools/`, `shaders/`, `upstream/`, `generated/`, `examples/`, `CMakeLists.txt`, `README.md`, `INTEGRATION_GUIDE.md`, `UPSTREAM.md`, `NOTICE.md`, `PUBLISHING.md`, `.github/workflows/ci.yml`), because edits conflict with every daily sync. The one standing exception is our FSR 4 profiling switches (`src/ffx_vk_fsr4_v07.c`, `upstream/ffx-1.1.4/sdk/src/backends/vk/ffx_vk.cpp`); an issue that must touch another upstream file says so and why.
- **Link upstream targets, do not copy them.** `universal/` pulls upstream's tree in with `add_subdirectory(... EXCLUDE_FROM_ALL)` and links only the targets its backends need.
- **Do not open PRs or issues upstream.** Every upstream offer needs my yes (R36).
- **Ship no vendor files.** No NVIDIA, AMD or Intel binaries, runtimes or model files are committed or packaged here. Vendor runtimes (DLSS, XeSS) are found on the player's machine at run time; FSR 4 model assets are supplied by the game.
- Licensing: vendored AMD source is MIT per file; do not import or derive from signed FSR binaries (see `UPSTREAM.md`). Licence notes for each vendor path: `docs/licences.md`.
- Do not build or install anything on a small machine.

## Model-extraction tools (owner ruling, 2026-11-01)

We keep upstream's files as they are, but we never run the model-extraction tools and we never ship their output.

- Never run `tools/extract_fsr412_vk.py`.
- Never run any other tool that extracts models or weights from a vendor's binaries or drivers.
- Never build, package, publish or commit anything produced by those tools. That includes the "recovered" XeSS and DLSS pipelines (for example `examples/xess-recovered/`, the `ffx-vulkan::xess-recovered` and `ffx-vulkan::dlss-contract` targets) and any model data derived from them. Our builds link only the targets `universal/` names, so these are never compiled.
- Keeping the upstream files in the tree is fine. Reading them is fine. Running them or releasing their output is not.
- If a job seems to need one of these tools, stop and ask me. Do not work around this rule.
