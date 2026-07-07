# Flight Computer

Custom STM32-based flight computer: attitude (pitch/roll/heading), altitude, and position computed on-board from raw sensors, logged to microSD, and downlinked to a ground station.

**Status: Phase 1 — breadboard bring-up** · *last updated 2026-07-03*

## Architecture

![Architecture](docs/flight_computer_architecture.svg)

- ICM-20948 — 9-axis IMU: gyro + accel + magnetometer (SPI)
- BMP390 — barometric altimeter (I2C)
- Adafruit Ultimate GPS — position/velocity, NMEA (UART)
- STM32F446RE (Cortex-M4F @ 180 MHz) — sensor fusion, bare-metal C
- Outputs: microSD flight log · 915 MHz telemetry downlink · USB debug

## Roadmap

- [ ] **Phase 1 — Bring-up:** every sensor talking on a breadboard, raw data streaming over USB
- [ ] **Phase 2 — Fusion:** complementary → Madgwick/Kalman filter, live artificial-horizon display, SD logging
- [ ] **Phase 3 — Custom PCB:** KiCad design, fab, hand assembly, board bring-up
- [ ] **Phase 4 — Telemetry:** 915 MHz downlink + ground station, flight test against GPS ground truth

## Repo layout

    firmware/        STM32 firmware — CubeMX-generated CMake project, bare-metal C (HAL)
    hardware/        KiCad schematics + PCB (Phase 3)
    ground-station/  Python live display (Phase 2)
    docs/            build log, architecture, test notes

## Build log

Dated engineering notes — what broke, how it got fixed, and why decisions were made: [docs/build-log.md](docs/build-log.md)

## Toolchain

- VS Code + STM32Cube for Visual Studio Code extension (CMake, ARM GCC, ST-LINK/GDB)
- STM32CubeMX (peripheral configuration + code generation)
- Board: NUCLEO-F446RE
