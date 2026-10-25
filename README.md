# LOLIN32 Lite Matrix Clock v2.0

An advanced **48 × 16 RGB LED Matrix Clock, Weather Station, and Smart Alarm System** built for the LOLIN32 Lite (ESP32). Features triple-output WS2812B display mapping, dual redundant hardware RTCs (DS3231 + PCF8563), automated NTP internet time sync, Open-Meteo live weather with 16×16 animated icons, Bluetooth Low Energy (BLE) mobile controls, customizable multi-alarm manager, hourly chimes, and an onboard web dashboard with mDNS and REST APIs.

---

## ⚡ Specifications & Hardware Mapping

| Subsystem | Configuration & Pinout |
|---|---|
| **Microcontroller** | LOLIN32 Lite (ESP32-WROOM-32), 240 MHz, 4 MB Flash |
| **Matrix Display** | 48 × 16 physical pixels (768 WS2812B LEDs, GRB color order) |
| **Matrix Channel 1** | **GPIO 27** → `matrix1` (256 LEDs, physical lower-right 32 × 8 section) |
| **Matrix Channel 2** | **GPIO 25** → `matrix2` (256 LEDs, physical left 16 × 16 section) |
| **Matrix Channel 3** | **GPIO 26** → `matrix3` (256 LEDs, physical upper-right 32 × 8 section) |
| **RTC I²C Bus** | **SDA: GPIO 23**, **SCL: GPIO 19** (400 kHz Fast-Mode I²C) |
| **Primary RTC** | Maxim **DS3231** High-Precision TCXO at I²C `0x68` |
| **Secondary RTC** | NXP **PCF8563** Fallback RTC at I²C `0x51` |
| **Alarm Buzzer** | SFM-27-I Active Piezo Buzzer on **GPIO 18** |
| **Time Sync Sources** | NTP (`pool.ntp.org`), DS3231, PCF8563, BLE 1-Tap Sync, Web UI |
| **Network Modes** | Wi-Fi Station (home network) + SoftAP (`MatrixClock-Setup`) with mDNS |
| **Weather Engine** | Open-Meteo REST API (temperature, WMO conditions, solar dawn/dusk) |

---

## 📐 Physical LED Mapping

The custom `XY(x, y)` coordinate mapping translates global Cartesian coordinates `(x: 0–47, y: 0–15)` to the physical zigzag wiring of the 3 separate panels according to the front-view reference ([`mapping_reference.png`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/mapping_reference.png)). Global screen coordinate `(0, 0)` is top-left; **LED index 0 is bottom-right**.

| Region | Screen Coordinates | LED Indices | Channel & Buffer |
|---|---:|---:|---|
| **Left Panel** | `x 0–15`, `y 0–15` | `256–511` | `matrix2`, **GPIO 25** |
| **Upper-Right Panel** | `x 16–47`, `y 0–7` | `512–767` | `matrix3`, **GPIO 26** |
| **Lower-Right Panel** | `x 16–47`, `y 8–15` | `0–255` | `matrix1`, **GPIO 27** |

> [!WARNING]
> Do **not** daisy-chain the three panels into a single continuous strip. The firmware renders concurrently into three discrete buffers for maximum refresh performance and exact timing stability across all 768 pixels.

---

## 🚀 Key Features

### 🕒 Timekeeping & Triple-Redundancy Sync
- **NTP Internet Synchronization:** Automatically fetches accurate time on Wi-Fi connection from `pool.ntp.org` / `time.nist.gov` and automatically re-seeds both hardware RTCs.
- **Dual RTC Fallback:** Defaults to the ultra-precise DS3231. If the DS3231 experiences power loss or failure, the system falls back to the PCF8563 or system clock.
- **Offline Phone BLE Sync:** 1-tap second-accurate synchronization from mobile devices without requiring Wi-Fi.
- **Digit Animations:** Smooth scrolling digit transitions, blinking colon indicator, and status icons.

