# Motor Driver Power Stage — Gate Driver, MOSFETs, Phase Output, Sensing

## Goal

Design the **power stage** of a three-phase BLDC/PMSM FOC motor controller for a robot.
This part of the project covers:

1. **Gate driver**: turns the MCU's PWM signals into gate drive for six MOSFETs.
2. **MOSFET bridge**: three half-bridges that switch the battery onto the three motor phases.
3. **Phase power output**: the three motor phase connections.
4. **Sensing**: low-side shunts for phase current, and voltage dividers for phase and bus voltage.

Out of scope here (handled by teammates): MCU, logic power supplies, encoder, communication.

**Top design priority: the MOSFETs must not fail from overvoltage or overcurrent.**

## Power source

| Parameter | Value |
|---|---|
| Battery | 12S3P Li-ion / LiPo |
| Nominal | ~44–48 V |
| Fully charged | **50.4 V** (4.2 V/cell): the design maximum |
| Empty | ~36–40 V |

The 3P pack is "stiff" (low internal resistance), so plug-in inrush and voltage spikes are large.

## Main components (based on the mjbots moteus x1, rated 10–54 V / 12S)

| Function | Part | Key specs |
|---|---|---|
| Gate driver | **TI DRV8353S** (DRV8353SRTAR, 40-pin WQFN) | 9–100 V operating, 102 V abs max; high-side charge pump (100% duty, no bootstrap caps); 3 integrated bidirectional current-sense amplifiers, gain 5/10/20/40; IDRIVE 50 mA–1 A source / 100 mA–2 A sink; VDS overcurrent, UVLO, overtemperature; SPI config + nFAULT |
| MOSFETs (×6) | **onsemi FDMT80080DC** | 80 V, 1.35 mΩ max at VGS = 10 V, Dual Cool 88 package; Qg 195 nC typ, Qgd 36 nC; VGS(th) 2–4 V; VGS max ±20 V; EAS 1734 mJ |

Notes:
- The DRV8353H (hardware-pin config) or DRV8353RS (adds a 350 mA buck) are alternatives; **DRV8353S is the default** for SPI tuning and fault readout.
- Average gate current ≈ Qg × f_PWM × 6 (≈ 35 mA at 30 kHz). Verify against the DRV8353S charge-pump and VGLS supply limits for the chosen PWM frequency.
- Start with IDRIVE ≈ 0.5 A source / 1 A sink (≈ 70 ns / 36 ns edges) and tune on hardware.

## Required overvoltage protection (mandatory, not optional)

80 V MOSFETs give ~1.6× margin over a full 12S pack. That margin only holds if spikes are controlled, so all of the following must be installed:

| Item | Placement | Spec / guidance | Purpose |
|---|---|---|---|
| **TVS diode** | At the power input | Standoff 51–54 V (above 50.4 V), e.g. SMBJ51 / SMCJ54 class. **Check its clamping voltage at the expected surge current against the 80 V MOSFET rating** | Clamps hot-plug, load-dump and fault-clearing spikes |
| **Bulk (buffer) capacitor** | At the power input, between connector and bridge | Electrolytic or polymer, **63 V minimum, 80 V preferred**, capacitance several times the ceramic bank, ESR ≈ 0.05–0.1 Ω | Damps hot-plug LC ringing (all-ceramic banks ring to ~2× the battery voltage); absorbs some regen |
| **Ceramic DC-link bank** | Tight against each half-bridge | **100 V X7R**, e.g. moteus uses 36 × 4.7 µF (169 µF nominal) plus 0.1 µF, 100 V on each leg | Supplies switching current; keeps switching spikes small |
| **Anti-spark connector** | Battery input | XT90-S / AS150 or a precharge circuit | Limits inrush and hot-plug ringing at the source |

Also:
- Firmware overvoltage limit around 52–54 V, with regen-current limiting.
- On overvoltage at speed, short the motor (all three low sides on) rather than disabling all switches, which lets the body diodes pump the bus.
- Decide with the team whether a brake chopper + resistor is needed (required if ever powered from a bench supply).

