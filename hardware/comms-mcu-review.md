# Communications and MCU review follow-up

This records the hardware and CubeMX decisions for the [23 November schematic review](https://github.com/varunsunkara15/Nexus_drive/blob/9ac35e0/README.md#schematic-review-23-nov-2026).

| Item | Resolution |
| --- | --- |
| N3, FL1 | Selected TDK ACT1210D-101-2P-TL00. The TDK circuit drawing shows windings 1–4 and 2–3. The schematic and PCB use 1–4 for CANH and 2–3 for CANL. [TDK part and circuit drawing](https://product.tdk.com/en/search/emc/emc/cmf_cmc/info?part_no=ACT1210D-101-2P-TL00) |
| N8, J1 | The five-wire SmartElex HC-05 cable mapping is below. |
| N12, J5 | F1 and F2 provide 100 mA hold PTC protection on the encoder feeds. Both fuse inputs are routed to the logic rail. Both SPI buses are routed through their series resistors. Both SPI ports start at 625 kHz with 160 MHz peripheral clocks and a /256 divider. Check cable signal integrity and verify whether a cable fault resets the MCU. If continuous MCU operation is required during an encoder short, size an isolated feed or a current-limited switch with the power owner, using the actual encoder loads. |
| N15, U6 | The installed KiCad library and saved board agree on the pad geometry and pin orientation. The PCB uses a 3 × 3 mm QFN, 0.50 mm pitch, and a grounded 1.70 mm exposed pad with four paste apertures. The KiCad perimeter lands are 0.825 × 0.25 mm. Review the assembly land and stencil pattern with the fabricator when freezing the build. [MPS MA702 package drawing](https://www.monolithicpower.com/en/documentview/productdocument/index/version/2/document_type/Datasheet/lang/en/sku/MA702GQ-Z/) |
| N17, Y1 | The specified 16 MHz crystal has a 10 pF load rating. Two 12 pF capacitors give 6 pF in series, so the fitted values assume 4 pF of effective stray capacitance. Retain them as the prototype starting point. Check startup margin and frequency on assembled hardware, using a buffered clock output for the frequency check. [Abracon Y1 specification](https://abracon.com/parametric/crystals/ABM8-16.000MHZ-10-1-U-T) · [ST AN2867](https://www.st.com/resource/en/application_note/cd00221665-oscillator-design-guide-for-stm8afals-stm32-mcus-and-mpus-stmicroelectronics.pdf) |
| N18, CAN termination | Retain the 120 Ω resistor and JP2 for this revision. Close JP2 only when this board terminates a physical bus end. Two 120 Ω end terminations should measure about 60 Ω across CANH/CANL with the bus unpowered. Revisit split termination with the system owner if the full harness fails EMC testing. [TI TCAN1042-Q1 layout examples](https://www.ti.com/lit/ds/symlink/tcan1042gv-q1.pdf) |
| D7, motor NTC protection | Selected Nexperia PESD5V0S1BA,115, a 5 V bidirectional ESD diode in SOD323 that matches the fitted footprint. [Nexperia datasheet](https://assets.nexperia.com/documents/data-sheet/PESD5V0S1BA.pdf) |
| N24, C20 return | Keep C20 between DVDD (U5 pin 38) and the local power ground (U5 pin 39, `GNDPWR`). TI’s pin table calls out GND for this capacitor. [TI DRV8353 pin table](https://www.ti.com/lit/ds/symlink/drv8353.pdf#page=6) |

## Debug and communications cables

J2 is a 1 × 5 right-angle male header at 1.27 mm pitch. The plug enters parallel to the board from the upper right in the KiCad front view. Verify plug insertion and cable clearance in the actuator assembly. The table maps its nets to two common ST probes. Check the pin-1 markings and cable continuity before applying power. Feed probe VTref from J2.1 once Nexus has a regulated 3.3 V rail.

| Nexus J2 | Signal | ST-LINK/V2 CN3 | STLINK-V3MINIE CN4 |
| --- | --- | --- | --- |
| 1 | +3V / VTref | 1 (VAPP) | 3 (T_VCC) |
| 2 | SWDIO | 7 | 4 |
| 3 | SWCLK | 9 | 6 |
| 4 | NRST | 15 | 12 |
| 5 | GND | 4 (GND) | 5 (GND) |

[ST-LINK/V2 UM1075, table 4](https://www.st.com/resource/en/user_manual/um1075-stlinkv2-incircuit-debuggerprogrammer-for-stm8-and-stm32-stmicroelectronics.pdf) · [STLINK-V3MINIE UM2910, table 4](https://www.st.com/resource/en/user_manual/dm00813040-.pdf)

J1 is a front-mounted JST GH SM05B-GHS-TB side-entry header, angled toward the upper left edge in the KiCad front view. Use a GHR-05V-S housing and a five-wire adapter to the SmartElex HC-05. Map the housing cavities by their pin numbers and the breakout by its printed labels. The module's EN/KEY pin is left open.

| Nexus J1 pin | Connect to SmartElex HC-05 | Purpose |
| --- | --- | --- |
| 1 | RXD | USART3 TX from the STM32 |
| 2 | TXD | USART3 RX into the STM32 |
| 3 | GND | Common return |
| 4 | VCC | +5 V supply to the breakout |
| 5 | STATE | Bluetooth link status into PB11 (U1 pin 42). CubeMX configures a GPIO input with a pull-down. |

Check cable continuity and plug clearance in the actuator before assembly. [SmartElex module pinout](https://robu.in/wp-content/uploads/2024/10/SmartElex-HC-05-6-pin-Bluetooth-Module-%E2%80%93-1-PCS.pdf)

### Actuator harness revision, 24 November 2026

Both board headers mount on the front and accept the plug parallel to the PCB. J3 faces the right edge and J5 faces the top edge in the KiCad front view. Use the header's pin-1 mark to map the mating housing. Check the plugged-in housings, latch access, wire bends and LED sight lines in the actuator assembly before releasing the harness.

| Header | Board header and mating housing | Pin assignment |
| --- | --- | --- |
| J3, CAN | 2.00 mm JST PH S3B-PH-K-S with PHR-3 | 1 CANH, 2 CANL, 3 GND |
| J5, external encoder 2 | 1.25 mm JST GH SM14B-GHS-TB with GHR-14V-S | 1 fused +3V (F1), 2 SPI3 SCK, 3 MISO, 4 MOSI, 5 CS, 6 GND |
| J5, external encoder 1 | Same 14-position housing | 7 fused +3V (F2), 8 SPI2 SCK, 9 MISO, 10 MOSI, 11 CS, 12 GND |
| J5, motor thermistor | Same 14-position housing | 13 NTC input, 14 GNDA return |

J3 has one bus connection. A mid-bus drive needs pass-through in the harness, with a short branch to J3. Close JP2 only at a bus end. J5 pin 13 reaches the motor-temperature divider and STM32 through R39. D7 clamps the cable side to GNDA. U3 and U4 protect the encoder signals directly under J5. [JST PH drawing](https://www.jst-mfg.com/product/pdf/eng/ePH.pdf) · [JST GH drawing](https://www.jst.com/wp-content/uploads/2025/06/eGH.pdf)

KiCad connectivity confirms all eight STM32 encoder signals reach J5 through their series resistors. J5 pins 2 through 5 reach SPI3 clock, MISO, MOSI and chip select through R20 through R23. Pins 8 through 11 reach the corresponding SPI2 signals through R8 through R11. The final board check reports zero DRC violations, 115 open connections across the board and zero schematic parity issues. Cable signal integrity still requires testing on the assembled board.

### Drive indicators

| Indicator | Circuit | Behaviour |
| --- | --- | --- |
| D8, 3V3 | Green 150060VS75000 with R40 (1 kΩ) | Lights when the regulated logic rail is powered, including during MCU reset. |
| D9, STAT | Common-anode 150080M153000 on +3V. PB6 sinks red through R41 (1 kΩ), PB7 sinks green through R42 (330 Ω), and PB9 sinks blue through R43 (330 Ω). | Blue idle, cyan ready, a 250 ms green pulse once a second while running, red fault. |

CubeMX configures the three GPIO outputs high at initialization. After generating the application, call `NexusStatusLed_Update(state, HAL_GetTick())` periodically following GPIO initialization and supply the actual drive state, giving faults precedence. The source and header are in `firmware/Core/Src` and `firmware/Core/Inc`. Verify brightness and colour balance on the 3.3 V rail with the housing fitted, particularly the green and blue channels. [D8 datasheet](https://www.we-online.com/components/products/datasheet/150060VS75000.pdf) · [D9 datasheet](https://www.we-online.com/components/products/datasheet/150080M153000.pdf)

## Firmware and first-board checks

- The CubeMX pinout starts all encoder chip selects and `DRV_nCS` high, starts `GATE_EN` low, and requests CAN standby high until firmware initializes FDCAN and drives it low. These are GPIO initialization levels. The hardware pulls CAN_STB low during MCU reset. Check the generated initialization and the pin levels on the prototype.
- Start CAN with the robot controller’s agreed nominal and data bit rates. Verify transmit and receive with the actual termination and harness. Check external encoder transactions with each selected encoder’s SPI mode, cable and motor switching conditions before increasing SCK.
- For N7, the motor NTC and R39 (1 kΩ) form the lower leg of the divider against R35 (10 kΩ). With the ADC and divider sharing +3VA, let `r` be the corrected ADC reading divided by full scale. Use `R_NTC = 10 kΩ × r / (1 − r) − 1 kΩ`, handling out-of-range and near-full-scale readings before applying the actual NTC curve.
- For N9 and N11, choose current-sense gain in the motor profile and calibrate each ADC channel at zero phase current, with PWM outputs off and the sense amplifiers powered. Use the measured offset in conversion. With a 0.5 mΩ shunt, gains 40 and 20 yield approximately ±70 A and ±140 A at ±1.4 V of amplifier swing. Confirm shunt value, ADC range and gain on hardware.
- For N10 and N23, confirm the gate-drive supply budget before choosing the PWM frequency. Agree the bus-voltage thresholds and the regenerative current response with the power owner before enabling braking. Verify the VBUS measurement and fault response during controlled bring-up.
- N21 still needs rail sources and a current budget. Give the power owner loads for the TCAN1042 5 V supply and 3.3 V VIO, MCU, onboard encoder, both external encoders and optional HC-05. Complete N20–N23 before committing further copper routing around the power stage.