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

## Schematic review (3 Nov 2026)

This review covers the schematic and the first PCB placement at commit `5195f6f`: footprint assignment, the HC-05 header on J1, footprint import, and the `sensing` sheet. It was checked against the KiCad netlist, ERC and DRC. Blocks that are still to be drawn (logic supplies, DC-link, input protection) are planned work and are not reviewed.

**Design intent: the board is deliberately over-specified.** The first target motors are the CubeMars **RO80 KV105** (≈15 A rated, ~50 A peak) and **RO100 KV55** (≈20 A rated, ~62 A peak) at 48 V. The power stage keeps moteus x1's 120 A-class parts so that larger motors can be used later.

**Checks:** ERC has 0 errors and 9 library warnings. PCB-to-schematic parity has 0 issues.

### ✅ Done well
- **All 115 parts have footprints, and the packages match the parts:** LQFP-80 for the STM32G474ME, RTA-40 for the DRV8353SRTA, PQFN 8×8 for the FETs, QFN-16 3×3 for the MA702 and SOIC-8 for the TCAN1042.
- **J1 (HC-05) is crossed over correctly:** MCU TX (PB10) goes to the module's RXD, and MCU RX (PE15) to its TXD.
- **The sensing sheet is well thought out:**
  - The VBUS divider (99.8k / 4.7k) gives 2.27 V at 50.4 V and reaches 3.3 V full scale at about 73 V, so it can see regen overvoltage.
  - The top resistor is split into two, so each 0603 sees only about 25 V.
  - The three phase-voltage dividers use the same ratio, filtered at about 3.5 kHz.
  - The thermistors have 1 % pull-ups and filter caps, and every signal goes to an ADC pin.
- Carried over from earlier work:
  - The power stage is copied from moteus x1 pin for pin.
  - DRV8353 nFAULT goes to TIM1_BKIN, so a driver fault stops PWM in hardware.
  - Each phase current has its own ADC.
  - CAN-FD has a choke, ESD protection and jumper-selectable termination.
  - Encoders have ESD protection and series resistors.

### 🔴 Fix before routing
- [ ] **N1: the VGLS capacitor C23 returns to `GND`.** VGLS supplies the low-side gate drive, so C23 must return to **`GNDPWR` at the DRV8353 PGND pin**. On `GND`, the gate current crosses between the two grounds every PWM cycle.
- [ ] **N2: `GND` and `GNDPWR` aren't joined anywhere.** NT1 joins only `GNDA` to `GND`, and NT3/5/7 are the shunt sense connections, so they don't count. Add **one net tie under the driver**.
- [ ] **N3: check FL1's pin numbers against the TDK ACT1210D datasheet.** The symbol uses windings 1–4 and 2–3. If TDK numbers them differently, the choke either shorts CANH to CANL or breaks the CAN line. Check the land pattern at the same time. Neither has been verified yet.
- [ ] **N4: placement has started before the parts that set the layout exist.** The DC-link capacitors, input connector and protection, and the logic supplies (+12 V, +5 V, +3.3 V) aren't drawn yet. Those decide where the power stage, the ground tie and the heat paths go. Finish them, plus N5, before placing parts seriously.

### 🟠 Should fix
- [ ] **N5: VM bulk capacitance.** +12 V (the DRV8353 VM pin) has only 0.1 µF (C19). Add the local bulk capacitor the DRV8353 datasheet recommends, next to the pin.
- [ ] **N6: drill size on the phase output pads.** X1–X3 each have 20 vias with 0.254 mm holes, but the board rule minimum is 0.3 mm, giving 60 DRC errors. Either confirm the fab can drill 0.25 mm and change the rule, or enlarge the holes to 0.3 mm. These vias carry phase current, so their size and count matter for the over-spec rating (N13).
- [ ] **N7: protect the motor thermistor input.** J7 leaves the board next to the phase wires and goes straight to the ADC with only 100 nF. Add about 1k in series and an ESD clamp. A motor thermistor sits inside the windings, so an insulation fault can put bus voltage on that ADC pin.
- [ ] **N8: HC-05 on J1.**
  - Nothing supplies `+5V` yet. Include the HC-05 (up to about 40 mA while pairing) and the TCAN1042 (about 70 mA) in the 5 V budget.
  - J1 is a male pin header and the HC-05 module also has male pins, so it needs a cable or a socket.
  - Treat Bluetooth as a bench and debug link only. CAN is the main bus for 12 joints.
- [ ] **N9: current-sense gain must be set per motor.** With the 0.5 mΩ shunts, gain 40 gives ±70 A full scale, for the RO80 and RO100. Gain 20 gives ±140 A, for larger motors. Make the gain a per-motor firmware parameter.
- [ ] **N10: gate-drive supply versus PWM frequency.** Six FDMT80080DC gates (195 nC each) at 30 kHz need about 35 mA. Check that against the DRV8353 charge-pump and VGLS limits before fixing the PWM frequency, and include it in the +12 V budget.
- [ ] **N11: sensing offset and ratiometric reference.** The current-sense amplifier zero (VREF/2) and the thermistor pull-ups run from `+3V`, but the ADC reference is `+3VA`. Calibrate the offsets at startup with the gates off, or move them to `+3VA`.
- [ ] **N12: external encoders.**
  - J903/J904 take power from the MCU's own `+3V`, so a shorted encoder cable resets the MCU. Add a polyfuse, a load switch or a separate LDO.
  - Keep single-ended SPI cables short and slow, or use RS-485/SSI for the joint-output encoder.
- [ ] **N13: thermal design for the over-spec rating.** The 120 A-class headroom only holds with:
  - heavy copper for V+, the phases and `GNDPWR`
  - a thermal path for the Dual Cool FETs
  - shunts sized for the largest motor you plan to use

### 🟡 Housekeeping
- [ ] **N14:** `sym-lib-table` has no `nexus_drive` entry, which causes 3 ERC warnings.
- [ ] **N15:** The U6 footprint differs from the library copy. Update it from the library, or note why it's custom.
- [ ] **N16:** Before routing, add the board outline. The 196 silkscreen warnings from placement can be cleaned up later.
- [ ] **N17:** Check the following:
  - The SWD header order (VCC, SWDIO, SWCLK, NRST, GND) isn't a standard pinout. Match it to your programmer.
  - The 12 pF crystal caps should suit the 16 MHz crystal's load capacitance.
  - PD10 (DRV chip select) has no label in the `.ioc`.
- [ ] **N18:** Consider split CAN termination (2 × 60 Ω + ~4.7 nF to GND) for better EMC on a long robot bus.
- [ ] **N19:** Commit the literature review (moteus, ODrive, ODESC, ODRI) to `docs/`.

**Most important:** N1, N2 and N4. Get the grounding and the missing power blocks into the schematic before routing.

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
