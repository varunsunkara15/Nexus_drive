# Nexus Drive

**Nexus Drive** is an open-hardware motor driver we are designing for a humanoid robot's joints.

The goal is a compact board that can sit near each joint actuator and drive its motor accurately and efficiently. It should also report back to the robot's main controller.

> **Status:** 🚧 Early development. Schematic in progress (power stage, gate driver and MCU connections done); PCB layout not started.

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
│   ├── nexus_drive.kicad_pcb     PCB layout
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
| Encoder interface | TBD |
| Communication | TBD |
| Board dimensions | TBD |

### Note: bulk capacitance on the battery bus

The power stage in this repo has **no bulk / DC-link capacitor bank yet**. It is owned by another team member and must be added before the board is built. The reference design (moteus x1) uses 36 × 4.7 µF 100 V ceramics. A **bulk electrolytic (or tantalum) capacitor** at the power input may also be needed. Reasons:

- **Hot-plug ringing:** a bank of only ceramics has very low ESR. Together with the battery cable's inductance, it can ring up to ~2× the battery voltage when the battery is plugged in (~100 V on a 50.4 V pack), above the 80 V MOSFET rating. An electrolytic's higher ESR (≈ 0.05–0.1 Ω) damps this ringing.
- **Ceramics lose capacitance under DC bias:** 100 V X7R ceramics keep only about 30–50 % of their rated value at 50 V, so the real bus capacitance is much lower than the printed value.
- **Regen and load steps:** a larger capacitor absorbs some regenerative braking energy and supplies current during fast load changes, reducing bus voltage swings.

Guidance: rated **80 V or higher** (63 V is the bare minimum), capacitance several times the effective ceramic bank (e.g. 330–470 µF total), ESR ≈ 0.05–0.1 Ω, placed between the input connector and the half-bridges. **Tantalum is not recommended at this voltage:** tantalums need ~2× voltage derating (a >100 V part is hard to find), and they tend to fail short-circuit, which is a fire risk on a stiff 12S3P pack. Aluminium electrolytic or hybrid polymer is the safer choice. See [docs/power-stage.md](docs/power-stage.md) for the full overvoltage-protection requirements.

## Schematic review (29 Oct 2026)

This review covers **only the sheets drawn so far** (gate driver, H-bridges, current sense, MCU, CAN-FD, encoders) at commit `779c08d`. Blocks that are still to be added (logic supplies, DC-link, input protection, voltage/temperature sensing) are not reviewed here. It was checked against the part datasheets, the moteus x1 reference and the KiCad ERC/netlist.

**Design intent: the board is deliberately over-specified.** The first target motors are the CubeMars **RO80 KV105** (≈15 A rated, ~50 A peak) and **RO100 KV55** (≈20 A rated, ~62 A peak) at 48 V, but the power stage keeps moteus x1's 120 A-class parts, so that larger motors can be used later. The review keeps that headroom.

**Overall:** the power stage is a careful copy of a proven design and is mostly correct. There is one real wiring issue (R1), a few loose ends, and some decisions to settle for the over-spec use.

### ✅ What's done well
- Power stage copied from moteus x1 and checked pin for pin; Apache-2.0 attribution in NOTICE.md.
- 6-PWM on TIM1 with **DRV8353 nFAULT on TIM1_BKIN**: a driver fault stops the PWM in hardware.
- **One ADC per phase current** (ADC1/2/3), so the three phases are sampled at the same instant.
- Kelvin current-sense net ties, a 0.1 µF 100 V loop capacitor per half-bridge, and BAT41 clamps on the low-side gates.
- Gate-driver support parts correct: 47 nF 100 V charge-pump cap, VCP cap to VDRAIN, DVDD/VREF caps, open-drain pull-ups.
- CAN-FD done properly: TCAN1042GV (5 V VCC, 3.3 V VIO, STB defaults to normal), common-mode choke, ESD, jumper-selectable 120 Ω termination, two daisy-chain connectors.
- MCU: per-pin decoupling, ferrite-filtered VDDA/VREF+, NRST cap, BOOT0 pull-down + jumper, SWD header, 16 MHz crystal.
- Encoders: ESD (USBLC6) and 22 Ω series resistors on every external line.

