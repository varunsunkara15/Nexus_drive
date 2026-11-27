# ODrive walkthrough, module 1: `Board/v3/board.cpp`

Part of the series that follows [`../odrive-firmware-study.md`](../odrive-firmware-study.md). Source: [odriverobotics/ODrive](https://github.com/odriverobotics/ODrive) at commit `3a2e4dd`, file `Firmware/Board/v3/board.cpp` (593 lines). Line numbers below refer to that file.

## What this file is

`board.cpp` is the **glue between the hardware and the motor-control code**. It does four jobs:

1. **Builds every object** (motors, encoders, gate drivers, axes) and tells each one which pins and timers it owns. *(lines 1–264)*
2. **Starts the hardware** (clocks, peripherals, interrupt priorities, gate-driver reset). *(lines 266–378)*
3. **Starts the PWM timers in sync** and arms the ADC triggers. *(lines 380–416)*
4. **Runs the real-time heartbeat**: the two interrupts that read the ADCs and call the control loop 8,000 times a second. *(lines 418–571)*

Parts 1–3 run once at boot. Part 4 is the most important code in ODrive.

Two terms used throughout:

- **Motor 0 / M0** uses timer TIM1. **Motor 1 / M1** uses TIM8. ODrive drives two motors; we drive one.
- **Center-aligned PWM:** the timer counts 0 → 3500 → 0 → 3500 … (a triangle). Each full triangle is one PWM period (24 kHz). At the **bottom** (count 0) all three low-side FETs are on; at the **top** (count 3500) all three high-side FETs are on.

---

## Part 1: Building the objects (lines 1–264)

### Lines 1–19: includes and one stray variable

```cpp
#include <board.h>
#include <odrive_main.h>
...
bool TaskTimer::enabled = false;
```

Pulls in the board constants (`board.h`), the main ODrive classes, and the CubeMX-generated peripheral headers (`adc.h`, `tim.h` …). `TaskTimer::enabled` switches the code profiler on or off (it measures how long each control-loop task takes). They put it here only to avoid a one-line file.

**For us:** nothing to copy. A profiler is useful later; a cycle counter (`DWT->CYCCNT`) is enough.

### Lines 21–24: borrowing an unused interrupt

```cpp
#define ControlLoop_IRQHandler OTG_HS_IRQHandler
#define ControlLoop_IRQn OTG_HS_IRQn
```

ODrive needs a **second, lower-priority interrupt** to run the control loop in (explained in Part 4). The STM32F405 has a USB High-Speed peripheral that the board doesn't use, so ODrive **borrows its interrupt vector** and triggers it from software. Renaming it `ControlLoop` makes the rest of the code readable.

**For us:** the same trick works on the G474. Pick any interrupt of a peripheral we don't use and trigger it in software (`NVIC->STIR` or `HAL_NVIC_SetPendingIRQ`).

### Line 30: fake OTP

```cpp
uint8_t __attribute__((section(".testdata"))) fake_otp[...] = {0, 0, 0, HW_VERSION_MAJOR, HW_VERSION_MINOR, HW_VERSION_VOLTAGE};
```

ODrive burns the board version (e.g. v3.6, 56 V) into the chip's one-time-programmable (OTP) memory at the factory. This is a RAM copy used for testing the factory scripts before they write to the real OTP.

**For us:** drop it. We build one board version.

### Lines 32–37: shared SPI bus and UART handles

```cpp
Stm32SpiArbiter spi3_arbiter{&hspi3};
UART_HandleTypeDef* uart_a = &huart4;
```

On ODrive both gate drivers **and** the SPI encoders share **one** SPI bus (SPI3). The *arbiter* is a queue that makes sure only one device uses the bus at a time. The UART pointers name which hardware UARTs carry ODrive's protocols.

**For us:** we don't need an arbiter. The DRV8353S has its own bus (SPI4), and the MA702 encoder has its own (SPI1/2/3). One device per bus means no sharing logic.

### Lines 39–51: the two gate drivers

```cpp
Drv8301 m0_gate_driver{ &spi3_arbiter, {M0_nCS...}, {}, {nFAULT...} };
```

Each motor's DRV8301 gate driver gets: the SPI bus, its chip-select pin, its enable pin (left empty because both drivers share one enable pin, handled in `board_init`), and the shared fault pin.

**For us:** one `Drv8353` object: SPI4, chip select `PD10`, enable `PC4` (`GATE_EN`), fault `PB12` (`DRV_nFAULT`).

### Lines 53–73: temperature sensors

```cpp
const float fet_thermistor_poly_coeffs[] = {363.9, -462.2, 307.6, -27.7};
OnboardThermistorCurrentLimiter fet_thermistors[AXIS_COUNT] = { {15, ...}, {4 or 1, ...} };
OffboardThermistorCurrentLimiter motor_thermistors[AXIS_COUNT];
```

- The four numbers are a **cubic polynomial** that converts the thermistor's ADC reading (as a fraction of full scale) straight to °C. This avoids a logarithm (the usual Steinhart–Hart thermistor equation) at run time.
- Each FET thermistor is told which ADC channel it's on. The `#if` picks the channel for older board revisions.
- "CurrentLimiter" in the name: as the temperature rises, these objects **reduce the allowed motor current** (they don't just trip a fault).
- Motor thermistors are "offboard" (in the motor) and configured by the user.

**For us:** keep the idea. We have `TEMP_FET_HS`, `TEMP_FET_LS` and `TEMP_MOTOR`. Fit our own polynomial to our thermistor's datasheet. Remember README item N7: the motor thermistor has a 1 kΩ series resistor that the conversion must subtract.

### Lines 75–94: the two motors

```cpp
Motor motors[AXIS_COUNT] = {
    { &htim1, 0b110, 1.0f / SHUNT_RESISTANCE, m0_gate_driver, m0_gate_driver, fet_thermistors[0], motor_thermistors[0] },
    ...
```

Each `Motor` is told:

| Argument | Meaning |
|---|---|
| `&htim1` | The timer that makes this motor's PWM |
| `0b110` | **Current-sensor mask**: which phases have a shunt. Bits are C-B-A, so `110` = phases B and C only. Phase A is calculated as −B − C |
| `1.0f / SHUNT_RESISTANCE` | Shunt conductance (1 / 0.5 mΩ = 2000 S), used to turn shunt voltage into amps |
| gate driver (twice) | Once as the switch driver, once as the current-sense amplifier ("opamp"). The DRV8301 does both |
| thermistors | For the temperature-based current derating |

**For us:** one motor, `&htim1`, mask `0b111` (we have three shunts), our shunt value (0.5 mΩ, still TBD per the README).

### Lines 96–113: the two encoders

Each encoder object gets a hardware timer (TIM3/TIM4, used to count A/B pulses from an incremental encoder), the A, B and Z (index) pins, which double as hall-sensor inputs, and the SPI bus for absolute encoders.

ODrive supports five encoder types with one object, so it needs all these pins.

**For us:** much simpler. The MA702 only needs an SPI bus and a chip-select pin.

### Lines 115–155: everything else, and the two axes

```cpp
Endstop endstops[2 * AXIS_COUNT];
MechanicalBrake mechanical_brakes[AXIS_COUNT];
SensorlessEstimator sensorless_estimators[AXIS_COUNT];
Controller controllers[AXIS_COUNT];
TrapezoidalTrajectory trap[AXIS_COUNT];

std::array<Axis, AXIS_COUNT> axes{{ { 0, 1, 2, osPriorityHigh+1, encoders[0], ..., motors[0], ... }, ... }};
```

An **Axis** bundles everything for one motor: encoder, controller, motor, trajectory planner, endstops, brake. It also gets the step/dir pin numbers and the priority of its state-machine thread (axis 0 slightly higher than axis 1).

Note that **all objects are statically allocated** (global arrays, no `new`). That's good practice in embedded code: memory use is known at compile time and nothing can fail to allocate at run time.

**For us:** keep the static allocation. Drop endstops, mechanical brake, sensorless estimator and trajectory planner. Our "axis" is just: encoder + controller + motor + state.

### Lines 159–255: GPIO tables

Three versions of a pin table (for board revisions 3.1–3.2, 3.3–3.4, 3.5–3.6), then a table of **alternate functions** each general-purpose pin can be switched to at run time: UART, PWM input, encoder, I²C, CAN.

ODrive lets the *user* decide in configuration what each spare pin does. That's why this table exists.

**For us:** drop it. Our pin functions are fixed by the schematic and set once by CubeMX (`firmware/STM32G474 Pinout.ioc`).

### Lines 257–264: PWM input and USB handle

`PwmInput` reads hobby-RC style PWM signals on TIM5 as a command source. The USB handle is for ODrive's USB protocol.

**For us:** drop both.

---

## Part 2: Starting the hardware (lines 266–378)

### Lines 266–290: `system_init()` — clocks and board check

```cpp
HAL_Init();                 // reset peripherals, set up SysTick (1 ms tick)
SystemClock_Config();       // PLL → 168 MHz core clock (CubeMX-generated)
const uint8_t* otp_ptr = (const uint8_t*)FLASH_OTP_BASE;
if (*otp_ptr == 0xff) otp_ptr = fake_otp;      // blank OTP → use the RAM copy
if (!check_board_version(otp_ptr)) for (;;);    // wrong board → hang forever
```

1. Basic HAL init and the clock tree.
2. Reads the board version from OTP. `0xff` means the OTP was never written.
3. If the firmware was built for a different board, **it stops here on purpose**. It never turns on the gate driver, so wrong firmware can't damage the board.

**For us:** keep `HAL_Init()` and `SystemClock_Config()` (170 MHz on the G474). The version-check idea is cheap and worth keeping if we ever have more than one board revision.

### Lines 292–306: `board_init()` — peripherals

```cpp
MX_GPIO_Init(); MX_DMA_Init();
MX_ADC1_Init(); MX_ADC2_Init();
MX_TIM1_Init(); MX_TIM8_Init();   // motor PWM
MX_TIM3_Init(); MX_TIM4_Init();   // encoder counters
MX_SPI3_Init(); MX_ADC3_Init();
MX_TIM2_Init(); MX_TIM5_Init(); MX_TIM13_Init();
```

Calls the CubeMX-generated init functions. The settings that matter most are inside `Src/tim.c` and `Src/adc.c`:

**TIM1/TIM8 (PWM), `tim.c:77–144`:**

| Setting | Value | Meaning |
|---|---|---|
| `CounterMode` | `CENTERALIGNED3` | Triangle counting; update events at both the top and the bottom |
| `Period` | 3500 | 168 MHz / (2 × 3500) = 24 kHz PWM |
| `RepetitionCounter` | 2 | Only every **3rd** top/bottom event becomes an "update event" |
| `OCMode` | `PWM2` | Output high when count ≥ compare value. At count 0 every high-side is off, so the low-sides are on |
| `DeadTime` | 20 clocks ≈ 119 ns | Gap between one FET turning off and its partner turning on, so both never conduct at once (shoot-through) |
| `MasterOutputTrigger` | `TRGO_UPDATE` | Each update event sends a trigger signal to the ADCs |
| `OffStateRunMode/IdleMode` | enabled | When outputs are disabled, the pins are driven to a defined "off" level instead of floating |
| `AutomaticOutput` | disabled | Outputs stay off after a break until software re-enables them |

Why repetition counter = 2? It gives 3 events per update. **3 is odd**, so consecutive updates alternate: bottom, top, bottom, top … Each update therefore lands at the opposite end of the triangle from the one before. Part 4 relies on this.

So: the PWM switches at 24 kHz, but an update happens every 3 half-periods = 62.5 µs, and a full bottom-to-bottom cycle takes 125 µs = **8 kHz control rate**.

**ADC triggers, `adc.c`:**

| ADC | Injected channel (triggered by **TIM1** update) | Regular channel (triggered by **TIM8** update) |
|---|---|---|
| ADC1 | Vbus voltage | — |
| ADC2 | M0 phase B current | M1 phase B current |
| ADC3 | M0 phase C current | M1 phase C current |

Each ADC does double duty: its *injected* channel samples motor 0 when TIM1 updates, and its *regular* channel samples motor 1 when TIM8 updates. Because the trigger comes from the timer hardware, **the sample is taken at exactly the right moment** with no software delay.

**For us:** our pin plan already has `CURR_A/B/C` on ADC1/2/3 (PA0, PC3, PE9). Trigger all three plus Vbus from **TIM1 TRGO** (on the G474, use TRGO2 for finer control). With one motor we only need the injected channels.

### Lines 308–328: interrupt priorities

```cpp
HAL_NVIC_SetPriority(EXTI0_IRQn ... EXTI15_10_IRQn, 1, 0);  // pin-change interrupts (step/dir, index pulse)
HAL_NVIC_SetPriority(ControlLoop_IRQn, 5, 0);               // control loop
HAL_NVIC_SetPriority(TIM8_UP_TIM13_IRQn, 0, 0);             // timer update: HIGHEST
```

On Cortex-M, **a lower number = higher priority**. So:

| Priority | Interrupt | Why |
|---|---|---|
| **0** (highest) | TIM8 update | Must never be delayed: it timestamps samples and starts each cycle |
| 1 | Pin changes | Step/dir pulses and encoder index must not be missed |
| **5** | Control loop | Long-running maths; may be interrupted by the two above |
| (lower) | FreeRTOS threads, USB, UART, CAN | Everything else |

FreeRTOS only allows its API to be called from interrupts at priority 5 or lower (numerically ≥ 5). That's why the control loop sits at 5 and the comments elsewhere say "no FreeRTOS calls in the timer interrupt".

**For us:** same structure: timer update at 0, control loop below it, CAN below that.

### Lines 330–361: optional communication ports

Starts UART A/B, I²C and CAN only if the user's saved configuration enables them. For I²C it reads three pins to build the bus address. For CAN it checks the pins are set to CAN mode and flags `misconfigured_` if not.

**For us:** CAN-FD is always on, so we simply initialise FDCAN1. A UART for the HC-05 debug link.

### Lines 363–366: freeze PWM when the debugger halts — **important**

```cpp
__HAL_DBGMCU_FREEZE_TIM1();
__HAL_DBGMCU_FREEZE_TIM8();
__HAL_DBGMCU_FREEZE_TIM13();
```

When you pause the CPU at a breakpoint, the timers normally **keep running**. The PWM would keep switching with whatever duty cycle was last written, with no control loop watching the current. That can burn the motor or the FETs.

These lines stop the timers whenever the debugger halts the core. Combined with the off-state settings above, the outputs go to a safe state.

**For us: copy this exactly** (`__HAL_DBGMCU_FREEZE_TIM1()` exists on the G474 too). It's a one-liner that prevents real damage during bring-up.

### Lines 368–377: reset the gate drivers

```cpp
drv_enable_gpio.write(false);
delay_us(40);        // ≥ 20 µs low = full reset of the DRV8301
drv_enable_gpio.write(true);
delay_us(20000);     // wait 20 ms for the chip to wake up
```

Pulsing the enable pin low fully resets the DRV8301 (its SPI registers too). Then it waits for the chip to be ready before any SPI configuration. (The second comment is a copy-paste error; the 20 ms is a wake-up wait.)

**For us:** the DRV8353S has its own reset and wake-up timing on `ENABLE`. Take the numbers from its datasheet. After this, write its registers over SPI: **6x PWM mode** (README note), IDRIVE, VDS overcurrent level, current-sense gain.

---

## Part 3: Starting the timers in sync (lines 380–416)

```cpp
void start_timers() {
    CRITICAL_SECTION() {                                       // interrupts off
        hadc1.Instance->CR2 &= ~(ADC_CR2_JEXTEN);              // 1. disconnect ADC triggers
        hadc2.Instance->CR2 &= ~(ADC_CR2_EXTEN | ADC_CR2_JEXTEN);
        hadc3.Instance->CR2 &= ~(ADC_CR2_EXTEN | ADC_CR2_JEXTEN);

        Stm32Timer::start_synchronously<3>(                    // 2. start timers together
            {&htim1, &htim8, &htim13},
            {TIM1_INIT_COUNT, 0, TIM1_INIT_COUNT / 2});

        hadc1.Instance->CR2 |= (...RISING);                    // 3. reconnect ADC triggers
        ...
        __HAL_ADC_CLEAR_FLAG(...JEOC / EOC / OVR);             // 4. clear stale flags
        __HAL_TIM_CLEAR_IT(&htim8, TIM_IT_UPDATE);             // 5. enable the heartbeat
        __HAL_TIM_ENABLE_IT(&htim8, TIM_IT_UPDATE);
    }
}
```

Step by step:

1. **Disconnect the ADC triggers**, so starting the timers doesn't fire a stray conversion.
2. **Start the timers together.** `start_synchronously` (in `Drivers/STM32/stm32_timer.hpp`) stops each timer, forces an update so all registers load, sets each counter to a chosen starting value, and then sets the enable bits in three back-to-back store instructions with interrupts off.
   - TIM1 starts at `TIM1_INIT_COUNT` = 3500/2 − 128 = 1622; TIM8 starts at 0. So **TIM1's triangle runs about a quarter-period ahead of TIM8's** (≈ 90°).
   - Why? The two motors then **don't switch at the same instant**. That spreads the current drawn from the DC bus and means one motor's switching noise doesn't land on the other motor's current sample.
   - The 128-count offset is an unexplained tuning value (the source has "TODO: explain why this offset").
   - TIM13 runs at the control-loop period. Nothing else in this firmware version reads it; it appears to be a leftover.
3. **Reconnect the ADC triggers** on the rising edge of the timer trigger.
4. **Clear stale ADC flags** (end-of-conversion and overrun), so the first check in Part 4 doesn't see old data.
5. **Enable the TIM8 update interrupt.** From now on the heartbeat in Part 4 runs.

**For us:** much simpler with one timer. Start TIM1 alone, with ADC triggers connected after it starts. The "disconnect triggers, start, clear flags, then enable the interrupt" order is worth keeping.

---

## Part 4: The real-time heartbeat (lines 418–571)

This is the core of ODrive. Everything in the motor-control code is called from here.

### The timeline of one control cycle (125 µs)

```
TIM1/TIM8 triangle:   /\/\/\/\/\/\   (one /\ = 41.7 µs at 24 kHz)
update events:        B     T     B     T          (B = bottom, T = top; every 3 half-periods)
                      |     |     |
ADC samples:          |     |     └─ next cycle
                      |     └─ zero-current sample (high-sides on) → offset calibration
                      └─ real current sample (low-sides on, current in shunts)

TIM8 update ISR @B:   timestamp, start encoder read, trigger ControlLoop
ControlLoop ISR:      read currents → run all control → wait for T sample
                      → offset calibration → compute PWM → write CCR registers
TIM8 update ISR @T:   preload 50 % duty (safety net)
next B:               new CCR values take effect
```

### Lines 418–451: `fetch_and_reset_adcs()` — reading the samples

```cpp
bool all_adcs_done = (ADC1->SR & ADC_SR_JEOC) == ADC_SR_JEOC
    && (ADC2->SR & (ADC_SR_EOC | ADC_SR_JEOC)) == (ADC_SR_EOC | ADC_SR_JEOC)
    && (ADC3->SR & (ADC_SR_EOC | ADC_SR_JEOC)) == (ADC_SR_EOC | ADC_SR_JEOC);
if (!all_adcs_done) return false;
```

**Check every conversion has finished.** `JEOC` = "injected end of conversion" (motor 0 + Vbus); `EOC` = "regular end of conversion" (motor 1). If any is missing, the sample timing is broken. The caller then disarms with `ERROR_BAD_TIMING`.

```cpp
vbus_sense_adc_cb(ADC1->JDR1);
```

**Bus voltage.** Converts the raw count to volts: `count × 3.3 V × divider ratio / 4096` (`low_level.cpp:315`). Stored in the global `vbus_voltage` used everywhere else.

```cpp
if (m0_gate_driver.is_ready()) {
    std::optional<float> phB = motors[0].phase_current_from_adcval(ADC2->JDR1);
    std::optional<float> phC = motors[0].phase_current_from_adcval(ADC3->JDR1);
    if (phB.has_value() && phC.has_value()) {
        *current0 = {-*phB - *phC, *phB, *phC};
    }
}
```

**Motor 0 currents.**
- Only if the gate driver is configured. Otherwise its amplifier output isn't valid.
- `phase_current_from_adcval` (`motor.cpp:397`) does:
  1. **Saturation check.** If the reading is too close to 0 or 4095, the amplifier is clipping, so the true current is unknown. It returns "no value" and sets `ERROR_CURRENT_SENSE_SATURATION`.
  2. Subtract 2048 (mid-scale; the amplifier output is centred so it can read both directions of current).
  3. Count → amplifier volts → divide by amplifier gain → shunt volts → × conductance → **amps**.
- **Phase A = −B − C.** By Kirchhoff's current law the three phase currents sum to zero, so two measurements are enough.
- `std::optional` means "a value, or nothing". If either phase is invalid, `current0` stays empty, and the motor code later disarms with "unknown current". **A missing measurement is never silently replaced with a guess.**

```cpp
if (m1_gate_driver.is_ready()) { ... ADC2->DR ... ADC3->DR ... }
```

**Motor 1**, the same thing from the *regular* data registers (`DR`).

```cpp
ADC1->SR = ~(ADC_SR_JEOC);
ADC2->SR = ~(ADC_SR_EOC | ADC_SR_JEOC | ADC_SR_OVR);
ADC3->SR = ~(ADC_SR_EOC | ADC_SR_JEOC | ADC_SR_OVR);
return true;
```

**Clear the flags** so the next call can tell new data from old. Writing 0 to a status bit clears it; writing 1 leaves it alone, hence the `~`.

**For us:**
- Same structure, but we read **three** phases. Two good options: (a) use all three and check that they sum to about zero, which catches a broken sensor; or (b) at high duty cycle, a phase's low-side is on only briefly and its sample is unreliable, so use the two phases with the longest low-side on-time and calculate the third.
- Keep the saturation check and the "no value → disarm" rule.

### Lines 453–472: SPI and PWM-input callbacks

```cpp
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi == &hspi3) spi3_arbiter.on_complete();
}
void TIM5_IRQHandler(void) { pwm0_input.on_capture(); }
```

When an SPI transfer (encoder read or gate-driver register access) finishes, the HAL calls this, and the arbiter starts the next queued transfer. TIM5 captures RC-PWM input edges.

**For us:** an SPI-complete callback for the MA702 read (store the new angle and mark it fresh). No PWM input.

### Lines 474–475: heartbeat state

```cpp
volatile uint32_t timestamp_ = 0;
volatile bool counting_down_ = false;
```

- `timestamp_`: a running clock in **timer ticks** (1/168 MHz). Advanced by a fixed amount at every update event. Every measurement and every PWM output is labelled with this, so later code knows exactly *when* things happened.
- `counting_down_`: the timer direction at the last update. Used to detect a missed update.
- `volatile`: tells the compiler these change inside interrupts, so it must re-read them from memory every time.

### Lines 477–516: `TIM8_UP_TIM13_IRQHandler` — the heartbeat (priority 0)

Runs at every TIM8 update event: every 62.5 µs, alternating bottom and top.

```cpp
__HAL_TIM_CLEAR_IT(&htim8, TIM_IT_UPDATE);
```

Clear the interrupt flag first, so the interrupt doesn't fire again immediately.

```cpp
bool counting_down = TIM8->CR1 & TIM_CR1_DIR;
```

**Where are we on the triangle?** The direction bit tells us:
- **Counting up** → we just passed the **bottom**. The current sample (low-sides on) was just taken.
- **Counting down** → we just passed the **top**. The zero-current sample (high-sides on) was just taken.

```cpp
bool timer_update_missed = (counting_down_ == counting_down);
if (timer_update_missed) {
    motors[0].disarm_with_error(Motor::ERROR_TIMER_UPDATE_MISSED);
    motors[1].disarm_with_error(Motor::ERROR_TIMER_UPDATE_MISSED);
    return;
}
counting_down_ = counting_down;
```

**Missed-interrupt detector.** Updates must alternate bottom/top/bottom (that's why the repetition counter gives an odd number, see Part 2). If the direction is the *same* as last time, one update interrupt was skipped. Every timestamp from now on would be wrong, so **both motors are disarmed immediately**. Cheap and clever.

```cpp
timestamp_ += TIM_1_8_PERIOD_CLOCKS * (TIM_1_8_RCR + 1);
```

**Advance the clock** by one update interval: 3500 × 3 = 10,500 ticks = 62.5 µs.

```cpp
if (!counting_down) {
    TaskTimer::enabled = odrv.task_timers_armed_;
    odrv.sampling_cb();
    NVIC->STIR = ControlLoop_IRQn;
}
```

**At the bottom (start of a cycle):**
1. Enable the profiler for this cycle if someone requested it.
2. `sampling_cb()` (`main.cpp:341`): **latch the encoder now**, at the same moment as the current sample. For an incremental encoder it copies the counter; for SPI encoders it starts the transfer. Current and angle must be measured together, or the Park transform uses an angle that doesn't match the current.
3. **Trigger the ControlLoop interrupt in software** (`STIR` = Software Trigger Interrupt Register). It runs as soon as this priority-0 handler returns.

Why split into two interrupts? This handler must be **short and never late**: it timestamps the cycle and catches missed updates. The control maths takes tens of microseconds. Running it at a lower priority lets the next timer update interrupt *preempt* it, which is exactly how the deadline check below works.

```cpp
} else {
    TIM1->CCR1 = TIM1->CCR2 = TIM1->CCR3 =
    TIM8->CCR1 = TIM8->CCR2 = TIM8->CCR3 = TIM_1_8_PERIOD_CLOCKS / 2;
}
```

**At the top (middle of a cycle): a safety net.** Set every phase to 50 % duty. Equal duty on all three phases puts **zero voltage** across the motor.

The compare registers are *preloaded*: values written now only take effect at the next update (the next bottom). Normally the control loop writes the real duty cycles *after* this, overwriting the 50 %. If the control loop is late, the 50 % values go live instead, and the motor gets zero voltage rather than stale values from the previous cycle.

### Lines 518–571: `ControlLoop_IRQHandler` — one control cycle (priority 5)

```cpp
uint32_t timestamp = timestamp_;
```

Take a **local copy** of the clock. The global keeps changing (the heartbeat can interrupt this handler); the local copy marks "this cycle".

```cpp
std::optional<Iph_ABC_t> current0, current1;
if (!fetch_and_reset_adcs(&current0, &current1)) {
    motors[0].disarm_with_error(Motor::ERROR_BAD_TIMING);
    motors[1].disarm_with_error(Motor::ERROR_BAD_TIMING);
}
```

**Read the real-current samples** (taken at the bottom). If any ADC isn't finished, the timing is broken → disarm.

```cpp
if (!(TIM1->BDTR & TIM_BDTR_MOE_Msk)) current0 = {0.0f, 0.0f};
if (!(TIM8->BDTR & TIM_BDTR_MOE_Msk)) current1 = {0.0f, 0.0f};
```

**If the PWM outputs are off** (`MOE` = Main Output Enable is clear), the low-side FETs aren't conducting, so the shunts carry no current and the reading is meaningless. Report 0 A instead. The comment admits this is wrong right after disarming or while the motor freewheels fast, but it lets FOC start from a clean state.

(This line sets only two of the three fields; it relies on brace-initialisation zeroing the third.)

```cpp
motors[0].current_meas_cb(timestamp - TIM1_INIT_COUNT, current0);
motors[1].current_meas_cb(timestamp, current1);
```

**Hand the currents to each motor**, labelled with *when* they were sampled. Motor 0's timestamp is shifted back by `TIM1_INIT_COUNT` because TIM1 runs ahead of TIM8 (Part 3). Inside (`motor.cpp:600`): subtract the offset, check overcurrent, check bus voltage, pass to FOC.

```cpp
odrv.control_loop_cb(timestamp);
```

**Run all the control** (`main.cpp:364`): encoder update, position/velocity loops, torque → current. Covered in the next module.

```cpp
MEASURE_TIME(odrv.task_times_.dc_calib_wait) {
    while (!(ADC2->SR & ADC_SR_EOC));
}
if (!fetch_and_reset_adcs(&current0, &current1)) { ... disarm BAD_TIMING ... }
```

**Wait for the top sample.** By now the timer should have reached the top, and the ADCs sampled with all high-sides on, so no shunt current. The loop busy-waits for the conversion to finish (and the profiler measures how long that wait was, which shows how much spare time there is).

```cpp
motors[0].dc_calib_cb(timestamp + TIM_1_8_PERIOD_CLOCKS * (TIM_1_8_RCR + 1) - TIM1_INIT_COUNT, current0);
motors[1].dc_calib_cb(timestamp + TIM_1_8_PERIOD_CLOCKS * (TIM_1_8_RCR + 1), current1);
```

**Offset calibration.** Since no current flows through the shunts at this moment, whatever the ADC reads is the amplifier's **offset error**. `dc_calib_cb` (`motor.cpp:667`) slowly averages it (time constant 0.2 s). The current readings at the bottom have this offset subtracted. This runs continuously, so offset drift with temperature is tracked. The timestamp is one update interval later than the cycle start (that's when the top sample was taken).

```cpp
motors[0].pwm_update_cb(timestamp + 3 * TIM_1_8_PERIOD_CLOCKS * (TIM_1_8_RCR + 1) - TIM1_INIT_COUNT);
motors[1].pwm_update_cb(timestamp + 3 * TIM_1_8_PERIOD_CLOCKS * (TIM_1_8_RCR + 1));
```

**Compute and write the PWM.** `pwm_update_cb` (`motor.cpp:686`) runs the FOC current controller and SVM, then writes the three compare registers.

The timestamp is **3 update intervals ahead** of the cycle start. The new duty cycles take effect at the next bottom (+2 intervals) and stay until the bottom after that (+4), so +3 is the **middle of the time the voltage is actually applied**. FOC uses this to rotate the voltage vector forward by `speed × time` so it points the right way *when it is applied*, not when it was computed. At high electrical speed this correction matters.

```cpp
if (timestamp_ != timestamp + TIM_1_8_PERIOD_CLOCKS * (TIM_1_8_RCR + 1)) {
    motors[0].disarm_with_error(Motor::ERROR_CONTROL_DEADLINE_MISSED);
    motors[1].disarm_with_error(Motor::ERROR_CONTROL_DEADLINE_MISSED);
}
```

**Deadline check.** During this handler, exactly one heartbeat should have happened (the top one). So the global clock should be exactly one interval ahead of our local copy.
- **More than one interval:** we overran into the next cycle. The PWM values we just wrote may have arrived too late → disarm.
- **Zero intervals:** we finished before the top sample, which the busy-wait makes impossible.

```cpp
odrv.task_timers_armed_ = odrv.task_timers_armed_ && !TaskTimer::enabled;
TaskTimer::enabled = false;
```

Profiler housekeeping: a profiling request covers one cycle, then switches itself off.

### Lines 573–591: I²C and USB interrupts

Standard handlers that pass the interrupt to the HAL. Not relevant for us.

---

## Summary: what to take from `board.cpp`

| Idea | Lines | Take it? |
|---|---|---|
| Static allocation of all objects | 1–155 | **Yes** |
| Timer-triggered ADC sampling at the bottom of the triangle | `tim.c`, `adc.c` | **Yes**, the basis of current sensing |
| Second sample at the top for continuous offset calibration | 548–560 | **Yes** (handles README N11) |
| Repetition counter with an odd count, so updates alternate bottom/top | `tim.c:80` | **Yes**, it enables the missed-update check |
| Freeze PWM timers on debugger halt | 363–366 | **Yes, copy exactly** |
| Gate-driver reset pulse on enable | 368–377 | Yes, with DRV8353S timings |
| Two interrupts: short heartbeat at priority 0 + control loop at lower priority | 477–571 | **Yes** |
| Missed-update detection from the counting direction | 487–493 | **Yes** |
| Timestamps on samples and outputs, for latency compensation | 474–571 | **Yes** |
| 50 % duty safety net at the top event | 503–515 | **Yes** |
| Deadline check at the end of the cycle | 568–571 | **Yes** |
| "No measurement → disarm", never guess | 418–451, 526–529 | **Yes** |
| Phase A = −B − C | 434 | Adapt: we have three shunts |
| Second motor, TIM8, 90° offset between timers | 394–397 | No |
| OTP version check, fake OTP | 30, 266–290 | Optional |
| Shared-SPI arbiter | 32 | No: one device per bus |
| Run-time GPIO function tables, PWM input, USB, I²C | 159–264, 573–591 | No |

**Our version** of this file (call it `board.c` + `isr.c`) should come to roughly **200–300 lines**: one motor object, the hardware init, a debugger freeze, the DRV8353S reset, starting TIM1, and the two interrupts with all their safety checks.

**Next module:** `MotorControl/main.cpp`, specifically `control_loop_cb()`, which decides the order everything runs in each cycle.
