# Flight Computer

A custom STM32-based flight computer that computes real-time attitude, altitude, and
position from raw sensors. Bare-metal C, no RTOS, no Arduino libraries — every driver
written from the datasheet.

**Status:** attitude fusion working on hardware · *last updated 2026-08-26*

## What it does

Runs a 100 Hz fixed-rate control loop that reads a 9-axis IMU, fuses gyroscope and
accelerometer data through a complementary filter, and produces a stable pitch/roll
estimate. A barometer provides altitude, and a GNSS receiver provides position and
absolute time. Telemetry streams over serial at 10 Hz.

## Measured performance

Every number below was measured on the assembled hardware, not taken from a datasheet.

| Metric | Result |
|---|---|
| Loop rate | 100 Hz, hardware-timer driven, 0 overruns |
| Attitude drift (gyro alone) | 117°/min (2.13 dps measured bias) |
| Attitude drift (fused) | < 1° steady-state |
| Filter time constant | 0.49 s |
| Barometric noise floor | 0.8 Pa ≈ 7 cm altitude resolution |
| Flash / RAM used | 37.7 KB / 3.0 KB of 512 KB / 128 KB |

The fused steady-state error matches theory: gyro bias × filter time constant
= 2.13 dps × 0.49 s = 1.04° predicted, 0.98° observed.

## Hardware

| Part | Role | Interface |
|---|---|---|
| STM32F446RE (Nucleo-64) | Cortex-M4F @ 180 MHz, sensor fusion | — |
| TDK ICM-20948 | 9-axis IMU: gyro, accel, magnetometer | I²C @ 0x69 |
| Bosch BMP581 | Barometric pressure / altitude | I²C @ 0x47 |
| Adafruit Ultimate GPS (PA1616D) | GPS + GLONASS position and time | UART, 9600 baud |

Both I²C devices share a single two-wire bus. The GNSS receiver runs on its own
interrupt-driven UART so no NMEA sentences are dropped while the main loop is busy.

## Firmware design

- **Fixed-rate scheduling.** A hardware timer sets an exact 10 ms tick. The loop
  detects and counts its own overruns, so missed deadlines are visible rather than
  silent. Integration math depends on `dt` being exact, not approximate.
- **Rate decoupling.** Sensors are sampled at 100 Hz; telemetry prints at 10 Hz.
  Serial transmission would otherwise consume most of the loop budget.
- **Interrupt-driven UART with error recovery.** A byte-level ISR assembles NMEA
  sentences; an error callback clears overrun/framing flags and re-arms reception,
  so a single line-noise glitch cannot permanently kill the GNSS feed.
- **Race-free buffer handoff.** Completed sentences are copied to the main loop
  inside a critical section, so the ISR cannot overwrite a buffer mid-read.
- **Power-on self-test.** At boot the firmware enumerates the I²C bus, verifies each
  device against its expected chip ID, and reports pass/fail per sensor before
  entering the control loop.
- **Modular drivers.** Each sensor is a self-contained module with its own header;
  `main.c` orchestrates and nothing more.

## Repo layout

    firmware/Core/Src/main.c        control loop, scheduling, fusion
    firmware/Core/Src/bmp581.c      barometer driver
    firmware/Core/Src/icm20948.c    IMU driver (bank-switched register map)
    firmware/Core/Src/gps.c         interrupt-driven NMEA receiver
    firmware/firmware.ioc           STM32CubeMX peripheral configuration
    docs/build-log.md               dated engineering log

## Roadmap

- [x] **Phase 1 — Bring-up:** all three sensors live, verified by chip ID, streaming
- [x] **Phase 2a — Fixed-rate loop:** 100 Hz hardware timing with overrun detection
- [x] **Phase 2b — Attitude fusion:** complementary filter, drift characterized
- [ ] **Phase 2c — Ground station:** live artificial-horizon display
- [ ] **Phase 2d — Logging:** onboard microSD flight recorder
- [ ] **Phase 3 — Custom PCB:** KiCad schematic, layout, fabrication, bring-up
- [ ] **Phase 4 — Telemetry:** 915 MHz downlink, flight test against GPS ground truth

## Build log

Dated engineering notes — what broke, how it was diagnosed, and why each decision was
made: [docs/build-log.md](docs/build-log.md)

## Toolchain

- VS Code + STM32Cube extension (CMake, ARM GCC, ST-LINK/GDB)
- STM32CubeMX for peripheral configuration and code generation
- macOS on Apple Silicon

Build and flash:

    cmake --build build/Debug
    STM32_Programmer_CLI -c port=SWD -w build/Debug/firmware.elf -rst
