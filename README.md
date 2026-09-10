# Platypus — Freediving Computer (PoC)

<p align="center">
  <img src="./picture_finished.jpg" alt="Platypus Dive Computer" width="55%">
</p>

**Platypus** is a wearable freediving computer developed as a hardware and software **Proof of Concept (PoC)** to validate a **zero-penetration, hermetically sealed architecture**.

In underwater instrumentation, mechanical push-buttons and exposed charging pins are the primary vectors of failure: dynamic O-rings wear out, salt crusts jam button cavities, and contacts corrode. This PoC eliminates through-hull penetrations entirely by testing:
1. **Accelerometer-based tap navigation** as a solid-state button replacement.
2. **Inductive wireless charging** through a completely sealed casing.
3. **Magnetic battery cutoff** for zero shelf-discharge without opening the housing.
4. **Hybrid mechanical integration** pairing an off-the-shelf color OLED module with a custom 4-layer carrier PCB.

---

## Demo

<p align="center">
  <img src="./interface_demo.gif" alt="UI Navigation Demo" width="55%">
</p>

<p align="center"><em>Tap-based navigation through the interface — accelerometer detects directional impacts on the casing.</em></p>

<p align="center">
  <img src="./dive_test.gif" alt="Dive Test" width="55%">
</p>

<p align="center"><em>Live depth tracking during a test — automatic dive detection and surface recovery timer.</em></p>

---

## Key Features

| Category | Details |
|---|---|
| **Depth & Temperature** | Real-time tracking via MS5837-30BA (±2 mm resolution, 300–1200 hPa). Automatic fresh/salt water density compensation. |
| **Dive State Machine** | Auto-triggers at 1.0 m depth. Tracks dive time, max depth per dive, and session statistics. |
| **Surface Recovery Timer** | Starts automatically on surfacing. Color-coded: red until 2× dive time, then green. Auto-hides after 10 min. |
| **Session Statistics** | Max dive time, max depth, total dives, and device run time — all tracked per session. |
| **Tap Navigation** | LIS2DUX12 accelerometer detects directional taps on the casing. Left/right to scroll, down to select. No buttons, no dynamic seals. |
| **Magnetic Battery Cutoff** | Integrated magnetic switch isolates the LiPo battery with an external magnet (true 0 µA shelf storage while sealed). |
| **Wireless Charging** | Completely sealed enclosure — power delivered through the case via LTC4120 wireless power receiver. |
| **Display** | 1.27" SSD1351 color OLED (128×96), driven over SPI with DMA. LVGL handles 60 fps animated transitions. |
| **Battery Monitoring** | ADC with hardware calibration curve. Voltage divider maps LiPo range (3.0–4.2 V) to percentage. |
| **Deep Sleep** | Full power-off via menu. GPIO wakeup from accelerometer interrupt. OLED and SPI bus shut down cleanly. |

---

## Mechanical Design

The enclosure is a pressure-rated, sealed housing with no external buttons, ports, or gaskets that could compromise waterproofing. The only interface is the OLED front window, the pressure port, and the wireless charging coil on the back.

<p align="center">
  <img src="./Exploded_view.png" alt="Exploded View of the Enclosure" width="50%">
</p>

<p align="center">
  <a href="https://cad.onshape.com/documents/cc2f090fad22f578b68190d0/w/ffbf50c58a8e366da5acdcdc/e/88baec5796e801b1a2b4a1f3">
    View the 3D model on Onshape ↗
  </a>
</p>

---

## Hardware Architecture

The electronics are split between the wrist unit and a dedicated charging dock.

### 1. Dive Computer Board (Wrist Unit)

The core electronics are built around a custom **4-layer PCB** designed in KiCad, dimensioned to mate directly with an off-the-shelf color OLED module:

- **Form Factor & Display**: Sourced a compact 1.27" 128×96 color OLED module (SSD1351 controller, SPI) and tailored the custom 4-layer PCB and enclosure footprint around this geometry.
- **MCU**: ESP32-C3 (RISC-V core, low power modes, BLE).
- **Pressure Sensor**: TE MS5837-30BA (gel-protected submersible pressure & temperature sensor on I2C).
- **Accelerometer**: ST LIS2DUX12 (ultra-low-power, configured for shock/tap detection with hardware interrupt).
- **Wireless Power Receiver**: Analog Devices LTC4120 with an internal Würth planar receiver coil (WE-WPCC-RX), delivering CC/CV charging to the internal 3.7V LiPo cell.
- **Magnetic Power Switch**: Integrated Hall-effect sensor / magnetic switch controlling a P-MOSFET gate, enabling complete physical battery disconnection via an external magnet without unsealing the housing.

