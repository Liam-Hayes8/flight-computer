# Build log

Newest first. Every work session gets an entry: **did / broke / learned / next.**
This doubles as interview prep — a written record of every problem solved.

---

## 2026-09-04 — Altitude fusion, demo video, Phase 2 complete

**Did:** Implemented barometer/GPS altitude fusion as a second complementary filter. The
barometer supplies fast, precise motion; a slowly-corrected offset (0.001 per sample at
10 Hz, roughly a 100 s time constant) anchors it to GPS altitude. Aligned once on first
fix, then tracked. Verified outdoors: GPS altitude wandered 31.1 to 31.2 m while the fused
value held 32.54 to 32.60 m, a 6 cm spread. Recorded the ground-station demo video.
Rewrote the README with measured performance and Phase 2 marked complete.

**Broke:** First attempt pasted the fusion block outside the 10 Hz scope, referencing
variables that only exist inside it. Second attempt nested the new block inside the old
one, producing two `static uint8_t div10` declarations. Both were paste-placement errors,
not logic errors.

**Learned:** The two filters have very different time constants for a reason. Attitude
uses 0.49 s because accelerometer noise is fast and gyro drift is slow. Altitude uses
about 100 s because GPS noise lives on a timescale of seconds while barometric weather
drift lives on tens of minutes. Setting the crossover between the two noise regimes is the
whole design decision. Also: raw barometric altitude read 47 m, 24 m, then 17 m across
three days in the same apartment. That is 30 m of apparent altitude from pressure systems,
which is why an unfused barometer cannot be an absolute altitude reference.

**Next:** MicroSD logging over SPI with FATFS once the breakout arrives. Then the car test
against phone-GPS ground truth. Then KiCad.

---

## 2026-08-28 — NMEA GGA parser with host-side test suite

**Did:** Wrote `gps_parse.c`: a field-walker, degrees-minutes to decimal-degrees
conversion, hemisphere sign handling, empty-field guards, and XOR checksum validation.
Built a host-side test harness (`nmeatest.c`) that runs on the Mac with `gcc`, using five
real sentences captured from the hardware including a no-fix sentence, a wrong sentence
type, and a deliberately corrupted checksum. All tests passed before the parser ever
touched the board. Wired it into the firmware, which now emits `$POS,lat,lon,alt,sats`
lines from a real fix. Ordered microSD breakout and card.

**Broke:** Replacing the raw `GPS:` print with the parsed version removed the only way to
tell "sentences arriving, no fix" apart from "no sentences at all." Added a `$POS,nofix`
branch so silence became informative. Also hit the `cmake: command not found` problem;
the VS Code extension bundles its own CMake that is not on PATH. Fixed with `brew install
cmake`.

**Learned:** Pure-logic modules should be developed and tested on the host where the
edit-run cycle is one second, not a build-flash-connect-reset cycle. This is how real
firmware teams structure code. Also: NMEA packs coordinates as degrees+minutes in one
number, and latitude uses two degree digits while longitude uses three. That asymmetry is
the classic parsing bug.

**Next:** Altitude fusion, which now has all its inputs available.

---

## 2026-08-27 — Gyro bias calibration and I2C bus recovery

**Did:** Added startup gyro bias calibration: 200 samples averaged over one second with
the board still, then subtracted from every reading. Measured bias came out at x 2.161,
y 0.149 deg/s. Fused roll settled to 0.20 to 0.25 deg where it had sat at +1.3 deg before.
Added an I2C bus-recovery routine that runs before peripheral init: drives SCL as GPIO,
issues nine clock pulses, then a manual STOP. Pressing RESET repeatedly now brings both
sensors back every time.

**Broke:** Pressing RESET mid-session had killed the I2C bus three separate times: scan
found 0 devices, both sensors FAILED, and overruns exploded (1128 of 1729 ticks) because
each failed read blocked for its full 100 ms timeout. Cause: RESET restarts the STM32 but
not the sensors, so a sensor left mid-transaction holds SDA low forever and the master
cannot generate a START.

**Learned:** The residual attitude error before calibration was predictable from theory:
gyro bias times filter time constant = 2.13 deg/s x 0.49 s = 1.04 deg predicted, 0.98 deg
observed. The hardware agreed with the math to within 6%. Also: a stationary alignment
period at startup is a real operational constraint, and it is the same reason an aircraft
INS needs to sit still before taxi.

