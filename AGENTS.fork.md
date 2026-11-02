# Rules for agents in this repo

Everything you need to work here is in this repo. The full pipeline rules (PIPELINE.md, RULES.md, RUBRIC.md, roles) live in the pipeline rules repo; this is the short copy.

1. **Find the stage** in `PIPELINE.md`. The spec is `SPEC.md`, the plan `ROADMAP.md`, the code map `docs/code-map.md`.
2. **Work from an issue.** Take one that is not `in-progress` and not blocked; add `in-progress` before you start. Its body is your brief; an Easy issue has a "Qwen brief" section with the expected results and the code map snippet.
3. **Your own platform's issues first.** Mac, Windows and Linux all matter: say which you built on.
4. **Check that it builds**, then open the PR. CI and a review by another satellite with another model do the rest.
5. **Label the PR** `by:<machine>` and `model:<model>`, and write `Closes #N`.
6. **A failed job** gets a comment on the issue saying what happened, and the issue moves up one size (Easy -> Medium -> Hard).
7. **Use the project's words** from `GLOSSARY.md` and the decisions in `docs/adr/`.
8. **No private details** in code, commits or PR text: no names, e-mail addresses, home paths or machine names. Commit as the no-reply identity.

## This project

Reference mirror of FireBurn/FSR-Vulkan, a portable Vulkan implementation of AMD FidelityFX / FSR upscaling and frame generation (see `README.md` and `UPSTREAM.md`). Kept current by the daily fork sync.

- **Parked.** Do not add features. Read the code for reference only.
- **Never edit upstream-owned files**: everything that upstream ships (sources, `README.md`, `UPSTREAM.md`, `NOTICE.md`, `PUBLISHING.md`, CMake, tests, `upstream/`). Edits conflict with every daily sync. Ours are only `PIPELINE.md`, `AGENTS.fork.md`, `CHANGELOG.fork.md`, `CLAUDE.md` and `docs/code-map.md`.
- **Do not open PRs or issues upstream.**
- Licensing: vendored AMD source is MIT per file; do not import or derive from signed FSR binaries (see `UPSTREAM.md`).
- Do not build or install anything on a small machine.

## Model-extraction tools (owner ruling, 2026-11-01)

We mirror upstream as it is, but we never run the model-extraction tools and we never ship their output.

- Never run `tools/extract_fsr412_vk.py`.
- Never run any other tool that extracts models or weights from a vendor's binaries or drivers.
- Never build, package, publish or commit anything produced by those tools. That includes the "recovered" XeSS and DLSS pipelines (for example `examples/xess-recovered/`) and any model data derived from them.
- Keeping the upstream files in the mirror is fine. Reading them is fine. Running them or releasing their output is not.
- If a job seems to need one of these tools, stop and ask the owner. Do not work around this rule.

Tracked in iannitram/FSR-Vulkan issue #1.
