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

## Schematic review (23 Nov 2026)

This review covers the schematic at commit `200ccbd`. It was checked against the KiCad netlist, ERC, DRC (schematic parity) and the pin plan in `firmware/STM32G474 Pinout.ioc`. Item numbers continue from the 3 Nov review, so they can be tracked.

**Design intent: the board is deliberately over-specified.** The first target motors are the CubeMars **RO80 KV105** (≈15 A rated, ~50 A peak) and **RO100 KV55** (≈20 A rated, ~62 A peak) at 48 V. The power stage keeps moteus x1's 120 A-class parts so that larger motors can be used later.

**Checks:**
- **ERC:** 0 errors, plus 9 library-symbol warnings.
- **Schematic parity with the PCB:** 0 issues.
- **Pin plan:** all 40 pins in the `.ioc` match the schematic net for net. That covers PWM on TIM1, nFAULT on TIM1_BKIN, the three phase currents on ADC1/2/3, SPI4 for the DRV8353, FDCAN1 and USART3.

### ✅ Fixed since 3 Nov
| Item | Now |
|---|---|
| N1 | C23 (VGLS) returns to `GNDPWR`. |
| N2 | NT8 joins `GND` and `GNDPWR` once, under the driver. |
| N5 | C38 (22 µF 25 V) added on +12 V (VM). |
| N6 | The phase pads were replaced (`PhasePad_1.91mm`), and the drill errors are gone. |
| N7 | R39 (1k) and D7 (ESD) protect the motor thermistor input. Firmware must subtract 1k from the reading. |
| N11 (part) | The thermistor pull-ups moved to `+3VA`. |
| N12 | PTC fuses F1/F2 feed the external encoder power. |
| N14 | `nexus_drive` is now in `sym-lib-table`. |
| N16 | The board outline exists. |
| N17 (part) | PD10 is labelled `DRV_nCS` in the `.ioc`. |
| — | External connectors are merged as the mechanics require: J3 (CAN in/out + motor thermistor) and J5 (both encoder ports). The pinouts in `CHANGES_connectors.txt` match the netlist. |

### 🔴 Must add before the PCB can work
- [ ] **N20: the board has no power input.** Nothing on the PCB connects to the battery bus: there's no XT60 or power pads for `V+`, and none for `GNDPWR`. Today `V+` only reaches the FET drains, C21/C24-C26, R24 and the DRV8353 VDRAIN pin. Add the input connector, or solder pads sized for the cable, matching the PDB's XT60 outputs.
- [ ] **N21: no supply makes the logic rails.** `+12V` (DRV8353 VM), `+5V` (TCAN1042, HC-05) and `+3V`/`+3VA` (MCU, encoders, DRV8353 VREF) all have consumers but **no source**, so the board can't power up. Suggested power tree, with a rough budget:

  | Rail | Feeds | Approx. load | Suggested source |
  |---|---|---|---|
  | +12 V | DRV8353 VM (gate drive about 35 mA at 30 kHz, plus quiescent) | ~60–100 mA | Synchronous buck from `V+`, rated for **≥ 80 V input** (regen spikes) |
  | +5 V | TCAN1042 (~70 mA when dominant), HC-05 (~40 mA) | ~120 mA | Buck or LDO from +12 V |
  | +3.3 V | STM32G474 at 160 MHz, MA702, two external encoders, TCAN VIO | ~150–200 mA | LDO from +5 V |

  Keep the whole board's idle draw under about **2 W**. During PDB precharge, all 12 drivers together must draw less than about 0.6 A.
- [ ] **N22: DC-link capacitance is still only 3 × 0.1 µF.** This is the "bulk capacitance" note above, still open. Add the ceramic bank close to each half-bridge, plus bulk at the input (aluminium electrolytic or hybrid polymer, rated 80 V or more). Size the ceramics for ripple current: roughly half the phase RMS current of the largest motor you plan to use.
- [ ] **N23: input overvoltage protection is missing,** and the FETs are only **80 V** parts (FDMT80080DC) on a 50.4 V bus. Add a TVS from `V+` to `GNDPWR` with standoff at or above 50.4 V and a clamping voltage below 80 V at the expected surge current. That margin is tight, so a TVS alone handles only plug-in spikes. **Regen must be limited in firmware** using the VBUS reading, with a brake resistor if needed.
- [ ] **N4: stop routing until N20–N23 are drawn.** The PCB is already routed (147 connections left), but the input connector, bulk capacitors and three regulators need space near the power stage and the ground tie. Adding them now will move parts that are already routed.
- [ ] **N3: FL1 (TDK ACT1210D) pinout still not verified.** The symbol and footprint use windings 1–4 and 2–3 (pads 1/2 on one side, 4/3 on the other). If TDK numbers them differently, CANH and CANL are shorted through the choke. Check the pin drawing in the datasheet on TDK's ACT1210D product page before ordering.

### 🟠 Should fix
- [ ] **N24: C20 (DVDD, 1 µF) returns to `GNDPWR`.** The DRV8353 datasheet places the DVDD capacitor to **AGND** (`GND` here). Move it to `GND` next to pin 25.
- [ ] **N11 (rest):** the DRV8353 VREF (pin 24) still runs from `+3V`, while the ADC reference is `+3VA`. Calibrate the current-sense offsets at startup with the gates off, or move VREF to `+3VA`.
- [ ] **N12 (rest):** the PTCs take hundreds of milliseconds to trip. A hard short on an encoder cable can still pull `+3V` down long enough to reset the MCU. That's acceptable for now. A separate small LDO for the encoder ports would remove it.
- [ ] **D7:** the value is only "ESD 5V bidir". Pick a real part number so the BOM can be made.
- [ ] **N9, N10 and N13 are carried over and still open:** current-sense gain per motor (gain 40 = ±70 A, gain 20 = ±140 A with the 0.5 mΩ shunts); gate-drive budget against PWM frequency; thermal design for the over-spec rating.

### 🟡 Housekeeping
- [ ] **N8:** the HC-05 needs a cable or socket, since J1 is a male header. Use it as a bench link only.
- [ ] **N15:** the U6 footprint still differs from the library copy (1 DRC warning). Update it, or note why it's custom.
- [ ] **N17 (rest):** match the SWD header to your programmer. Check that the 12 pF crystal caps suit the new 3225 crystal's load capacitance.
- [ ] **N18:** consider split CAN termination (2 × 60 Ω + about 4.7 nF to GND).
- [ ] **N19:** commit the literature review (moteus, ODrive, ODESC, ODRI) to `docs/`.
- [ ] The status line at the top of this README still says "PCB layout not started".

**Most important:** N20–N23. Without an input connector, logic supplies, DC-link capacitors and input protection, the board can't be powered. Draw them before routing any further.

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