## Sensing

**Current (phase):**
- Three low-side shunts, one per phase, in the low-side MOSFET source paths.
- Kelvin (4-wire) sense traces from each shunt to the DRV8353S SPx/SNx inputs, routed as pairs.
- RC filter at the amplifier inputs (e.g. ~100 Ω + 1–2.2 nF) and/or at the outputs (moteus: 22 Ω + 1 nF).
- **Shunt value: TBD.** It depends on the motor's continuous and peak phase current. Sizing rule: I_peak × R_shunt × gain ≈ 1.5 V (half the 3.3 V ADC range, for bidirectional sensing). Check I² × R heat at continuous current.
- Reference: moteus x1 uses 0.5 mΩ shunts (120 A peak, 25 A / 60 A continuous).

**Voltage:**
- Phase-voltage dividers (×3) and a bus-voltage divider, ratio about ÷20, e.g. 68 kΩ / 3.3 kΩ (50.4 V → ~2.3 V, headroom to ~71 V).
- 10–100 nF filter capacitor on the low side of each divider.

## Layout rules

- Minimise the power loop: ceramic bank → high-side FET → low-side FET → shunt → back to the bank.
- Solid ground plane directly under the power-stage layer (thin prepreg), stitched with many vias.
- Keep switch-node (phase) copper compact; motor phase pads right at the half-bridges.
- Driver support capacitors (charge pump, VCP, VGLS, DVDD, VM) tight to the DRV8353S pins.
- Route each high-side gate trace alongside its phase return; keep gate loops short.
- Optional footprints: 0 Ω series gate resistors and 10–100 kΩ gate-to-source pull-downs.

## Schematic status

The phase power section has been copied into the schematic (sheets in [`hardware/`](../hardware/)) from the moteus x1 (see [NOTICE.md](../NOTICE.md)):

| Sheet | Contents (per instance) |
|---|---|
| `H Bridge A/B/C` (`h_bridge.kicad_sch`, 3 instances) | High-side + low-side FDMT80080DC; 0.5 mΩ 2512 low-side shunt; 0.1 µF 100 V 1812 cap from V+ to the low-side source; BAT41KFILM Schottky from the low-side gate to GNDPWR (clamps negative gate undershoot); net ties for Kelvin sensing (shunt top → SPx, GNDPWR → SNx); M2.5 screw pad for the phase output |
| `Gate Driver` (`gate_driver.kicad_sch`) | DRV8353SRTAR (U5) with its support parts: 47 nF 100 V charge-pump flying cap (CPH–CPL), 1 µF 25 V VCP cap to V+, 1 µF VGLS, 1 µF DVDD, 1 µF VREF, 0.1 µF 100 V VM decoupling, 2 kΩ pull-ups on SDO and nFAULT |
| `Current Sense` (`current_sense.kicad_sch`) | 22 Ω + 1 nF RC filter on each DRV8353S current-amplifier output (CURx → FCURx to the MCU ADC) |

The netlist of these sheets was checked pin-for-pin against moteus x1. The only differences are the removed DC-link bank and the 6-PWM change below.

