# ESP32 TMC2209 Stepper Driver Example

An example project demonstrating how to control a **Trinamic TMC2209** stepper motor driver using an **ESP32** (30-pin Dev Board / NodeMCU-32S) via **STEP/DIR** and **single-wire UART** with the [TMCStepper](https://github.com/teemuatlut/TMCStepper) library.

---

## ESP32 Board Reference

This project is tailored for standard 30-pin ESP32 development boards (such as the ESP32 Dev Module / NodeMCU-32S):

![ESP32 Pinout](resources/esp32-pinout.jpg)

![TMC2209 Pinout](resources/TMC2209.jpg)

---

## Wiring & Connections

### Pin Mapping Table

| ESP32 Pin Label | ESP32 GPIO | TMC2209 Pin | Function / Description |
| :--- | :--- | :--- | :--- |
| **D25** | `GPIO25` | `STEP` | Step pulse input |
| **D26** | `GPIO26` | `DIR` | Direction control |
| **D27** | `GPIO27` | `EN` | Driver Enable (**Active LOW**: LOW = Enabled, HIGH = Disabled) |
| **RX2** | `GPIO16` | `PDN_UART` | Direct connection to TMC2209 UART single-wire pin |
| **TX2** | `GPIO17` | `PDN_UART` | **Connect through a 1kΩ resistor** to `PDN_UART` (or to RX2) |
| **D14** *(optional)* | `GPIO14` | `DIAG` | StallGuard / diagnostic alert pin |
| **3V3** | `3.3V` | `VIO` | Logic voltage supply (3.3V) |
| **GND** | `GND` | `GND` (Logic) | Logic ground (common with ESP32) |
| *External 12V–24V* | — | `VMOT` | Stepper motor DC power supply (add 100µF capacitor across VMOT/GND) |
| *External GND* | — | `GND` (Power)| Motor power supply ground (**must tie to ESP32 GND**) |
| Stepper 4-wire | — | `2B, 2A, 1A, 1B` | Stepper motor phase coils (A1/A2, B1/B2) |

---

### Single-Wire UART Connection

TMC2209 communicates over single-wire half-duplex UART on the `PDN_UART` pin:

```
ESP32 TX2 (GPIO17) -----[ 1kΩ Resistor ]-----+
                                             |
ESP32 RX2 (GPIO16) --------------------------+----> TMC2209 PDN_UART Pin
```

- Connect ESP32 **RX2 (GPIO 16)** directly to the TMC2209 **PDN_UART** pin.
- Connect ESP32 **TX2 (GPIO 17)** to the same **PDN_UART** line through a **1kΩ series resistor**.
- This enables bidirectional communication with HardwareSerial `Serial2` without signal contention.

---

### Driver Address & Sense Resistor
- **Address (`0b00`)**: Ensure `MS1` and `MS2` pins are tied to GND (default on standard BigTreeTech / StepStick modules).
- **Sense Resistor (`0.11Ω`)**: Standard SilentStepStick and BTT TMC2209 modules use $R_{SENSE} = 0.11\,\Omega$.

> [!CAUTION]
> **NEVER plug or unplug the stepper motor while `VMOT` is powered!**  
> Inductive voltage spikes will instantly damage the TMC2209 driver. Always power down the motor power supply before making wiring changes.

---

## Features

- **UART Configuration**: Sets current, microstepping, and chopper modes via software registers.
- **StealthChop2™**: Ultra-quiet motor movement with automatic PWM tuning.
- **SpreadCycle™**: High-torque chopper mode available on demand.
- **Microstepping**: Supports 1, 2, 4, 8, 16, 32, 64, 128, and 256 microsteps.
- **Comprehensive Diagnostics**: Queries connection status, silicon version (`0x21`), over-temperature warnings, short-to-ground, and open-load flags.
- **Interactive Serial Console**: Real-time test menu at **115200 baud**.

---

## Building and Flashing

This project is built with [PlatformIO](https://platformio.org/).

### Flash and Monitor (Combined)
```bash
make flash-monitor
```
or via PlatformIO CLI:
```bash
pio run --target upload --target monitor
```

### Individual Commands
```bash
# Build only
pio run            # or: make build

# Upload / Flash only
pio run -t upload  # or: make flash

# Serial Monitor only (115200 baud)
pio device monitor # or: make monitor
```

---

## Interactive Serial Commands

Open the serial monitor at **115200 baud** and send any of the following single-key commands:

| Command | Action |
| :---: | :--- |
| `f` / `b` | Continuous motion **FORWARD** / **REVERSE** |
| `1` / `2` | Rotate exactly **1 full revolution** forward / backward |
| `a` | Toggle **Auto-Demo** (alternates 1 revolution CW and CCW with pauses) |
| `s` | **STOP** motor immediately |
| `e` | Toggle driver **Enable** state (`EN` pin LOW/HIGH) |
| `+` / `-` | Increase / decrease stepping speed |
| `m` | Cycle microstepping (`1`, `2`, `4`, `8`, `16`, `32`, `64`, `128`, `256`) |
| `c` | Cycle RMS current (`300`, `500`, `700`, `900`, `1100` mA) |
| `t` | Toggle **StealthChop** (silent) vs **SpreadCycle** (torque) |
| `d` | Display full **Driver Diagnostics** (version, faults, thermal state, `DRV_STATUS`) |
| `?` / `h` | Print help menu |

---

## Example Diagnostics Output (`d` command)

```
-------------------------------------------
       TMC2209 DRIVER DIAGNOSTICS          
-------------------------------------------
UART Connection      : OK (code 0)
Silicon Version      : 0x21 [TMC2209 VALID]
Driver Output (EN)   : ENABLED (EN=LOW)
RMS Motor Current    : 600 mA
Microstep Setting    : 1/16
Chopper Mode         : StealthChop (Quiet)
DRV_STATUS Register  : 0x80100000
 - Overtemp Prewarn  : Normal
 - Overtemp Shutdown : Normal
 - Short to GND CoilA: OK
 - Short to GND CoilB: OK
 - Open Load Coil A  : OK
 - Open Load Coil B  : OK
 - Motor Standstill  : YES
 - Current Scale (CS): 18 / 31
 - StallGuard Result : 0
 - DIAG Pin (GPIO14) : HIGH
-------------------------------------------
```
