# Flight Computer

A custom STM32-based flight computer that computes real-time attitude, altitude, and
position from raw sensors. Bare-metal C, no RTOS, no Arduino libraries. Every driver
written from the datasheet.

**Status:** Phase 2 complete. Attitude and altitude fusion running on hardware.
*Last updated 2026-09-04.*

## Demo

https://github.com/Liam-Hayes8/flight-computer/raw/main/docs/demo.mp4

Live fused attitude estimate driving a Python artificial-horizon ground station at 10 Hz.

## What it does

Runs a 100 Hz fixed-rate control loop that reads a 9-axis IMU, fuses gyroscope and
accelerometer data through a complementary filter, and produces a stable pitch/roll
estimate. A second complementary filter fuses barometric altitude with GNSS altitude,
giving barometer-grade precision anchored to an absolute GPS reference. Telemetry
streams over serial to a ground station that renders an artificial horizon.

## Measured performance

Every number below was measured on the assembled hardware, not taken from a datasheet.

| Metric | Result |
|---|---|
| Loop rate | 100 Hz, hardware-timer driven, 0 overruns |
| Attitude drift, gyro alone | 117 deg/min (2.13 deg/s measured bias) |
| Attitude drift, fused + bias-calibrated | under 0.3 deg steady-state |
| Attitude filter time constant | 0.49 s |
| Barometric noise floor | 0.8 Pa, about 7 cm altitude resolution |
| Barometric weather drift, uncorrected | about 30 m over three days |
| GPS altitude wander, stationary | 1 to 2 m over 20 s |
| Fused altitude spread, stationary | 6 cm, anchored to GPS level |
| Flash / RAM used | 40 KB / 3.1 KB of 512 KB / 128 KB |

The fused attitude error before calibration matched theory: gyro bias times filter
time constant = 2.13 deg/s x 0.49 s = 1.04 deg predicted, 0.98 deg observed.

## Hardware

| Part | Role | Interface |
|---|---|---|
| STM32F446RE (Nucleo-64) | Cortex-M4F @ 180 MHz, sensor fusion | - |
| TDK ICM-20948 | 9-axis IMU: gyro, accel, magnetometer | I2C @ 0x69 |
| Bosch BMP581 | Barometric pressure / altitude | I2C @ 0x47 |
| Adafruit Ultimate GPS (PA1616D) | GPS + GLONASS position and time | UART, 9600 baud |

Both I2C devices share a single two-wire bus. The GNSS receiver runs on its own
interrupt-driven UART so no NMEA sentences are dropped while the main loop is busy.

## Firmware design

- **Fixed-rate scheduling.** A hardware timer sets an exact 10 ms tick. The loop
  detects and counts its own overruns, so missed deadlines are visible rather than
  silent. Integration math depends on dt being exact.
- **Rate decoupling.** Sensors sample at 100 Hz; telemetry prints at 10 Hz. Serial
  transmission would otherwise consume most of the loop budget.
- **Two complementary filters.** Attitude: gyro provides short-term motion, the
  accelerometer slowly corrects drift (0.49 s time constant). Altitude: barometer
  provides short-term motion, GPS slowly corrects the offset (about 100 s time constant,
  so weather drift is tracked but GPS noise is rejected).
- **Startup gyro bias calibration.** 200 samples averaged during a one-second
  stationary window at boot, then subtracted from every reading.
- **I2C bus recovery.** Nine manual clock pulses on SCL before peripheral init free any
  slave left holding SDA low by a reset mid-transaction.
- **Interrupt-driven UART with error recovery.** A byte-level ISR assembles NMEA
  sentences; an error callback clears overrun/framing flags and re-arms reception.
  Completed sentences are handed to the main loop inside a critical section.
- **NMEA parser with checksum validation.** Parses GGA sentences into decimal-degree
  coordinates, altitude, fix quality, and satellite count. Corrupted sentences are
  rejected. Developed and validated with a host-side test suite before touching hardware.
- **Power-on self-test.** Enumerates the I2C bus, verifies each device against its
  expected chip ID, and reports pass/fail per sensor before entering the control loop.
- **Modular drivers.** Each sensor is a self-contained module; main.c orchestrates.

## Repo layout

    firmware/Core/Src/main.c        control loop, scheduling, both fusion filters
    firmware/Core/Src/bmp581.c      barometer driver
    firmware/Core/Src/icm20948.c    IMU driver (bank-switched register map)
    firmware/Core/Src/gps.c         interrupt-driven NMEA receiver
    firmware/Core/Src/gps_parse.c   NMEA GGA parser with checksum validation
    firmware/firmware.ioc           STM32CubeMX peripheral configuration
    ground-station/horizon.py       Python artificial-horizon display
    docs/build-log.md               dated engineering log

## Roadmap

- [x] **Phase 1 - Bring-up:** all three sensors live, verified by chip ID, streaming
- [x] **Phase 2a - Fixed-rate loop:** 100 Hz hardware timing with overrun detection
- [x] **Phase 2b - Attitude fusion:** complementary filter, drift characterized, bias calibrated
- [x] **Phase 2c - Ground station:** live artificial-horizon display
- [x] **Phase 2d - Altitude fusion:** barometer + GPS with slow offset correction
- [ ] **Phase 2e - Logging:** onboard microSD flight recorder over SPI
- [ ] **Phase 3 - Custom PCB:** KiCad schematic, layout, fabrication, bring-up
- [ ] **Phase 4 - Telemetry:** 915 MHz downlink, flight test against GPS ground truth

## Build log

Dated engineering notes, including what broke, how it was diagnosed, and why each
decision was made: [docs/build-log.md](docs/build-log.md)

## Toolchain

- VS Code + STM32Cube extension (CMake, ARM GCC, ST-LINK/GDB)
- STM32CubeMX for peripheral configuration and code generation
- macOS on Apple Silicon

Build and flash:

    cmake --build build/Debug
    STM32_Programmer_CLI -c port=SWD -w build/Debug/firmware.elf -rst

Ground station:

    python3 -m venv ground-station/venv
    source ground-station/venv/bin/activate
    pip install pyserial pygame
    python3 ground-station/horizon.py
