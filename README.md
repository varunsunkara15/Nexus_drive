# Nexus Drive

**Nexus Drive** is an open-hardware motor driver we are designing for a humanoid robot's joints.

The goal is a compact board that can sit near each joint actuator and drive its motor accurately and efficiently. It should also report back to the robot's main controller.

> **Status:** 🚧 Early development. Schematic in progress (power stage, gate driver, MCU, CAN-FD, encoders and sensing drawn; ERC has 0 errors; logic supplies, DC-link and input protection still to add); PCB layout not started.

---

## Repository layout

```
Nexus_drive/
├── hardware/                     KiCad 10 project (open nexus_drive.kicad_pro)
│   ├── nexus_drive.kicad_pro     Project file
│   ├── nexus_drive.kicad_sch     Root schematic sheet
│   ├── gate_driver.kicad_sch     DRV8353S gate driver and support parts
│   ├── h_bridge.kicad_sch        One half-bridge (MOSFETs, shunt, phase output), used 3× for phases A/B/C
│   ├── current_sense.kicad_sch   RC filters on the current-sense amplifier outputs
│   ├── stm32g474.kicad_sch       STM32G474METx MCU, wired to the pin plan
│   ├── can_fd.kicad_sch          TCAN1042GV CAN-FD transceiver and bus connectors
│   ├── encoders.kicad_sch        On-board MA702GQ encoder and external SPI encoder connectors
│   ├── sensing.kicad_sch         Bus and phase voltage dividers, FET and motor temperature inputs
│   ├── nexus_drive.kicad_sym     Project symbols (TCAN1042GV, MA702GQ, EMI filter)
│   ├── nexus_drive.kicad_pcb     PCB layout (not started)
│   ├── fp-lib-table, sym-lib-table   Project library tables
│   └── lib/                      Project-local symbol and footprint libraries (moteus)
├── firmware/
│   └── Comms Component Selection.ioc   STM32CubeMX pin assignment for the MCU
├── docs/
│   └── power-stage.md            Power stage design notes
├── LICENSES/, NOTICE.md          Attribution for files derived from mjbots moteus (Apache-2.0)
└── README.md
```

Folders for fabrication outputs (Gerbers, BOM, pick-and-place) and datasheets will be added as the design matures.

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
| Encoder interface | 3 × SPI: on-board MA702GQ magnetic encoder, 2 external connectors |
| Communication | CAN-FD (TCAN1042GV), two daisy-chain connectors |
| Board dimensions | TBD |

### Note: bulk capacitance on the battery bus

The power stage in this repo has **no bulk / DC-link capacitor bank yet**. It is owned by another team member and must be added before the board is built. The reference design (moteus x1) uses 36 × 4.7 µF 100 V ceramics. A **bulk electrolytic (or tantalum) capacitor** at the power input may also be needed. Reasons:

- **Hot-plug ringing:** a bank of only ceramics has very low ESR. Together with the battery cable's inductance, it can ring up to ~2× the battery voltage when the battery is plugged in (~100 V on a 50.4 V pack), above the 80 V MOSFET rating. An electrolytic's higher ESR (≈ 0.05–0.1 Ω) damps this ringing.
- **Ceramics lose capacitance under DC bias:** 100 V X7R ceramics keep only about 30–50 % of their rated value at 50 V, so the real bus capacitance is much lower than the printed value.
- **Regen and load steps:** a larger capacitor absorbs some regenerative braking energy and supplies current during fast load changes, reducing bus voltage swings.

Guidance: rated **80 V or higher** (63 V is the bare minimum), capacitance several times the effective ceramic bank (e.g. 330–470 µF total), ESR ≈ 0.05–0.1 Ω, placed between the input connector and the half-bridges. **Tantalum is not recommended at this voltage:** tantalums need ~2× voltage derating (a >100 V part is hard to find), and they tend to fail short-circuit, which is a fire risk on a stiff 12S3P pack. Aluminium electrolytic or hybrid polymer is the safer choice. See [docs/power-stage.md](docs/power-stage.md) for the full overvoltage-protection requirements.

## Schematic review (27 Nov 2026)

This review covers the schematic at commit `345d5f4`. It was checked against the KiCad netlist, ERC, DRC with schematic parity, the pin plan in `firmware/STM32G474 Pinout.ioc`, and [hardware/comms-mcu-review.md](hardware/comms-mcu-review.md). Item numbers continue from the earlier reviews.

**Design intent: the board is deliberately over-specified.** The first target motors are the CubeMars **RO80 KV105** (≈15 A rated, ~50 A peak) and **RO100 KV55** (≈20 A rated, ~62 A peak) at 48 V. The power stage keeps moteus x1's 120 A-class parts so that larger motors can be used later.

**Checks:**
- **ERC:** 0 errors, plus 9 library-symbol warnings.
- **DRC:** 0 violations, 0 schematic-parity issues and 115 unrouted connections.
- **Pin plan:** the new pins in the `.ioc` match the schematic: status LED on PB6/PB7/PB9 and HC-05 STATE on PB11.

