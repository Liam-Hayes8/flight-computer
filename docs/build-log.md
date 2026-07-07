# Build log

Newest first. Every work session gets an entry: **did / broke / learned / next.**
This doubles as interview prep — a written record of every problem solved.

---

## 2026-07-03 — Day 0

**Did:** Ordered Phase 1 parts (Nucleo-F446RE, ICM-20948, BMP390, Ultimate GPS, iron, logic analyzer). Set up the toolchain on macOS. Created this repo.

**Broke:** Original plan was STM32CubeIDE, but on Apple Silicon it runs x86 under Rosetta, and current macOS 26 builds hit a known Eclipse text-selection bug that ST lists as unsupported. Switched to ST's VS Code flow instead: STM32Cube extension pack (bundle-managed ARM GCC / GDB / ST-LINK tools) + standalone STM32CubeMX for peripheral config and CMake project generation.

**Learned:** Check the host-OS support notes in release documentation before committing to a toolchain. ST's ecosystem is mid-migration from Eclipse to VS Code — the VS Code path is the supported one on this hardware.

**Next:** Generate the NUCLEO-F446RE CMake project in CubeMX, get a clean build in VS Code, push. When parts arrive: blink onboard LED, printf over the ST-LINK virtual COM port, then I2C address scan and read the BMP390 chip ID (expect 0x60 at address 0x77).
