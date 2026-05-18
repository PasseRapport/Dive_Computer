## 💻 Software Architecture & User Interface

The software is built on **ESP-IDF** using **FreeRTOS** to handle real-time sensor processing without interrupting the UI animations. The graphical interface is powered by **LVGL**.

### FreeRTOS Task Management
To ensure reliability, the system is divided into isolated tasks:
* **Sensor Task:** Reads the MS5837 pressure sensor at high frequency to compute accurate depth and triggers the "Dive Mode" state machine.
* **UI Task:** Handles LVGL rendering, screen transitions, and animations at a steady frame rate.
* **BLE Task:** Runs the NimBLE stack in the background to handle smartphone pairing and GATT server requests.

### 📱 User Interface (Screens)
The UI is designed for high contrast and extreme readability underwater, using a pure black OLED background and bold white typography. 

1. **Surface Screen (Home):**
   * Displays the current Time, Battery level, and Surface Recovery Timer (SRT).
   * Shows a quick summary of the last dive (Max Depth & Dive Time).
2. **Active Dive Screen:**
   * Automatically triggered when depth exceeds 1.2 meters.
   * Displays massive, high-contrast numbers for **Current Depth** and **Dive Time**.
   * Shows real-time Water Temperature.
3. **Water Type Selection:**
   * A settings screen to toggle between **FRESH** and **SALT** water.
   * Features a custom-built 60fps sliding cursor animation with dynamic color inversion to highlight the active selection.
4. **Session Logbook:**
   * Scrollable list of the day's stats: Total Dives, Maximum Depth reached, and Longest Apnea time.

### 👆 Tap-to-Control Navigation
Since the enclosure has absolutely no physical buttons, UI navigation is entirely managed by the **LIS12DUX1** accelerometer.
* The IMU is configured to detect sharp mechanical shocks (taps) on the casing.
* **Single Tap:** Scrolls through menus or cycles to the next screen.
* **Double Tap:** Acts as the "Select" or "Enter" action to confirm settings.
* This logic is filtered by the software to prevent accidental triggers from water turbulence or fast arm movements during a dive.

### ⚙️ Dive State Machine
The computer operates autonomously based on water pressure:
* **Idle/Sleep:** Low power mode, accelerometer waiting for a wake-up tap.
* **Surface Mode:** Bluetooth is active, UI is accessible.
* **Dive Mode:** Triggered by pressure change. Disables Bluetooth to save battery, locks the UI to the Dive Screen, and starts the dive chronometer.
* **Post-Dive:** Triggered upon surfacing. Stops the dive timer, saves the session log to flash memory, and starts the Surface Recovery Timer.
