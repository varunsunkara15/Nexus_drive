# Nexus Drive

**Nexus Drive** is an open-hardware motor driver we are designing for a humanoid robot's joints.

The goal is a compact board that can sit near each joint actuator and drive its motor accurately and efficiently. It should also report back to the robot's main controller.

> **Status:** 🚧 Early development. The KiCad project is set up, but the schematic and PCB layout have not been started.

---

## Repository contents

| File | Description |
| --- | --- |
| `nexus_drive.kicad_pro` | KiCad project file |
| `nexus_drive.kicad_sch` | Schematic |
| `nexus_drive.kicad_pcb` | PCB layout |
| `.gitignore` | Keeps KiCad backups, autosaves, lock files and caches out of the repo |

Folders for fabrication outputs (Gerbers, BOM, pick-and-place), datasheets and firmware will be added as the design matures.

## Design goals

These are the intended targets. Exact values are **TBD** and will be updated as we choose parts.

- **Motor type:** Brushless DC / PMSM motors used in humanoid joint actuators
- **Control:** Field-oriented control (FOC) for smooth, quiet torque control
- **Feedback:** Rotor position sensing (e.g. a magnetic encoder) and phase current sensing
- **Communication:** A bus to the robot's main controller, such as CAN / CAN-FD
- **Protection:** Over-current, over-voltage, under-voltage and over-temperature protection
- **Form factor:** Small enough to mount on or inside the joint actuator

### Planned specifications

| Parameter | Target |
| --- | --- |
| Supply voltage | TBD |
| Continuous / peak phase current | TBD |
| Microcontroller | TBD |
| Gate driver / power stage | TBD |
| Encoder interface | TBD |
| Communication | TBD |
| Board dimensions | TBD |

## Getting started

### Requirements

- [KiCad](https://www.kicad.org/) **10.0** or newer. The project files use the KiCad 10 format and will not open in older versions.

### Opening the project

```bash
git clone https://github.com/varunsunkara15/Nexus_drive.git
cd Nexus_drive
```

Then open `nexus_drive.kicad_pro` in KiCad.

## Roadmap

- [x] Create the KiCad project and repository
- [ ] Define the specifications (voltage, current, motor type)
- [ ] Choose the main parts (MCU, gate driver, MOSFETs, current sensing, encoder)
- [ ] Draw the schematic
- [ ] Lay out the PCB
- [ ] Run design review, DRC and ERC
- [ ] Order and assemble the first prototype
- [ ] Bring up the board and write the firmware (FOC loop, communication)
- [ ] Test on a humanoid joint actuator

## Contributing

This project is under active development. Issues and pull requests are welcome. Please describe the change you're proposing, and why, before sending large design changes.

## License

No license has been chosen yet. Until one is added, all rights are reserved by the authors.