**Next:** NMEA parsing, then altitude fusion.

---

## 2026-08-26 (evening) — Fixed-rate loop, attitude fusion, ground station

**Did:** Replaced `HAL_Delay(500)` with a TIM2 hardware timer at exactly 100 Hz (84 MHz /
8400 / 100). The loop now runs on a flag set by the timer ISR, counts its own overruns,
and decouples rates: IMU at 100 Hz, telemetry at 10 Hz, health line at 1 Hz. Verified
ticks climbing by exactly 100 per second with zero overruns. Then built attitude
estimation in three deliberate stages. Stage 1, accelerometer only: correct and stable on
a still desk, catastrophic during motion (consecutive samples 100 ms apart showed an 83.6
deg jump, an implied 836 deg/s that never happened). Stage 2, gyro only: silky through
motion, but drifted perfectly linearly to 128.2 deg in 60 seconds while motionless (2.14
deg/s, matching the bias measured weeks earlier). Stage 3, complementary filter at
0.98/0.02: held within 0.08 deg over a minute while the gyro-only estimate went to 117
deg, and traced a smooth curve through an aggressive shake while the accelerometer thrashed
between +91 and -18 deg on consecutive samples. Then wrote the Python ground station:
firmware emits `$TLM,roll,pitch,alt` at 10 Hz; `horizon.py` reads it over pyserial and
draws an artificial horizon in pygame.

**Broke:** The board was running stale firmware for a while: exactly two prints per second
is the signature of `HAL_Delay(500)`, and it took a moment to recognize that the new code
had been built but not flashed. Also: `pip install` hit PEP 668 "externally managed
environment" on Homebrew Python. Fixed with a virtual environment, which is the right
answer anyway.

**Learned:** Every line of fusion math depends on `dt` being exact, not approximate. With
`HAL_Delay` the real interval was 500 ms plus however long the sensor reads and printfs
took, which varies. Integrating with a wrong `dt` accumulates error forever. Also: seeing
each sensor fail on its own before fusing them is what makes the filter feel inevitable
rather than magic. The accelerometer and gyro have exactly complementary failure modes.

**Next:** Bias calibration to remove the 1 deg residual. Bus recovery for the reset
problem.

---

## 2026-08-26 (day) — GPS first fix, driver refactor, race condition

**Did:** Took the board outside and got the first satellite fix: `$GNGGA` with fix
quality 1, five satellites, real coordinates, and the date field reading 260826 downloaded
from orbit. Both `$GP` and `$GL` sentences present, confirming the dual-constellation
PA1616D. Refactored `main.c` into modular drivers: `bmp581.c`, `icm20948.c`, `gps.c`,
each with its own header and a small init/read API. Behavior byte-for-byte identical
before and after, which is the definition of a successful refactor.

**Broke:** After the refactor, GPS lines printed empty. Root cause was a race condition:
`gps_get_sentence()` cleared the ready flag and handed main a pointer to the shared
buffer, then `printf` read it slowly while the ISR overwrote it. Fixed by copying the
sentence out inside a `__disable_irq()` critical section. Then a second bug appeared that
looked like GPS reboots (`$PMTK011` startup messages, concatenated fragments) and was
diagnosed as power sag. It was actually tio launched without `-b 115200`, decoding the
stream at the wrong baud rate. The garbage was a measurement artifact.

**Learned:** `volatile` stops the compiler from optimizing away reads; it does nothing to
stop an interrupt from firing mid-operation. Shared state between an ISR and the main loop
needs both `volatile` and a critical section. And the bigger lesson: verify the
measurement tool before diagnosing the system. I spent a round diagnosing a hardware fault
that was a terminal misconfiguration. Also added `HAL_UART_ErrorCallback` because a single
overrun or framing error otherwise stalls interrupt reception permanently.

**Next:** Fixed-rate timing, then fusion.

---

## 2026-08-25 — GPS soldered and wired, first NMEA text

**Did:** Soldered headers on the Ultimate GPS (nine pins, cleaner joints than the first two
boards). Wired VIN to 3V3, GND, and the TX/RX crossover to D2/D8 (USART1 on PA10/PA9).
Enabled USART1 at 9600 baud in CubeMX. First version polled the UART with a blocking read
and got truncated `GPS: $` fragments because the loop was asleep most of the time.
Replaced with interrupt-driven reception: a one-byte ISR assembles lines and re-arms itself
after every byte. Full `$GNGGA` sentences immediately.

