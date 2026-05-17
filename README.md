# \# 🌊 Nautilus Dive Computer

# 

# !\[Nautilus Cover](https://via.placeholder.com/800x400?text=Nautilus+Dive+Computer+Cover+Image)

# \*Note: Replace the link above with a real photo of your finished prototype.\*

# 

# \*\*Nautilus\*\* is a custom-built, open-source dive computer powered by an ESP32 microcontroller. Designed for freedivers and underwater explorers, it features a vibrant OLED display, fluid UI animations, and a completely sealed waterproof enclosure utilizing accelerometer-based tap detection instead of traditional mechanical buttons.

# 

# \## ✨ Key Features

# 

# \### 🖥️ Hardware \& Interface

# \* \*\*1.27" Color OLED Display:\*\* High-contrast screen ensuring perfect readability underwater, even in low-light or murky conditions.

# \* \*\*Fluid UI powered by LVGL:\*\* Custom-built user interface featuring 60fps smooth transitions, ease-in-out animations, and an intuitive sliding carousel menu.

# \* \*\*Tap-to-Control (Accelerometer):\*\* Zero hull penetrations required. Navigation is handled by an internal accelerometer that detects physical taps on the casing, ensuring maximum depth rating and zero leak points.

# \* \*\*High-Precision Pressure Sensor:\*\* Real-time depth and temperature monitoring.

# 

# \### 🤿 Dive Tracking \& Software

# \* \*\*Real-Time Dive Metrics:\*\* Displays Current Depth, Dive Time, Water Temperature, and Last Dive Depth.

# \* \*\*Surface Recovery Timer:\*\* Automatically triggers a highly visible recovery timer upon surfacing.

# \* \*\*Session Statistics:\*\* Tracks and saves Max Depth, Total Run Time, Max Dive Time, and Total Number of Dives.

# \* \*\*Environment Selection:\*\* Toggle between Fresh Water and Salt Water for accurate water density calculations (1.000 vs \~1.025).

# \* \*\*BLE Connectivity:\*\* Bluetooth Low Energy integration allows you to sync and download your latest session statistics directly to your smartphone after a dive.

# 

# \## ⚙️ Mechanical Design

# 

# The enclosure is designed to withstand high pressure while maintaining a compact, wearable form factor.

# 

# \* 🔗 \*\*\[View the 3D Model on Onshape](https://cad.onshape.com/documents/cc2f090fad22f578b68190d0/w/ffbf50c58a8e366da5acdcdc/e/88baec5796e801b1a2b4a1f3)\*\*

# \* 📄 \*\*\[Download the Exploded View PDF](./Exploded\_view.pdf)\*\* \*(Make sure to place `Exploded\_view.pdf` in the root of your repository).\*

# 

# > \*\*Tip:\*\* You can also add a screenshot of your exploded view here for quick visualization!

# > `!\[Exploded View](path\_to\_your\_screenshot.png)`

# 

# \## 🛠️ Tech Stack \& Components

# 

# \* \*\*MCU:\*\* ESP32-C3 (NimBLE for Bluetooth stack)

# \* \*\*Framework:\*\* ESP-IDF (C/C++)

# \* \*\*Graphics Library:\*\* LVGL (Light and Versatile Graphics Library)

# \* \*\*Sensors:\*\* \* Accelerometer (for UI navigation)

# &#x20; \* High-resolution absolute pressure sensor (Depth/Temp)

# \* \*\*Display Driver:\*\* SSD1351 (SPI interface overclocked for fluid 60fps rendering)

# 

# \## 🚀 Getting Started

# 

# \### Prerequisites

# \* \[ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) (v5.0 or later recommended)

# \* C/C++ build tools

# 

# \### Installation

# 1\. Clone the repository:

# &#x20;  ```bash

# &#x20;  git clone \[https://github.com/](https://github.com/)\[YourUsername]/Nautilus.git

# &#x20;  cd Nautilus