### 🔴 Must fix
- [ ] **R1: the VGLS capacitor (C23) returns to `GND` instead of `GNDPWR`.** VGLS supplies the low-side gate drive. Its charge flows through the low-side FET source, the shunt and PGND, so the capacitor must return to **PGND/GNDPWR right at the DRV8353 PGND pin**. On `GND`, the gate current crosses between the two grounds every PWM cycle.
- [ ] **R2: `GND` and `GNDPWR` are not joined anywhere.** The DRV8353 needs AGND and PGND connected. Add **one net tie under the driver** (moteus has this on a sheet that was not copied).
- [ ] **R3: the encoder sheet is not connected to the MCU.** SPI1–3 and ENC1–3_CS have no sheet pins on the root sheet (12 ERC errors).

### 🟠 Should fix
- [ ] **R4: VM bulk capacitance.** VM (+12V) has only 0.1 µF (C19). Add the local bulk capacitor that the DRV8353 datasheet recommends for VM, next to the pin.
- [ ] **R5: current-sense gain must be set per motor (over-spec plan).** Keep the 0.5 mΩ shunts for headroom. The DRV8353 amplifier output swings about ±1.4 V around VREF/2, so the gain chosen over SPI sets the range:

  | Shunt | Gain | Full scale | Use |
  |---|---|---|---|
  | 0.5 mΩ | 40 | ±70 A | RO80 / RO100 (best resolution) |
  | 0.5 mΩ | 20 | ±140 A | Larger motors, up to the 120 A-class power stage |

  Make the gain a **per-motor firmware parameter**. At gain 40, the amplifier offset is a bigger share of the signal, so do the offset calibration in R7.
- [ ] **R6: gate-drive supply versus PWM frequency.** The FDMT80080DC (195 nC Qg) is kept for headroom. Six gates at 30 kHz need about 35 mA from the DRV8353 charge pump and VGLS. **Check this against the DRV8353 limits before fixing the PWM frequency**, and include it in the +12 V rail budget.
- [ ] **R7: current-sense zero offset.** The amplifier zero is VREF/2 from `+3V`, but the ADC reference is `+3VA` (after the ferrite). Calibrate the offsets at startup with the gates off, or tie the DRV8353 VREF to `+3VA`.
- [ ] **R8: external encoder power.** J903/J904 get the MCU's own `+3V`, so a shorted encoder cable resets the MCU. Add a polyfuse or load switch, or a separate LDO.
- [ ] **R9: SPI over cables.** Single-ended SPI next to motor phase wires is noise-prone. Keep external encoder cables short and the SPI clock low, or use a differential interface (RS-485 / SSI) for the joint-output encoder.
- [ ] **R10: thermal design for the over-spec rating.** The 120 A-class headroom only holds with matching copper and heatsinking: heavy copper for V+, phases and GNDPWR, a thermal path for the Dual Cool FETs, and shunts sized for the continuous current of the largest motor you plan to use.

### 🟡 Minor / housekeeping
- [ ] **R11:** Annotate the 49 un-annotated parts (`C?`, `R?`, `U?` on the CAN, encoder and MCU sheets). Until then, the netlist merges parts with the same reference.
- [ ] **R12:** Assign footprints to the parts on the CAN, encoder and MCU sheets (≈45 parts have none).
- [ ] **R13:** Delete the 6 dangling root-sheet labels (`SWDIO`, `SWCLK`, `NRST`, `BOOT0`, `OSC_IN`, `OSC_OUT`). They are already wired inside the MCU sheet.
- [ ] **R14:** Add the MA702GQ, TCAN1042GV and EMI-filter symbols to a library in `sym-lib-table`.
- [ ] **R15:** The SWD header order (VCC, SWDIO, SWCLK, NRST, GND) is not a standard pinout. Match it to your programmer, or use the standard 10-pin Cortex debug connector.
- [ ] **R16:** Check that the 12 pF crystal caps suit the 16 MHz crystal's load capacitance (12 pF caps ≈ 9–11 pF crystal with stray capacitance).
- [ ] **R17:** PD10 (DRV chip select) has no label in the `.ioc`, and USART3 is assigned but not connected. Add a debug UART header or free the pins.
- [ ] **R18:** Consider split CAN termination (2 × 60 Ω + ~4.7 nF to GND) for better EMC on a long robot bus.
- [ ] **R19:** Commit the literature review (moteus, ODrive, ODESC, ODRI) to `docs/`.

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
