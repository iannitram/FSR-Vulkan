# Fork rules for agents

This repository mirrors FireBurn/UniversalScalers-Vulkan (renamed from FireBurn/FSR-Vulkan). These rules are for our fork only.

## Model-extraction tools (owner ruling, 2026-11-01)

We mirror upstream as it is, but we never run the model-extraction tools and we never ship their output.

- Never run `tools/extract_fsr412_vk.py`.
- Never run any other tool that extracts models or weights from a vendor's binaries or drivers.
- Never build, package, publish or commit anything produced by those tools. That includes the "recovered" XeSS and DLSS pipelines (for example `examples/xess-recovered/`) and any model data derived from them.
- Keeping the upstream files in the mirror is fine. Reading them is fine. Running them or releasing their output is not.
- If a job seems to need one of these tools, stop and ask the owner. Do not work around this rule.

Tracked in iannitram/FSR-Vulkan issue #1.