**Broke:** The polling approach dropped most of every sentence. Not a wiring problem; the
main loop simply was not listening when bytes arrived.

**Learned:** UART TX and RX cross over: the GPS's transmit goes to the STM32's receive.
Wiring them straight across fails silently with no error. Also: interrupt-driven reception
is the only correct approach for asynchronous serial data. Polling works in a tutorial and
fails the moment the main loop has other work to do.

**Next:** Get a fix outdoors. Refactor the growing `main.c`.

---

## 2026-08-20 — ICM-20948 bring-up: two sensors on one bus

**Did:** Soldered the ICM-20948 (two header rows, straddling the breadboard center
channel). Wired VIN/GND/SCL/SDA into the same rows the barometer already occupies. Scan
found two devices: 0x47 and 0x69. WHO_AM_I read 0xEA. Wrote the init sequence: device
reset, clear sleep mode, enable all six axes, then bank-switch to bank 2 for gyro (+/-250
dps) and accel (+/-2 g) config, and back to bank 0 for reading. Burst-read twelve bytes
from 0x2D. Flat on the desk: Z reads 1.02 g, X and Y near zero, gyro shows about 1.9 deg/s
of X bias at rest. Tilt test through all orientations: gravity magnitude stayed 1.02 to
1.07 across every orientation, confirming consistent calibration on all three axes.

**Broke:** Nothing on the bus. But the tilt-test data captured the accelerometer being
corrupted by hand motion: magnitude drifted off 1.0 g during fast movement because it
measures gravity plus acceleration and cannot separate them.

**Learned:** The ICM-20948 has more registers than fit in its address space, so it uses
banks: write to 0x7F to select which set of registers is visible. Also: this chip sends
data big-endian (high byte first), the opposite of the BMP581. Getting endianness backwards
produces plausible-looking garbage. And the measured gyro bias is the whole reason a
Kalman or complementary filter exists: integrating 1.9 deg/s naively gives 114 deg of error
per minute.

**Next:** GPS, the last Phase 1 sensor.

---

## 2026-08-18 — BMP581 live readings, first real instrument

**Did:** Configured the BMP581: wrote 0x60 to OSR_CONFIG (pressure enabled, 16x
oversampling) and 0x5D to ODR_CONFIG (normal mode, 10 Hz). Burst-read six bytes from
0x1D: temperature is bytes 0 to 2 divided by 65536, pressure is bytes 3 to 5 divided by
64. Converted pressure to altitude with the standard atmosphere formula. Measured the noise
floor: 0.8 Pa peak-to-peak, about 7 cm of altitude. Lifted the board 80 cm by hand: 9.6 Pa
swing, a clean rise and return, signal-to-noise about 12:1.

**Broke:** The first build printed blank floats because the default newlib-nano printf has
no float support. Fixed with `target_link_options(... -u _printf_float)` in CMakeLists.
This one bites everybody once.

**Learned:** The barometer is a genuine instrument: 7 cm resolution, sub-second response,
returns to its starting value after a round trip. But its absolute value depends on a
sea-level pressure assumption that today's weather does not match, so only the change is
trustworthy. That is the seed of the altitude-fusion argument.

**Next:** The IMU.

---

## 2026-08-18 — First sensor on the bus

**Did:** Soldered headers on the BMP581 (first solder joints ever). Wired VIN to 3V3, GND,
SCL to D15 (PB8), SDA to D14 (PB9). Scan reported `answered: 0x47`. Read register 0x01
and got 0x50, matching the chip ID from the datasheet homework. Both numbers predicted in
advance from the datasheet, both confirmed by the hardware.

**Broke:** Pressed the board into the breadboard by the PCB instead of the header spacer,
and two pins popped loose through the plastic. The joints that failed were the skimpy ones.
Resoldered with more dwell time. Also initially soldered with peaks instead of cones (iron
pulled away too fast); reflowed.

**Learned:** Press on the black plastic spacer, not the board. Solder joints should be
small shiny cones that flow onto both pad and pin; dull grey beads sitting on top are cold
joints. And the address (0x47, where it lives on the bus) and the chip ID (0x50, what it
is, in register 0x01) are two different numbers with two different jobs.

