# 💧 Nautilus Dive Computer

Nautilus is a prototype dive computer specifically designed for ***freediving and spearfishing***. Powered by an ESP32 microcontroller, it features a sealed, buttonless waterproof enclosure utilizing accelerometer-based tap detection and wireless charging.

## ✨ Key Features

**Hardware & Interface**
* 💧 **1.27" Color OLED Display:** High-contrast screen ensuring *perfect readability* in every condition.
* 🚀 **Fluid UI:** Custom user interface built with **LVGL**, featuring smooth *60fps transitions*.
* 👆 **Tap-to-Control:** Navigation is handled by an internal accelerometer that detects physical taps on the casing, *eliminating mechanical buttons* and preventing leaks.
* 🔋 **Wireless Charging:** Completely sealed power management using a dedicated wireless charging receiver.

**Dive Tracking**
* 📊 **Real-Time Metrics:** Current Depth, Dive Time, Water Temperature, and Last Dive Depth.
* ⏱️ **Surface Recovery Timer:** Automatically triggers upon surfacing.
* 📈 **Session Statistics:** Tracks Max Depth, Total Run Time, Max Dive Time, and Total Dives.
* 🌊 **Environment Selection:** Toggle between *Fresh Water* and *Salt Water* for accurate pressure-to-depth conversion.
* 📱 **BLE Connectivity:** Bluetooth Low Energy integration to *sync session statistics* to a smartphone.

## ⚙️ Mechanical Design

The enclosure is designed to withstand high pressure while maintaining a compact form factor.

<p align="center"><img src="./Exploded_view.png" alt="Exploded View" width="50%"></p>

* 🔗 [View the 3D Model on Onshape](https://cad.onshape.com/documents/cc2f090fad22f578b68190d0/w/ffbf50c58a8e366da5acdcdc/e/88baec5796e801b1a2b4a1f3)

## 🛠️ Tech Stack & Components

* 💻 **MCU:** *ESP32-C3* (NimBLE for Bluetooth stack)
* 🛠️ **Framework:** *ESP-IDF* (C/C++)
* 🎨 **Graphics Library:** *LVGL*
* 📡 **Sensors:**
    * Accelerometer (for UI navigation): ***LIS12DUX1***
    * Absolute pressure & temperature: ***MS583730BA01-50***
* ⚡ **Power Management:** Wireless Power Receiver ***LTC4120EUD-4.2#PBF***
* 🖥️ **Display Driver:** ***SSD1351*** (SPI interface)