| Component | Part | Role |
|---|---|---|
| MCU | ESP32-C3 | RISC-V core, sensor aggregation, LVGL rendering |
| Pressure Sensor | MS5837-30BA | Absolute pressure & temperature (I2C @ 100 kHz) |
| Accelerometer | LIS2DUX12 | Tap detection with hardware interrupt (I2C) |
| Wireless Charger RX | LTC4120 | Wireless power receiver & CC/CV LiPo charger |
| Magnetic Disconnect | A1102 + P-MOS | Physical battery isolation via external magnet (0 µA storage) |
| Display Module | SSD1351 (Off-the-shelf) | 1.27" 128×96 color OLED (SPI with DMA) |
| Battery | LiPo 3.7V | Single cell, monitored via calibrated ADC voltage divider |

### 2. Wireless Charger Dock (Transmitter)

A companion transmitter base powered via USB-C:

| Component | Part | Role |
|---|---|---|
| Transmitter IC | LTC4125 | 5W AutoResonant wireless power transmitter |
| Transmit Coil | Würth WE-WPCC-TX | 24 µH primary coil for inductive power transfer |
| Input | USB-C (5V) | Power supply |

Source files (KiCad projects, gerbers, BOM): [`Hardware/V1/`](Hardware/V1/)

---

## Software Architecture

The firmware is written in **C** on **ESP-IDF** with **FreeRTOS**. The codebase follows an MVC pattern:

```
Software/DiveComputer/main/
├── main.c                  # App entry, event loop, screen state machine
├── components/
│   ├── pressure_sensor.c   # MS5837 driver, depth calculation, calibration
│   ├── accelerometer.c     # LIS2DUX12 tap detection (ISR + burst analysis)
│   ├── battery.c           # ADC with hardware calibration curve
│   └── oled_display.c      # SSD1351 init, SPI+DMA, LVGL driver
└── ui/
    ├── ui_model.h          # Global state structure (dive_state_t)
    ├── ui_controller.c     # Dive logic, timers, brightness control
    ├── ui_layout.c         # All LVGL screens and refresh functions
    └── ui_layout.h         # Public UI API
```

### Task Model

| Task | Core | Priority | Rate | Role |
|---|---|---|---|---|
| `dive_sensor_task` | 1 | 5 | 4 Hz | Reads pressure sensor, computes depth, updates model |
| `lvgl_port_task` | 0 | 5 | 100 Hz | LVGL rendering and screen refresh |
| `tap_analyzer_task` | 0 | 10 | Event-driven | Wakes on accelerometer interrupt, analyzes shock direction |
| Main loop | 0 | 1 | 50 Hz | Polls tap events, drives screen state machine |

### Dive State Machine

```
                  depth > 1.0m                    depth < 0.3m
    ┌─────────┐ ───────────────> ┌───────────┐ ───────────────> ┌────────────┐
    │ SURFACE │                  │  DIVING   │                  │ POST-DIVE  │
    │         │ <─────────────── │           │                  │ (Recovery) │
    └─────────┘   recovery > 10m └───────────┘                  └────────────┘
         ^                                                            │
         │                                                            │
         └────────────────────────────────────────────────────────────┘
                              recovery timer expires
```

### Tap Detection Pipeline

The accelerometer's hardware tap interrupt wakes the ESP32, which performs a 100-sample burst read (~10 ms) to capture the full shock waveform. The axis with the largest amplitude delta wins, and its sign determines the direction. A 2000-unit noise floor filter rejects accidental vibrations.

```
[HW Interrupt] ──> [ISR Queue] ──> [Burst Read (100 samples)] ──> [Axis Analysis] ──> [Direction Filter] ──> [UI Event]
```

---

## Build & Flash

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/).

```bash
cd Software/DiveComputer
idf.py set-target esp32c3
idf.py build
idf.py -p COMx flash monitor
```

---

## Repository Structure

```
Dive_Computer/
├── Hardware/
│   └── V1/
│       ├── Computer/       # Main board (4-layer) KiCad project + gerbers
│       └── Charger/        # Wireless charger dock KiCad project + gerbers
├── Software/
│   └── DiveComputer/       # ESP-IDF firmware project
├── Exploded_view.png
├── picture_finished.jpg
├── interface_demo.gif
└── dive_test.gif
```

---

## License

This is a personal portfolio project. Source code and hardware designs are shared for educational and demonstration purposes.