**Next:** Configure it and read pressure.

---

## 2026-08-13 — First hardware bring-up

**Did:** Parts arrived. Plugged in the Nucleo, flashed with `STM32_Programmer_CLI`,
connected with a serial terminal. Got the LED blinking and the boot banner over the
ST-LINK virtual COM port. I2C scan correctly reported 0 devices with nothing wired.

**Broke:** Almost everything, in sequence. The blink code had been sitting between `USER
CODE END WHILE` and `USER CODE BEGIN 3`, which is outside the protected region, and CubeMX
deleted it on regenerate. The printf and scanner paste-ins had never been saved. CubeMX
had dropped the PA5 LED config, so `LD2_Pin` no longer existed; switched to the BSP's
`BSP_LED_Init/Toggle(LED2)`. Flashed the stale binary twice before noticing the identical
9.47 KB size. Closed a `screen` window instead of quitting it, which left an orphan holding
the serial port ("Resource busy"). Switched to `tio`.

**Learned:** The USER CODE markers are not a suggestion. Rebuild before flashing, and
check the reported size. "Undefined reference" at link time means a source file is missing
from the build, not that the code is wrong. And one serial program at a time.

**Next:** Solder the barometer.

---

## 2026-07-17 — printf and I2C scanner, pre-written before hardware

**Did:** Enabled USART2 (115200 8N1, routed over the ST-LINK VCP) and I2C1 (PB8/PB9) in
CubeMX. Retargeted `printf` to the UART via `_write`. Wrote the boot banner and an I2C
bus scanner that knocks on 0x08 to 0x77 and reports every device that ACKs. Clean build.
Flash grew from 7.1 KB to 9.7 KB, the cost of printf formatting plus the UART and I2C
drivers.

**Broke:** USART2 came up disabled in the .ioc and had to be enabled by hand. CubeMX's
recent-projects entry pointed at a stale path after the project moved into the repo.

**Learned:** Peripheral hardware ships disabled; CubeMX pin config plus generate is what
turns it on, and the matching HAL driver files only compile once the peripheral is enabled.
Also: the scan runs 0x08 to 0x77 because the protocol reserves the first and last eight
addresses, and `addr << 1` makes room for the read/write bit.

**Next:** Wait for parts.

---

## 2026-07-13 — Toolchain complete, first clean build

**Did:** Installed the current STM32CubeCLT after the VS Code bundle manager came up empty
on the compiler. First clean build of the generated F446RE project: flash 7.1 KB, RAM 1.6
KB. Added an LED blink between the USER CODE markers.

**Broke:** ST's site search served a three-year-old x86 CubeCLT (v1.13, 2023). Gatekeeper
blocked the .pkg. Then the disk hit 100% full mid-build (ENOSPC): macOS had reported 188
GB available, but that counted purgeable space; `df` showed 117 MB. The real culprit was
about 50 GB of security-class ISOs in Downloads, not the ST tools. Also discovered VS Code
itself was running from Downloads and had to be moved to Applications.

**Learned:** Get ST tools from product pages, not site search. Restart VS Code after any
PATH change. `df -h /` shows the truth about disk space at write time.

**Next:** Pre-write bring-up firmware while parts ship.

---

## 2026-07-03 — Day 0

**Did:** Ordered Phase 1 parts. Set up the toolchain on macOS and verified a clean build
of a generated NUCLEO-F446RE project. Created this repo.

**Broke:** Original plan was STM32CubeIDE, but on Apple Silicon it runs x86 under Rosetta,
and current macOS 26 builds hit a known Eclipse text-selection bug ST lists as unsupported.
Switched to ST's VS Code flow: STM32Cube extension pack plus standalone STM32CubeMX for
peripheral config and CMake project generation.

**Learned:** Check the host-OS support notes before committing to a toolchain. ST's
ecosystem is mid-migration from Eclipse to VS Code, and the VS Code path is the supported
one on this hardware. Also: the BMP390 was unavailable everywhere, so the BMP581 (newer,
higher precision, same interface) was substituted. Supply-chain substitution with an
equivalent part is a routine engineering event.

**Next:** Generate the CMake project, get a clean build, push. When parts arrive: blink,
printf, I2C scan.