### 🌤️ Live Weather Station & Solar Calculator
- **Open-Meteo Integration:** Queries current temperature and WMO weather codes.
- **Animated 16×16 Weather Icons:** Weather condition graphics for sunny, cloudy, overcast, rain, showers, snow, thunderstorm, and fog.
- **Dynamic Solar Dawn/Dusk Engine:** Automatically calculates sunrise/sunset to dynamically swap day and night icon variants.
- **Periodic Display:** Automatically transitions between clock display and weather condition cards.

### ⏰ Comprehensive Multi-Alarm & Chime System
- **4 Independent Alarms:** Each alarm features its own scheduled time, custom scrolling banner text, alarm duration (5s–600s), beep bursts (1–10 beeps), and burst intervals (0.2s–4.0s).
- **Hourly Chime:** Configurable single crisp beep (200ms) at the top of every hour.
- **Instant Test Actions:** Dedicated test triggers via Web UI, BLE, and REST API.

---

## 📱 Control Interfaces

### 1. Bluetooth Low Energy Web App (`ble_clock_app.html`)
Open [`ble_clock_app.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/ble_clock_app.html) directly in any Web Bluetooth compatible browser:
- **Android, PC, Mac, Chromebook:** Google Chrome or Microsoft Edge.
- **iOS (iPhone / iPad):** Bluefy (Free Web Bluetooth browser from the App Store).

**Capabilities:**
1. **⚡ 1-Tap Sync:** Syncs exact hour, minute, second, and date directly from your phone's clock to the RTCs over BLE.
2. **Color & Brightness:** Real-time color wheel and brightness slider.
3. **Alarm Manager:** Edit all 4 alarms, duration, beep counts, intervals, and labels.
4. **Quick Actions:** Stop alarms, test buzzer, and toggle blinking colon.

### 2. Onboard Web Portal & mDNS (`http://clock.local`)
When connected to your local network, access the device at **`http://clock.local`** or its assigned IP address. If no Wi-Fi network is configured, connect to the device's hotspot:
- **SSID:** `MatrixClock-Setup`
- **Password:** `matrixclock`
- **Portal IP:** `http://192.168.4.1`

### 3. REST API Endpoints
The embedded HTTP server exposes programmatic control endpoints:

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | `GET` | Returns full system JSON (RTC health, active source, Wi-Fi, alarms, colors, IP) |
| `/api/scroll?text=...&speed=...` | `GET` | Scrolls custom ticker text across the matrix display |
| `/api/testalarm?id=1..4` | `GET` | Triggers a test run of alarm 1, 2, 3, or 4 |
| `/api/testbuzzer` | `GET` | Sounds the buzzer for 3 seconds |
| `/api/testchime` | `GET` | Triggers the 200 ms hourly chime |
| `/api/fetchweather` | `GET` | Immediately queries Open-Meteo and refreshes weather screen |
| `/save` | `POST` | Saves display, network, and alarm configurations to persistent memory |
| `/restart` | `POST` | Restarts the ESP32 |

---

## 🛠️ Interactive Browser Tools

| Tool | File | Description |
|---|---|---|
| **Matrix Simulator** | [`matrix_clock_simulator.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/matrix_clock_simulator.html) | Full 48×16 browser simulator with pixel rendering, live weather simulation, animations, and virtual BLE sync. |
| **Weather Icon Studio** | [`weather_icon_editor.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/weather_icon_editor.html) | 16×16 pixel art creator with palette editor, live matrix preview, and direct C++ PROGMEM code generator for `weather_icons.h`. |
| **BLE Web Controller** | [`ble_clock_app.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/ble_clock_app.html) | Standalone offline mobile-friendly BLE control application. |

---

## 🔌 Wiring & Electrical Guide

### 1. Power Supply Architecture
- **5 V Regulated Power Supply:** 768 WS2812B LEDs at full white draw up to ~15–20 A; at typical display brightness (`brightness = 36–80`), average draw is ~1.5–3 A.
- Connect 5 V power supply directly to all 3 matrix power injection rails and ESP32 `5V / VIN`.
- **Common Ground:** Connect ESP32 `GND`, LED power supply `GND`, RTC `GND`, and Buzzer `GND` together.
- > [!IMPORTANT]
  > Never power the 768 LEDs from the LOLIN32 Lite's onboard 3.3 V regulator or USB port.

### 2. Dual I²C RTC Wiring
Connect both modules in parallel on the shared I²C bus:
- **DS3231 & PCF8563 SDA** → ESP32 **GPIO 23**
- **DS3231 & PCF8563 SCL** → ESP32 **GPIO 19**
- **VCC** → 3.3 V or 5 V (depending on your module's onboard regulator)
- **GND** → Common GND

### 3. SFM-27-I Active Piezoelectric Buzzer
The SFM-27-I operates from 3 V–24 V DC with an internal oscillator (~3.0–3.5 kHz).

#### Direct 3.3 V Connection (Quick Setup)
- **Red wire (+)** → ESP32 **GPIO 18**
- **Black wire (-)** → ESP32 **GND**
*(Draws ~3–5 mA at 3.3 V, well within ESP32 GPIO pin ratings).*

#### 5 V Transistor Driver (Maximum Loudness ~90–95 dB)
- **Buzzer Red (+)** → 5 V Power Supply
- **Buzzer Black (-)** → NPN Transistor Collector (2N2222, BC547, or S8050)
- **Transistor Emitter** → Common GND
- **Transistor Base** → **1 kΩ Resistor** → ESP32 **GPIO 18**
- *(Optional)* **1N4148 Diode** reverse-biased across buzzer terminals to suppress flyback spikes.

---

## 📁 Project Structure

```
esp32_matrix_clock_package/
├── include/
│   ├── alarm_manager.h      # Alarm scheduling and buzzer tone control
│   ├── ap_html.h            # Responsive web setup portal HTML/CSS/JS
│   ├── ble_manager.h        # Web Bluetooth Low Energy UUIDs and handlers
│   ├── config.h             # Hardware pins, default settings, data structures
│   ├── config_manager.h     # Non-volatile EEPROM/Flash storage routines
│   ├── display.h            # Matrix renderer, coordinate mapping, text engine
│   ├── font.h               # 5x7 digit font and 8-pixel ASCII alphabet
│   ├── rtc_ntp.h            # Dual RTC and NTP synchronization engine
│   ├── weather_icons.h      # 16x16 PROGMEM weather condition bitmaps
│   └── wifi_weather.h       # Open-Meteo REST API parser and background task
├── src/
│   ├── alarm_manager.cpp    # Buzzer sequencing, alarm checks, hourly chimes
│   ├── ble_manager.cpp      # BLE GATT service, command parsing, sync handlers
│   ├── config_manager.cpp   # EEPROM settings serialization and loading
│   ├── display.cpp          # FastLED driver, XY() mapping, animations, overlays
│   ├── main.cpp             # System setup, task orchestration, web server & API
│   ├── rtc_ntp.cpp          # DS3231 / PCF8563 / NTP logic and timekeeping
│   └── wifi_weather.cpp     # HTTP weather client, JSON deserialization, WMO codes
├── platformio.ini           # PlatformIO project configuration (lolin32_lite)
├── firmware.bin             # Pre-compiled ready-to-flash binary
├── ble_clock_app.html       # Offline Web Bluetooth control client
├── matrix_clock_simulator.html # Browser-based 48x16 matrix simulator
├── weather_icon_editor.html # 16x16 pixel art studio and code generator
├── mapping_reference.png    # Physical hardware LED matrix routing diagram
└── README.md                # Project documentation
```

---

## 🔨 Flashing & Compilation

### Option 1: PlatformIO (Recommended)
1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO IDE extension](https://platformio.org/).
2. Open this folder in PlatformIO.
3. Connect the LOLIN32 Lite via USB.
4. Compile and upload:
   ```bash
   pio run -t upload
   pio device monitor -b 115200
   ```

### Option 2: Pre-compiled Binary (`firmware.bin`)
Flash using `esptool.py`:
```bash
esptool.py --chip esp32 --port <YOUR_PORT> --baud 921600 write_flash 0x10000 firmware.bin
```

---

## 📄 License & Credits

Developed for ESP32 and LOLIN32 Lite. Utilizes [FastLED](https://github.com/FastLED/FastLED), [RTClib](https://github.com/adafruit/RTClib), and [ArduinoJson](https://arduinojson.org/). Weather data powered by [Open-Meteo](https://open-meteo.com/).
