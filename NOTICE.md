# Third-party notices

## mjbots moteus

Parts of this design are derived from the **moteus-x1** motor controller by mjbots Robotic Systems:
https://github.com/mjbots/moteus (hardware in `hw/x1`, commit `8f747f4`).

moteus is licensed under the Apache License, Version 2.0. A copy is in [LICENSES/moteus-Apache-2.0.txt](LICENSES/moteus-Apache-2.0.txt).

| File in this repo | Source in moteus | Changes |
| --- | --- | --- |
| `h_bridge.kicad_sch` | `hw/x1/h_bridge.kicad_sch` | Retargeted sheet instances to this project, changed title block |
| `bulk_capacitance.kicad_sch` | `hw/x1/bulk_capacitance.kicad_sch` | Retargeted sheet instances to this project, changed title block |
| `lib/moteus.pretty/*.kicad_mod` | `hw/x1/moteus.pretty/` | Unmodified copies of `NetTie-2_SMD_Pad0.2`, `POWER_M2.5`, `SOD-523_STM` |
| `lib/moteus.kicad_sym` | `BAT41KFILM` symbol from `hw/x1/h_bridge.kicad_sch` | Extracted into a standalone library |
| `nexus_drive.kicad_pro` | Net classes `Default`, `Gate`, `Current Sense`, `HV` from `hw/x1/moteus_x1.kicad_pro` | Net-class patterns rewritten for this project's net names |
