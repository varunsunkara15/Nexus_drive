# Nexus Drive

**Nexus Drive** is an open-hardware motor driver we are designing for a humanoid robot's joints.

The goal is a compact board that can sit near each joint actuator and drive its motor accurately and efficiently. It should also report back to the robot's main controller.

> **Status:** 🚧 Early development. The KiCad project is set up, but the schematic and PCB layout have not been started.

---

## Repository contents

| File | Description |
| --- | --- |
| `nexus_drive.kicad_pro` | KiCad project file |
| `nexus_drive.kicad_sch` | Root schematic sheet |
| `h_bridge.kicad_sch` | One half-bridge (MOSFETs, shunt, phase output), used 3× for phases A/B/C |
| `gate_driver.kicad_sch` | DRV8353S gate driver and support parts |
| `current_sense.kicad_sch` | RC filters on the current-sense amplifier outputs |
| `stm32g474.kicad_sch` | STM32G474METx MCU, wired to the pin plan |
| `Comms Component Selection.ioc` | STM32CubeMX pin assignment for the MCU |
| `lib/` | Project-local symbol and footprint libraries (`moteus`) |
| `NOTICE.md`, `LICENSES/` | Attribution for files derived from mjbots moteus (Apache-2.0) |
| `nexus_drive.kicad_pcb` | PCB layout |
| `.gitignore` | Keeps KiCad backups, autosaves, lock files and caches out of the repo |
| [`docs/power-stage.md`](docs/power-stage.md) | Power stage design notes: gate driver, MOSFET bridge, overvoltage protection, current and voltage sensing, layout rules |

Folders for fabrication outputs (Gerbers, BOM, pick-and-place), datasheets and firmware will be added as the design matures.

## Design goals

These are the intended targets. Values still marked **TBD** will be filled in as we choose parts. The power stage is described in detail in [docs/power-stage.md](docs/power-stage.md).

- **Motor type:** Brushless DC / PMSM motors used in humanoid joint actuators
- **Control:** Field-oriented control (FOC) for smooth, quiet torque control
- **Feedback:** Rotor position sensing (e.g. a magnetic encoder) and phase current sensing
- **Communication:** A bus to the robot's main controller, such as CAN / CAN-FD
- **Protection:** Over-current, over-voltage, under-voltage and over-temperature protection
- **Form factor:** Small enough to mount on or inside the joint actuator

### Planned specifications

| Parameter | Target |
| --- | --- |
| Battery | 12S3P Li-ion / LiPo: 36–50.4 V (44–48 V nominal) |
| Continuous / peak phase current | TBD (depends on the motor) |
| Microcontroller | STM32G474METx (LQFP-80, 170 MHz Cortex-M4) |
| Gate driver | TI DRV8353S (SPI-configured, integrated current-sense amplifiers) |
| MOSFETs | 6 × onsemi FDMT80080DC (80 V, 1.35 mΩ) |
| Current sensing | 3 low-side shunts (value TBD) |
| Overvoltage protection | Input TVS, bulk capacitor, 100 V ceramic DC-link bank, anti-spark connector |
| Encoder interface | TBD |
| Communication | TBD |
| Board dimensions | TBD |

### Note: bulk capacitance on the battery bus

The power stage in this repo has **no bulk / DC-link capacitor bank yet**. It is owned by another team member and must be added before the board is built. The reference design (moteus x1) uses 36 × 4.7 µF 100 V ceramics. A **bulk electrolytic (or tantalum) capacitor** at the power input may also be needed. Reasons:

- **Hot-plug ringing:** a bank of only ceramics has very low ESR. Together with the battery cable's inductance, it can ring up to ~2× the battery voltage when the battery is plugged in (~100 V on a 50.4 V pack), above the 80 V MOSFET rating. An electrolytic's higher ESR (≈ 0.05–0.1 Ω) damps this ringing.
- **Ceramics lose capacitance under DC bias:** 100 V X7R ceramics keep only about 30–50 % of their rated value at 50 V, so the real bus capacitance is much lower than the printed value.
- **Regen and load steps:** a larger capacitor absorbs some regenerative braking energy and supplies current during fast load changes, reducing bus voltage swings.

Guidance: rated **80 V or higher** (63 V is the bare minimum), capacitance several times the effective ceramic bank (e.g. 330–470 µF total), ESR ≈ 0.05–0.1 Ω, placed between the input connector and the half-bridges. **Tantalum is not recommended at this voltage:** tantalums need ~2× voltage derating (a >100 V part is hard to find), and they tend to fail short-circuit, which is a fire risk on a stiff 12S3P pack. Aluminium electrolytic or hybrid polymer is the safer choice. See [docs/power-stage.md](docs/power-stage.md) for the full overvoltage-protection requirements.

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
- [ ] Define the specifications (battery voltage ✅, motor current, motor type)
- [ ] Choose the main parts (gate driver ✅, MOSFETs ✅, MCU ✅, shunts, encoder)
- [ ] Draw the schematic (phase power section ✅, gate driver ✅, MCU pin connections ✅)
- [ ] Lay out the PCB
- [ ] Run design review, DRC and ERC
- [ ] Order and assemble the first prototype
- [ ] Bring up the board and write the firmware (FOC loop, communication)
- [ ] Test on a humanoid joint actuator

## Contributing

This project is under active development. Issues and pull requests are welcome. Please describe the change you're proposing, and why, before sending large design changes.

## License

No license has been chosen yet. Until one is added, all rights are reserved by the authors.