**Not included:** the ceramic DC-link bank (moteus: 36 × 4.7 µF 100 V) and any bulk electrolytic. These are owned by another team member; see the note in the [README](../README.md#note-bulk-capacitance-on-the-battery-bus). Only the 0.1 µF local capacitor in each half-bridge and the gate driver's own support capacitors are on these sheets.

How the gate driver is wired:
- **VDRAIN** senses V+ (used for the high-side VDS overcurrent monitor and charge pump). **VM** (driver supply) comes from the **`+12V`** rail, not from the battery (as on moteus x1). This keeps driver dissipation low but means the logic-power sheet must provide +12 V.
- **6-PWM mode (changed from moteus):** moteus runs 3x PWM with INLA/B/C tied together as `MOTOR_HIZ`. Our MCU pin plan uses TIM1 complementary outputs, so INLA/B/C are split out. **Firmware must set the DRV8353S `PWM_MODE` register to 6x** and configure dead time in TIM1.
- `VREF` is the `+3V` rail, so the bidirectional amplifier outputs are centred on VREF/2.

## MCU connections

The MCU sheet (`hardware/stm32g474.kicad_sch`) holds an **STM32G474METx (LQFP-80)** with the pin assignment from `firmware/Comms Component Selection.ioc`.

| Net | MCU pin | Function | DRV8353S / circuit |
|---|---|---|---|
| `PWM_AH` / `PWM_AL` | PA8 / PA7 | TIM1_CH1 / CH1N | INHA / INLA |
| `PWM_BH` / `PWM_BL` | PA9 / PE10 | TIM1_CH2 / CH2N | INHB / INLB |
| `PWM_CH` / `PWM_CL` | PA10 / PB1 | TIM1_CH3 / CH3N | INHC / INLC |
| `GATE_EN` | PC4 | GPIO out | ENABLE |
| `DRV_nFAULT` | PB12 | TIM1_BKIN (active low) | nFAULT (2 kΩ pull-up). A driver fault shuts the PWM off in hardware |
| `DRV_SCLK` / `DRV_SDO` / `DRV_SDI` | PE12 / PE13 / PE14 | SPI4 SCK / MISO / MOSI (16-bit, CPHA = 1) | SCLK / SDO / SDI |
| `DRV_nCS` | PD10 | GPIO out | nSCS. **PD10 has no label in the .ioc; confirm it is the DRV chip select** |
| `CURR_A` / `CURR_B` / `CURR_C` | PA0 / PC3 / PE9 | ADC1_IN1 / ADC2_IN9 / ADC3_IN2 | SOA / SOB / SOC through the 22 Ω + 1 nF filter |

Inside the gate driver and current-sense sheets the moteus net names remain (`CUR1` = SOC, `CUR3` = SOA). On the root sheet they are mapped so `CURR_A` really is phase A.

Pins assigned but **not yet connected to a circuit** (ERC reports them as single-pin labels): `VPH_A/B/C`, `VBUS`, `TEMP_FET_HS`, `TEMP_FET_LS`, `TEMP_MOTOR` (sensing circuits still under discussion), and the teammates' `SPI1-3` + `ENC1-3_CS` (encoders), `CAN_TX/RX/STB`, `SWDIO/SWCLK`, `NRST`, `BOOT0`, `OSC_IN/OUT`.

**Still needed on the MCU sheet (MCU owner):** decoupling capacitors, VDDA filtering, HSE crystal/oscillator, NRST capacitor and SWD header, BOOT0 pull-down. Power pins are connected to the global `+3V` and `GND` nets.

Nets on the root sheet: `GHx`, `SHx` (= phase output), `GLx`, `SPx`, `SNx` for x = A/B/C link the driver and the bridges. The global supplies `+12V`, `+3V`, `GND` have no source until the logic-power sheet is added. Bus nets are the global labels `V+` and `GNDPWR`.

Deliberately not changed yet: the 0.5 mΩ shunt (still TBD, see Sensing), and no gate resistors or gate pull-downs (moteus has none; see Layout rules).

## Open items

- [ ] Motor continuous and peak phase current → shunt value and thermal design
- [ ] PWM frequency → check gate-driver supply current and switching loss
- [ ] Final TVS part, checked against the MOSFET rating
- [ ] Bulk capacitor part (value, voltage, ESR, ripple-current rating)
- [ ] Brake chopper: yes or no

## Reference designs

- **mjbots moteus x1** (primary reference): DRV8353S + FDMT80080DC, SMBJ51D TVS, 36 × 4.7 µF 100 V ceramics, 0.5 mΩ shunts.
- Also reviewed: VESC 4.12, RoboMotor-GIM6010, Tinymovr, ODrive/ODESC, ODRI Micro Driver v2. Most of these use 60 V-class parts and are **not suitable for a 12S bus**.
