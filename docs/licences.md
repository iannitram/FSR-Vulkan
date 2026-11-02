# Licences for each vendor path

Status: **stub**, filled by roadmap item 5.01. Until it is filled, we ship nothing of NVIDIA's, Intel's or AMD's beyond the MIT sources upstream vendors.

| Path | Licence | What we may ship | What the game or player supplies | Notice needed |
|---|---|---|---|---|
| Our code (`universal/`) | MIT | everything | - | `LICENSE.txt` |
| FSR 3 (upstream FSR 3.1.x, vendored AMD sources) | MIT per file | built into `uscale` | - | upstream's `NOTICE.md` |
| FSR 4 (v07 provider + assets) | provider MIT; AMD assets: to check | the provider code | the asset folder | to check |
| XeSS (Intel runtime) | to check (intel/xess) | nothing until checked | the runtime | to check |
| DLSS (NVIDIA runtime) | to check (NVIDIA/DLSS SDK licence) | nothing until checked | the runtime | to check |

Questions 5.01 answers: may a game redistribute `nvngx_dlss` and Intel's XeSS library beside its executable; may an MIT library load them at run time; what attribution each requires; whether XeSS has a Linux Vulkan runtime; what terms cover AMD's FSR 4 asset files.
