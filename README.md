# ESP32 Ultra-Low-Power (ULP) Acoustic Wake-Up Controller
A small embedded firmware project using the ESP323's Ultra-Low-Power (ULP) coprocessor to perform continuous acoustic monitoring while the primary CPUs remain in deep sleep.

By offloading the analog-to-digital (ADC) sampling to the RISC-like ULP coprocessor, the system maintains a micro-amp power footprint and only wakes up the high-power main cores when a specific acoustic threshold and pattern is met. It was implemented has a highly power-efficient smart light switcher.

# System Architecture
* **ULP Coprocessor (Assembly):** Continously samples a microphone's analog voltage. It calculates the delta between the minimum and maximum acoustic peaks. If the delta exceeds a predefined threshold, it sends a wake signal to the RTC controller.
* **Main Cores (C++):** Upon waking from deep sleep, the primary CPU executes a check to verify the acoustic pattern of a clap (Quiet --> Loud --> Quiet). If the pattern matches, it actuates an attached servo motor and returns back to deep sleep.

# Features
* **Acoustic Pattern Recognition:** Time-based sampling to prevent false positives from ambient noise like music 
* **Aggressive Power Management:** Isolates unused GPIO pins with `rtc_gpio_isolate()` to prevent leakage current during deep sleep

# Technologies Used
* **C++ & ESP32 ULP Assembly**
* **Duff2013's ulptool: https://github.com/duff2013/ulptool**