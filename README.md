# Nautilus — Freediving Computer

<p align="center">
  <img src="./picture_finished.jpg" alt="Nautilus Dive Computer" width="55%">
</p>

Nautilus is a fully custom dive computer built from scratch — PCB, firmware, enclosure, and UI — designed specifically for **freediving and spearfishing**. No off-the-shelf modules, no dev boards in the final product: just a purpose-built embedded system running on an ESP32-C3.

This is a personal project that covers the full product development cycle: schematic capture, 4-layer PCB layout, waterproof mechanical design, real-time firmware, and a custom graphical interface.

---

## Demo

<p align="center">
  <img src="./interface_demo.gif" alt="UI Navigation Demo" width="55%">
</p>

<p align="center"><em>Tap-based navigation through the interface — accelerometer detects directional impacts on the casing.</em></p>

<p align="center">
  <img src="./dive_test.gif" alt="Dive Test" width="55%">
</p>

<p align="center"><em>Live depth tracking during a pool test — automatic dive detection and surface recovery timer.</em></p>

---

## Features

| Category | Details |
|---|---|
| **Depth & Temperature** | Real-time tracking via MS5837-30BA (±2 mm resolution, 300–1200 hPa). Automatic fresh/salt water density compensation. |
| **Dive State Machine** | Auto-triggers at 1.0 m depth. Tracks dive time, max depth per dive, and session statistics. |
| **Surface Recovery Timer** | Starts automatically on surfacing. Color-coded: red until 2× dive time, then green. Auto-hides after 10 min. |
| **Session Statistics** | Max dive time, max depth, total dives, and device run time — all tracked per session. |
| **Tap Navigation** | LIS2DUX12 accelerometer detects directional taps on the casing. Left/right to scroll, down to select. No buttons, no seals to fail. |
| **Battery Monitoring** | ADC with hardware calibration curve. Voltage divider maps LiPo range (3.0–4.2 V) to percentage. |
| **Display** | 1.27" SSD1351 color OLED (128×96), driven over SPI with DMA. LVGL handles 60 fps animated transitions. |
| **Deep Sleep** | Full power-off via menu. GPIO wakeup from accelerometer interrupt. OLED and SPI bus shut down cleanly. |
| **Wireless Charging** | Sealed enclosure — power is delivered through the case via LTC4120 wireless power receiver. |

---

## Mechanical Design

The enclosure is a pressure-rated, sealed housing with no external buttons, ports, or gaskets that could compromise waterproofing. The only interface is the OLED window and the wireless charging coil.

<p align="center">
  <img src="./Exploded_view.png" alt="Exploded View of the Enclosure" width="50%">
</p>

<p align="center">
  <a href="https://cad.onshape.com/documents/cc2f090fad22f578b68190d0/w/ffbf50c58a8e366da5acdcdc/e/88baec5796e801b1a2b4a1f3">
    View the 3D model on Onshape →
  </a>
</p>

---

## Hardware

Both PCBs are custom-designed 4-layer boards in KiCad.

### Dive Computer Board

| Component | Part | Role |
|---|---|---|
| MCU | ESP32-C3 | RISC-V core, Wi-Fi/BLE, low power modes |
| Pressure Sensor | MS5837-30BA | Absolute pressure & temperature (I2C) |
| Accelerometer | LIS2DUX12 | Tap detection with hardware interrupt (I2C) |
| Display | SSD1351 | 1.27" 128×96 color OLED (SPI) |
| Battery | LiPo 3.7V | Single cell, voltage-divided ADC monitoring |

### Wireless Charger Board

| Component | Part | Role |
|---|---|---|
| Receiver IC | LTC4120 | Wireless power receiver, CC/CV LiPo charging |

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
    ┌─────────┐ ───────────────→ ┌────────────┐ ──────────────→ ┌────────────┐
    │ SURFACE │                  │   DIVING   │                 │ POST-DIVE  │
    │         │ ←─────────────── │            │                 │ (Recovery) │
    └─────────┘   recovery > 10m └────────────┘                 └────────────┘
         ↑                                                            │
         └────────────────────────────────────────────────────────────┘
                              recovery timer expires
```

### Tap Detection Pipeline

The accelerometer's hardware tap interrupt wakes the ESP32, which then performs a 100-sample burst read (~10 ms) to capture the full shock waveform. The axis with the largest amplitude delta wins, and its sign determines the direction. A 2000-unit noise floor filter rejects vibrations.

```
HW Interrupt → ISR Queue → Burst Read (100 samples) → Axis Analysis → Direction → UI Event
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
│       ├── Computer/       # Main board KiCad project + gerbers
│       └── Charger/        # Wireless charger KiCad project + gerbers
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
