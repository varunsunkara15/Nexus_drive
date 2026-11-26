# How the ODrive firmware works: a study for Nexus Drive

This is step one of writing our own firmware. Before we cut features, we need to know what ODrive does, where it does it, and why.

**Source studied:** [odriverobotics/ODrive](https://github.com/odriverobotics/ODrive), commit `3a2e4dd` (MIT licence). This is the open-source **v0.5.x firmware for the ODrive v3.x board** (STM32F405, two motors, DRV8301 gate drivers). The newer ODrive Pro/S1 firmware is closed source. All file paths below are relative to `Firmware/` in that repository.

**Size:** about 14,500 lines across `MotorControl/`, `communication/` and `Board/v3/`. The core of FOC is fewer than 1,500 of those lines. The rest is configuration, protocols, calibration routines and alternative modes.

---

## 1. The big picture

ODrive is built from three layers:

```
┌─────────────────────────────────────────────────────────────┐
│  Communication (FreeRTOS threads, low priority)             │
│  USB (Fibre), UART ASCII, CAN, step/dir, PWM input          │
│  → writes setpoints and config, reads telemetry             │
├─────────────────────────────────────────────────────────────┤
│  Axis state machine (one FreeRTOS thread per motor)         │
│  IDLE → MOTOR_CALIBRATION → ENCODER_OFFSET_CALIBRATION      │
│       → CLOSED_LOOP_CONTROL → IDLE                          │
│  → decides WHICH control law is connected and armed         │
├─────────────────────────────────────────────────────────────┤
│  Real-time control (interrupts, 8 kHz, highest priority)    │
│  ADC → current → FOC → SVM → PWM registers                  │
│  encoder → PLL → pos/vel → pos/vel/torque cascade           │
│  → all of the actual motor control                          │
└─────────────────────────────────────────────────────────────┘
```

The key point: **all control math runs in interrupts, not in threads.** Threads only change *modes* and *setpoints*. Our firmware should keep that split.

---

## 2. Timing: what runs when

ODrive v3 runs TIM1 and TIM8 (one per motor) in **center-aligned PWM** mode:

| Constant (`Board/v3/Inc/main.h`, `board.h`) | Value | Meaning |
|---|---|---|
| `TIM_1_8_CLOCK_HZ` | 168 MHz | Timer clock |
| `TIM_1_8_PERIOD_CLOCKS` | 3500 | Counts per half-period → PWM = 168 MHz / (2 × 3500) = **24 kHz** |
| `TIM_1_8_RCR` | 2 | Repetition counter: update event every 3 half-periods |
| `CURRENT_MEAS_HZ` | 168 MHz / (2 × 3500 × 3) = **8 kHz** | Control loop rate (`current_meas_period` = 125 µs) |

So the PWM switches at 24 kHz, but currents are sampled and the control loop runs at 8 kHz.

The ADCs are triggered by the timer at the moment when all three **low-side FETs are on** (the middle of SVM vector V0). Only then does current flow through the low-side shunts. On the opposite half-cycle (V7, all high-sides on), no current flows through the shunts. That second sample is used to measure the amplifier **DC offset** while the motor runs.

### Interrupt chain (`Board/v3/board.cpp`)

```
TIM8 update IRQ  (priority 0, board.cpp:477)
 ├─ counting up   → sampling_cb(): latch encoder count/start SPI read
 │                → trigger software IRQ "ControlLoop"
 └─ counting down → preload CCRx = 50 % (safe default if the loop overruns)

ControlLoop IRQ  (lower priority, board.cpp:518)
 1. fetch_and_reset_adcs()          read Vbus + phase B, C currents; A = -B - C
 2. motor.current_meas_cb()         subtract offset, overcurrent trip, Vbus check,
                                    hand currents to FOC (Clarke transform)
 3. odrv.control_loop_cb()          main.cpp:364 — encoder.update(), controller.update(),
                                    motor.update(), current_control.update()
 4. wait for the second ADC sample
 5. motor.dc_calib_cb()             low-pass the zero-current offset (tau = 0.2 s)
 6. motor.pwm_update_cb()           FOC PI → inverse Park → SVM → write TIM CCR1..3
 7. deadline check                  if TIM8 updated more than once meanwhile
                                    → ERROR_CONTROL_DEADLINE_MISSED, disarm
```

**Timestamps.** Every measurement carries a timestamp in timer ticks. FOC uses it to rotate the electrical angle forward to the moment the current was sampled (`foc.cpp:100`) and to the middle of the next PWM period (`foc.cpp:163`). This *latency compensation* matters at high electrical speed. The RO100 is a high-pole-count motor, so its electrical frequency is high for a given shaft speed, which makes this worth keeping.

---

## 3. The control cascade (signal flow in closed loop)

```
input_pos/vel/torque  (from CAN/USB)
      │
      ▼
[Input mode]  controller.cpp:182   passthrough / vel ramp / pos filter / trap traj ...
      │  pos_setpoint, vel_setpoint (feed-forward), torque_setpoint (feed-forward)
      ▼
[Position P]  vel_des = vel_ff + pos_gain · (pos_set − pos_est)
      ▼
[Velocity PI] torque = torque_ff + vel_gain · v_err + ∫ vel_integrator_gain · v_err
      │       (clamp to vel_limit, torque limit; integrator decays when saturated)
      ▼
[Motor]       motor.cpp:513   Iq = torque / Kt ;  Id = 0 (PMSM)
      │       circular current limit |Idq| ≤ I_lim
      │       optional feed-forward: Vd,Vq = R·I − ωL·Iq … + back-EMF
      ▼
[FOC current PI]  foc.cpp:58
      │   Clarke: Iα = Ia,  Iβ = (Ib − Ic)/√3
      │   Park:   Id, Iq from Iα, Iβ and θ_e
      │   PI on d and q:  V = Vff + Kp·e + ∫Ki·e
      │   saturate at 80 % of max modulation (integrator decays)
      │   Inverse Park → mod_α, mod_β
      ▼
[SVM]  utils.cpp:10   → duty tA, tB, tC ∈ [0,1]
      ▼
TIM1 CCR1/2/3
```

Each stage is **optional**. `control_mode` decides where the cascade starts. In torque mode, the position and velocity stages are skipped.

### Current-loop gains are calculated, not tuned (`motor.cpp:297`)

```
Kp = current_control_bandwidth · L          (default bandwidth 1000 rad/s)
Ki = (R / L) · Kp
```

This is pole-zero cancellation: the PI zero cancels the motor's R/L pole, so the closed loop becomes a first-order system with the chosen bandwidth. **This is why ODrive measures R and L at calibration.** We should keep this approach.

### Velocity/position estimate: a PLL (`encoder.cpp:651`)

The encoder count is never differentiated directly. A second-order PLL tracks it:

```
pos_est += dt · vel_est
err      = measured_count − floor(pos_est)
pos_est += dt · Kp · err          Kp = 2 · bandwidth
vel_est += dt · Ki · err          Ki = Kp² / 4   (critically damped)
```

This gives smooth velocity and sub-count position interpolation. The electrical angle is `θ_e = 2π · pole_pairs · (count − phase_offset) / CPR`.

---

## 4. Module by module

| File | What it does | Lines |
|---|---|---|
| `Board/v3/board.cpp` | Pin/peripheral setup, the two IRQ handlers above, ADC readout, board constants | 593 |
| `MotorControl/main.cpp` | `ODrive` object, startup, `control_loop_cb`, Vbus over/under-voltage checks | 846 |
| `MotorControl/axis.cpp` | Per-motor state machine thread, watchdog, homing, lock-in spin | 602 |
| `MotorControl/motor.cpp` | Arm/disarm, R and L measurement, torque→current, current limits, ADC callbacks, PWM write | 733 |
| `MotorControl/foc.cpp` | Clarke/Park, dq current PI, inverse Park, SVM call | 193 |
| `MotorControl/controller.cpp` | Input modes, position P, velocity PI, limits, anticogging, spinout detection | 451 |
| `MotorControl/encoder.cpp` | Incremental/hall/sin-cos/SPI-absolute decoding, PLL, offset calibration | 842 |
| `MotorControl/open_loop_controller.cpp` | Ramps a rotating current vector without feedback (used for calibration) | small |
| `MotorControl/component.hpp` | `OutputPort`/`InputPort`: wires modules together and invalidates stale data each cycle | 181 |
| `MotorControl/utils.cpp` | SVM, fast sin/cos/atan2, wrapping helpers | 190 |
| `MotorControl/low_level.cpp` | Brake resistor, analog polling thread (thermistors, GPIO ADC) | 411 |
| `MotorControl/sensorless_estimator.cpp` | Flux observer for running without an encoder | 108 |
| `MotorControl/acim_estimator.cpp` | Induction motor slip model | — |
| `MotorControl/trapTraj.cpp` | Trapezoidal trajectory planner | — |
| `communication/` | Fibre over USB, ASCII over UART, CANSimple, I²C | ~1,500 |

### The "port" pattern (`component.hpp`)

Modules don't call each other directly. Each one has `OutputPort<T>` values (e.g. `encoder_.phase_`) and `InputPort<T>` references (e.g. `current_control_.phase_src_`). At the start of every loop iteration, `control_loop_cb` **resets all output ports**. A consumer calls `.present()` and gets `std::nullopt` if the producer didn't update this cycle, which turns into an error and a disarm.

This is how ODrive swaps sources at run time. During encoder calibration, FOC's angle comes from the open-loop controller. In closed loop, it comes from the encoder. Sensorless mode connects the flux observer instead. **We only have one source of each signal, so we can use plain function calls and structs.** The "no stale data" safety idea is still worth keeping, for example as a sequence counter on the encoder sample.

### Arming and disarming (`motor.cpp:195`, `motor.cpp:266`)

- **Arm:** select the control law, reset integrators, then set `BDTR.AOE` so the timer's main output (MOE) turns on at the next update event. The outputs only turn on in sync with the PWM period.
- **Disarm:** disable the gate driver, clear `MOE` so all FETs go off. This is called from *everywhere*: about 20 different error paths.
- **Hardware break:** the gate driver fault pin is routed to the timer's BKIN input, so the hardware shuts off the PWM without any software involved. We do the same thing with `DRV_nFAULT` on PB12 = TIM1_BKIN.

### Safety checks (what trips a disarm)

| Check | Where | When |
|---|---|---|
| Phase current magnitude > limit + margin | `motor.cpp:600` | every sample (8 kHz) |
| Vbus outside under/over-voltage limits | `main.cpp:300` | every sample |
| Current sample missing or ADC not finished | `board.cpp:526` | every sample |
| Control loop overran its deadline / timer update missed | `board.cpp:489`, `:568` | every sample |
| DC bus current outside `I_bus_hard_min/max` (limits regen) | `motor.cpp:686` | every sample |
| PWM modulation NaN or too large | `foc.cpp:31` | every sample |
| Encoder SPI error rate > 5 % | `encoder.cpp` | every sample |
| Overspeed, spinout (encoder offset wrong) | `controller.cpp` | every sample |
| Communication watchdog not fed | `axis.cpp:171` | every sample |
| FET / motor thermistor over-temperature | `thermistor.cpp` | every sample |

### Calibration (run once, results saved to flash)

1. **Phase resistance** (`motor.cpp:416`): a PI loop drives a DC test current (default 10 A) along the α axis, capped at 2 V. Then R = V / I. It also checks that Iβ stays small; otherwise the phases are unbalanced (a broken wire).
2. **Phase inductance** (`motor.cpp:457`): alternates ±V every cycle and measures di/dt. L = V / (di/dt).
3. **Current gains** are calculated from R and L (section 3).
4. **Encoder offset** (`encoder.cpp:328`): with the open-loop controller, holds a current vector for 1 s, then rotates it 16π electrical forward and back at 4π rad/s. It checks direction, checks that CPR matches the pole-pair count, and averages the encoder reading to find `phase_offset` (the encoder count at electrical angle zero).

With an **absolute** encoder (our MA702), this offset is stable across power cycles. Save it once and skip encoder calibration at boot (`pre_calibrated`).

### Current offset calibration (always running)

`dc_calib_cb` (`motor.cpp:667`) samples current at the V7 vector (no shunt current) and low-pass filters it with τ = 0.2 s. Current readings are invalid until 7.5 τ (1.5 s) after the PWM starts. This takes care of README item **N11**: VREF on `+3V` but the ADC on `+3VA`. Any offset between the two rails is measured and subtracted continuously.

---

## 5. Keep, adapt or drop for Nexus Drive

Our case: **one motor** (RO100), **one absolute SPI encoder** (MA702GQ), **three shunts** (ODrive v3 has two), **CAN-FD** to the robot's main controller, joint torque/position control for a humanoid.

### Keep (the core; rewrite it in our own style)

| Feature | Why |
|---|---|
| Timer-synchronised ADC sampling at V0, offset sampling at V7 | Basis of clean current measurement |
| Clarke/Park, dq PI with anti-windup, inverse Park, SVM | The FOC itself |
| PI gains from R, L and bandwidth | No manual current-loop tuning |
| Latency compensation with timestamps | High electrical frequency on a high-pole-count motor |
| PLL for position/velocity | Smooth velocity from a quantised sensor |
| Position P → velocity PI → torque cascade with feed-forward | Standard joint control; humanoid controllers usually send pos + vel + torque_ff |
| Torque → Iq via Kt, circular current limit | |
| R/L measurement and encoder offset calibration | Needed once per motor build |
| Arm/disarm with MOE/AOE, hardware break input | Safety |
| All "every sample" checks in the table above | Safety. Over-voltage and bus-current limits matter even more for us (README **N23**: 80 V FETs, regen limiting in firmware) |
| Deadline / missed-update checks | Detects a control loop that's too slow |
| Communication watchdog | A joint must stop if the main controller goes silent |
| Thermistor monitoring | We have FET and motor thermistors |
| Config struct saved to flash | Calibration results, gains, limits |

### Adapt

| ODrive | Nexus Drive |
|---|---|
| 2 shunts, phase A = −B − C | 3 shunts. Use all three, or pick the two phases with the **longest low-side on-time** at high modulation (a common trick) |
| DRV8301 via SPI | DRV8353S via SPI4: set **6x PWM mode**, IDRIVE, VDS limits, CSA gain; read fault registers |
| Generic SPI encoder modes (`MODE_SPI_ABS_MA732` is the closest to ours) | One MA702 driver: start the SPI read in the sampling ISR, use the result in the next cycle |
| TIM1 + TIM8, two motors | TIM1 only |
| CANSimple (CAN 2.0, 8 bytes) | CAN-FD with a compact message set: one command frame (pos, vel, torque_ff, Kp, Kd) and one status frame per cycle |
| 8 kHz loop, 24 kHz PWM | The G474 at 170 MHz with an FPU and CORDIC can do 20–40 kHz. Pick it together with the gate-drive budget (README **N10**) |
| Brake resistor support | We have none: instead, limit regen current from the Vbus reading, and short the motor (all low-sides on) on overvoltage at speed (see `power-stage.md`) |
| FreeRTOS threads | Probably not needed. A bare-metal main loop (state machine + CAN handling) plus the two ISRs is simpler and easier to reason about |

### Drop

| Feature | Reason |
|---|---|
| Second axis | One motor per board |
| Fibre/USB protocol, ASCII UART protocol, Python tool integration | We use CAN-FD. Keep the UART only as a simple debug console (HC-05) |
| Port/component framework | One source per signal → direct calls |
| Hall, incremental, sin/cos encoder modes, index search | MA702 only |
| Sensorless estimator, ACIM estimator, gimbal motor type | PMSM with an encoder |
| Step/dir, PWM input, I²C, endstops, homing, mechanical brake | Not used in a joint driven over CAN |
| Trapezoidal trajectories, input filters, mirror mode, autotuning | The robot's main controller plans motion |
| Anticogging (for now) | Possible later, if torque ripple matters |
| Oscilloscope, task timers | Replace with a minimal debug-telemetry buffer |
| Brake resistor logic | No brake resistor |

A rough estimate: the parts we keep come to **1,500–2,500 lines of C**, against about 14,500 in ODrive.

---

## 6. Suggested reading order in ODrive's source

1. `Board/v3/board.cpp:477–571`: the two interrupt handlers. Everything starts here.
2. `MotorControl/main.cpp:364`: `control_loop_cb`, the order of updates.
3. `MotorControl/motor.cpp:600–733`: current measurement, offset calibration, PWM output.
4. `MotorControl/foc.cpp`: the whole FOC (193 lines).
5. `MotorControl/utils.cpp:10`: SVM.
6. `MotorControl/motor.cpp:513`: torque → current, feed-forward.
7. `MotorControl/controller.cpp:143`: position/velocity cascade.
8. `MotorControl/encoder.cpp:651`: PLL; `:328` offset calibration.
9. `MotorControl/motor.cpp:416–510`: R and L measurement.
10. `MotorControl/axis.cpp:455`: state machine.

## 7. Proposed build-up order for our firmware

Each step can be tested on the bench before the next one:

1. Clocks, GPIO, status LED, UART debug console.
2. DRV8353S over SPI4: read/write registers, 6x PWM mode, read faults.
3. TIM1 center-aligned 6-PWM with dead time and BKIN on nFAULT. Check on a scope **with no motor** first.
4. ADC triggered from TIM1 (3 currents + Vbus); check the sample timing; offset calibration.
5. MA702 over SPI, read in the ISR; check angle and direction by turning the shaft by hand.
6. Open-loop voltage rotation (slow, low voltage) → R and L measurement → encoder offset calibration.
7. Closed-loop dq current control → torque mode.
8. Velocity and position loops, limits, all safety checks.
9. CAN-FD protocol + watchdog; save configuration to flash.

## Open questions for the team

- **Motor:** the README lists the **RO100 KV55**. Confirm whether the motor is KV50 or KV55, and get its pole-pair count, phase R/L and Kt from the datasheet. Kt ≈ 8.27 / KV (N·m/A) is a usable first estimate.
- **Loop rate and PWM frequency:** decides the CPU budget and gate-drive load.
- **CAN-FD message format:** agree on it with whoever writes the main controller.
