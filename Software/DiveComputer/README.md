# Firmware — Nautilus Dive Computer

ESP-IDF firmware for the Nautilus freediving computer. Targets the **ESP32-C3** (RISC-V, single core, BLE).

## Quick Start

```bash
# Requires ESP-IDF v5.x
idf.py set-target esp32c3
idf.py build
idf.py -p COMx flash monitor
```

## Architecture

The firmware follows an **MVC pattern** with FreeRTOS task isolation:

- **Model** (`ui_model.h`) — Single `dive_state_t` struct holding all system state (depth, temperature, dive timers, settings). Shared across tasks.
- **View** (`ui_layout.c`) — All LVGL screen definitions and refresh functions. Pure rendering, no logic.
- **Controller** (`ui_controller.c`) — Dive detection state machine, recovery timer logic, brightness control. Runs as LVGL timer callbacks.

### Peripherals

| Driver | File | Bus | Frequency |
|---|---|---|---|
| MS5837-30BA (pressure) | `pressure_sensor.c` | I2C @ 100 kHz | 4 Hz continuous |
| LIS2DUX12 (accelerometer) | `accelerometer.c` | I2C @ 100 kHz | Event-driven (ISR) |
| SSD1351 (OLED) | `oled_display.c` | SPI @ 14 MHz | 100 Hz LVGL refresh |
| Battery ADC | `battery.c` | ADC1 CH3 | 0.1 Hz (10s interval) |

### Task Layout

| Task | Core | Stack | Priority | Purpose |
|---|---|---|---|---|
| `dive_sensor_task` | 1 | 4 KB | 5 | Pressure reading + depth computation |
| `lvgl_port_task` | 0 | 4 KB | 5 | LVGL timer handler (rendering) |
| `tap_analyzer_task` | 0 | 4 KB | 10 | Shock waveform analysis after ISR wakeup |
| Main loop (`app_main`) | 0 | — | 1 | Event polling + screen navigation FSM |

### Dive Detection

- **Dive start**: `depth > 1.0 m` → timer starts, stats reset
- **Dive end**: `depth < 0.3 m` → session stats updated, recovery timer starts
- **Recovery display**: Red if `surface_time < 2 × dive_time`, green otherwise. Hidden after 10 minutes.

### Tap Detection

The LIS2DUX12 hardware tap interrupt fires on GPIO 0 (positive edge). The ISR pushes to a FreeRTOS queue. The analyzer task then burst-reads 100 acceleration samples in ~10 ms, computes the delta (max − min) on each axis, and routes the dominant axis + sign to the UI event system.

Noise floor: deltas below 2000 raw units are discarded.