### ✅ Closed since 23 Nov
| Item | Resolution (verified against the netlist) |
|---|---|
| N3 | FL1 is ACT1210D-101-2P. TDK's drawing shows windings 1–4 and 2–3. The netlist runs U2 CANH → pin 1 → pin 4 → bus CANH, and CANL on 2 → 3. |
| N24 | **The earlier review was wrong.** The TI datasheet for the 40-pin package says the DVDD capacitor goes "between the DVDD and GND pins", and pin 39 is "Device power ground". C20 on `GNDPWR` (pin 39) is correct. The same table confirms C23 (VGLS → GND) and C19 + C38 (VM: 0.1 µF + ≥10 µF). |
| D7 | Part chosen: Nexperia PESD5V0S1BA. |
| N7 | The motor thermistor moved to J5 pins 13–14 (via R39 and D7). The firmware conversion is in `comms-mcu-review.md`. |
| N8 | J1 is a JST GH 5-pin, including the HC-05 STATE line to PB11. The cable mapping is documented. |
| N15 | The U6 pad geometry was checked against the MPS drawing. Confirm the stencil with the fab. |
| N17 | The SWD pinout is mapped to ST-LINK/V2 and STLINK-V3MINIE. The crystal (10 pF load) with 12 pF caps assumes about 4 pF of stray capacitance; check it on the first board. |
| N18 | Keep the 120 Ω and JP2. Close JP2 only at a bus end. |
| — | **New status LEDs:** D8 (3.3 V power) and D9 (RGB, MCU-driven). The firmware helper `NexusStatusLed_Update` writes the correct levels: low turns a colour on, high turns it off. |

### 🔴 Must add before the PCB can work
These are unchanged from 23 Nov, and routing has continued around them.
- [ ] **N20: no power input.** There's no connector or pads for `V+` and `GNDPWR`.
- [ ] **N21: no supply makes the logic rails.** `+12V` (DRV8353 VM), `+5V` (TCAN1042, HC-05) and `+3V`/`+3VA` have no source. See the 23 Nov power-tree table for a starting point: a buck from `V+` rated for ≥ 80 V input, then 5 V, then 3.3 V. Keep the board's idle draw under about 2 W so all 12 drivers fit the PDB precharge.
- [ ] **N22: DC-link capacitance is still only 3 × 0.1 µF.** Add the ceramic bank at the half-bridges and a bulk capacitor at the input, rated 80 V or more.
- [ ] **N23: no input TVS,** with 80 V MOSFETs on a 50.4 V bus. Regen must also be limited in firmware.
- [ ] **N4: stop routing near the power stage** until N20–N23 are in the schematic. Tracks routed there now are likely to move.

### 🟠 Should decide
- [ ] **N25: CAN now has a single 3-pin connector (J3).** Each driver hangs off the bus on a harness branch, and CAN-FD's fast data phase is sensitive to branch length. Either keep each branch to a few centimetres and agree the CAN-FD data rate with the harness owner, or, if the mechanics allow, use a 5-pin header with CANH/CANL in and out so the chain passes through the board.
- [ ] **N26: FL1 value.** The 100 µH ACT1210D-101 was chosen. TDK also makes a 51 µH version of the same choke. Check which one TDK specifies for your planned CAN-FD data rate.
- [ ] **N9, N10, N11, N13 are carried over:** current-sense gain per motor, gate-drive budget against PWM frequency, offset calibration, and thermal design for the over-spec rating. Plans are in `comms-mcu-review.md`, but they still need hardware confirmation.

### 🟡 Housekeeping
- [ ] **N27: voltage ratings missing from the values.** C23 (VGLS) needs **≥ 16 V** and C20 (DVDD) **≥ 6.3 V**. Add them to the values so the BOM is right.
- [ ] **N28: D9 green and blue may be dim and uneven** on the 3.3 V rail, because their forward voltage is close to 3 V. Check on the first board, and lower R42/R43 if needed.
- [ ] **N12 (rest):** a hard short on an encoder cable may still reset the MCU before F1/F2 trip. Confirm on hardware.
- [ ] **N19:** commit the literature review to `docs/`.
- [ ] The status line at the top of this README still says "PCB layout not started".

**Most important:** N20–N23. The board can't be powered until the input, logic supplies, DC-link capacitors and input protection are drawn.

## Getting started

### Requirements

- [KiCad](https://www.kicad.org/) **10.0** or newer. The project files use the KiCad 10 format and will not open in older versions.

### Opening the project

```bash
git clone https://github.com/varunsunkara15/Nexus_drive.git
cd Nexus_drive
```

Then open `hardware/nexus_drive.kicad_pro` in KiCad.

## Roadmap

- [x] Create the KiCad project and repository
- [ ] Define the specifications (battery voltage ✅, motor current, motor type)
- [ ] Choose the main parts (gate driver ✅, MOSFETs ✅, MCU ✅, shunts, encoder)
- [ ] Draw the schematic (phase power section ✅, gate driver ✅, MCU pin connections ✅, CAN-FD ✅, encoders ✅, sensing ✅, logic supplies, DC-link, input protection)
- [ ] Lay out the PCB
- [ ] Run design review, DRC and ERC
- [ ] Order and assemble the first prototype
- [ ] Bring up the board and write the firmware (FOC loop, communication)
- [ ] Test on a humanoid joint actuator

## Contributing

This project is under active development. Issues and pull requests are welcome. Please describe the change you're proposing, and why, before sending large design changes.

## License

No license has been chosen yet. Until one is added, all rights are reserved by the authors.
