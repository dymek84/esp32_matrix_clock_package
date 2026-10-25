# LOLIN32 Lite Matrix Clock v2.2.0

An advanced **48 × 16 RGB LED Matrix Clock, Weather Station, Cloud OTA System, and Smart Alarm Manager** built for the LOLIN32 Lite (ESP32). Features triple-output WS2812B display mapping, dual redundant hardware RTCs (DS3231 + PCF8563), automated NTP internet time sync, Open-Meteo live weather with high-contrast 16×16 pixel icons (Colijnsplaat, NL), automated GitHub Cloud OTA updates, phone hotspot background scanning, Bluetooth Low Energy (BLE) mobile controls, customizable 7-alarm break manager with continuous tone signals, hourly chimes, and an onboard web dashboard with mDNS and REST APIs.

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
| **Network Modes** | Wi-Fi Station (home network / phone hotspot) + SoftAP (`MatrixClock-Setup`) with mDNS |
| **Weather Engine** | Open-Meteo REST API (Colijnsplaat, NL `51.599° N, 3.849° E`), solar dawn/dusk engine |
| **Firmware Updates** | Automated GitHub Pages Cloud OTA + Web Bluetooth OTA trigger (`min_spiffs.csv`) |

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
- **NTP Internet Synchronization:** Automatically fetches accurate time on Wi-Fi connection from `pool.ntp.org` / `time.nist.gov` and re-seeds both hardware RTCs.
- **Dual RTC Fallback:** Defaults to the ultra-precise DS3231. If the DS3231 experiences power loss or failure, the system falls back to the PCF8563 or system clock.
- **Offline Phone BLE Sync:** 1-tap second-accurate synchronization from mobile devices without requiring Wi-Fi.
- **Digit Animations:** Smooth scrolling digit transitions, blinking colon indicator, and 3 startup matrix showcase animations (Aurora, Cyber Matrix Rain, Supernova).

### 🌤️ Live Weather Station & Solar Calculator
- **Open-Meteo Integration:** Queries temperature and WMO weather codes calibrated for **Colijnsplaat, NL** (`51.599° N, 3.849° E`).
- **Modern 16×16 Weather Icons:** Pixel art graphics for sunny, clear moon, cloudy, overcast, rain, sun/moon showers, snow, thunderstorm, and fog with automatic day/night color palettes.
- **Dynamic Solar Dawn/Dusk Engine:** Calculates sunrise and sunset times to swap day and night icon variants automatically.
- **Periodic Weather Overlay (1–30 min):** Cycles between clock display and full 16×16 weather card every 5 minutes by default (configurable from 1 to 30 minutes via BLE, Web Portal, or REST API).
- **5-Minute Phone Hotspot Scanner:** Periodically checks for your phone hotspot (`Damian` / `11223344`) every 5 minutes when deployed in offline environments.

### 🚀 Cloud OTA Auto-Updates (GitHub Pages)
- **Zero-Cable Updates:** Flash the clock with a cable once. Thereafter, all new versions pushed to GitHub are automatically downloaded and flashed over your phone hotspot.
- **Dual Partition Safety (`min_spiffs.csv`):** Allocates two ~1.9 MB OTA slots (`app0` & `app1`) with rollback protection.
- **Live LED Progress Bar:** The 48×16 matrix visualizes real-time download and flashing progress (`[====    ]`) before safely rebooting into the new version.
- **Cloud Check Trigger:** Check for updates on demand via Bluetooth in `ble_clock_app.html`, REST API (`/api/checkupdate` and `/api/update`), or automatically upon connecting to Wi-Fi.

### ⏰ Work Shift & Break Multi-Alarm System
Pre-configured for work and break schedules, with custom scrolling banner text, individual durations, beep bursts, and continuous tone modes:

| Alarm # | Time | Type | Duration | Buzzer Cadence | Matrix Text |
|---|---|---|---|---|---|
| **1** | `09:29` | Morning Break 1 Starts | 60 sec | 2 Beeps (1.0s pause) | "Pause" |
| **2** | `09:55` | Morning Break 1 Ends | **5 sec** | **Continuous Tone** (0 pause) | "End Pause" |
| **3** | `12:29` | Lunch Break 2 Starts | 60 sec | 2 Beeps (1.0s pause) | "Pause" |
| **4** | `12:55` | Lunch Break 2 Ends | **5 sec** | **Continuous Tone** (0 pause) | "End Pause" |
| **5** | `14:59` | Coffee Break Starts | 30 sec | 3 Beeps (1.5s pause) | "Coffie" |
| **6** | `15:10` | Coffee Break Ends | **3 sec** | **Continuous Tone** (0 pause) | "End Coffie" |
| **7** | `15:55` | Shift Finish / Day End | 300 sec | 2 Beeps (1.0s pause) | "Fininsh" |

- **Continuous Tone Support:** Setting pause between beeps to `0` holds the active buzzer high continuously for the specified duration (5s or 3s).
- **Hourly Chime:** Configurable single crisp beep (200ms) at the top of every hour.

---

## 📱 Control Interfaces

### 1. Bluetooth Low Energy Web App (`ble_clock_app.html`)
Host on GitHub Pages or open [`ble_clock_app.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/ble_clock_app.html) in any Web Bluetooth browser:
- **Direct Online URL:** [https://dymek84.github.io/esp32_matrix_clock_package/ble_clock_app.html](https://dymek84.github.io/esp32_matrix_clock_package/ble_clock_app.html)
- **Supported Browsers:** Google Chrome or Microsoft Edge (Android, PC, Mac, Linux). For iOS, use the free **Bluefy** app from the App Store.

**Capabilities:**
1. **⚡ 1-Tap Phone Sync:** Synchronizes exact second-level time and date from phone to the clock's hardware RTCs.
2. **Cloud OTA Updates:** Check GitHub for newer firmware versions and flash the clock over Bluetooth with 1 click.
3. **Weather Forecast & Push:** Pushes mobile cellular weather to the clock even when no Wi-Fi or hotspot is active.
4. **Display Frequency Slider:** Configures how often the 16×16 weather screen displays (1 to 30 minutes).
5. **Alarm Configuration:** Edit times, durations, beep cadences, continuous tones, and scrolling text for all 7 alarms.
6. **Display & Style:** Live color picker, brightness slider, and colon blink toggle.
7. **Animation Showcase:** Triggers 5-second demos of the 3 matrix animations.

### 2. Onboard Web Portal & mDNS (`http://clock.local`)
When connected to your local network, access the device at **`http://clock.local`** or its assigned IP address. If no Wi-Fi network is configured, connect to the device's setup access point:
- **SSID:** `MatrixClock-Setup`
- **Password:** `matrixclock`
- **Portal IP:** `http://192.168.4.1`

### 3. REST API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | `GET` | Returns full system JSON (RTC health, active source, Wi-Fi, alarms, colors, IP, weather interval, firmware version) |
| `/api/checkupdate` | `GET` | Checks GitHub Pages `version.json` for newer firmware versions |
| `/api/update` | `POST` | Triggers background Cloud OTA update and reboot |
| `/api/weatherinterval?min=1..30`| `GET` | Sets matrix weather display frequency in minutes |
| `/api/scroll?text=...&speed=...` | `GET` | Scrolls custom ticker text across the matrix display |
| `/api/testalarm?id=1..7` | `GET` | Triggers a test run of alarms 1 through 7 |
| `/api/testbuzzer` | `GET` | Sounds the buzzer for 3 seconds |
| `/api/testchime` | `GET` | Triggers the 200 ms hourly chime |
| `/api/fetchweather` | `GET` | Queries Open-Meteo and refreshes the weather screen immediately |
| `/save` | `POST` | Saves display, network, and alarm configurations to Flash EEPROM |
| `/restart` | `POST` | Restarts the ESP32 |

---

## 🛠️ Interactive Browser Tools

| Tool | File | Online / Local | Description |
|---|---|---|---|
| **Weather Icon Studio** | [`weather_icon_editor.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/weather_icon_editor.html) | [Open Studio](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/weather_icon_editor.html) | 16×16 pixel art editor pre-loaded with the v2.2.0 icon set, palette swatches, hardware matrix glow simulation, and direct C++ PROGMEM code generator for `weather_icons.h`. |
| **BLE Web Controller** | [`ble_clock_app.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/ble_clock_app.html) | [GitHub Pages App](https://dymek84.github.io/esp32_matrix_clock_package/ble_clock_app.html) | Standalone offline mobile BLE application with Cloud OTA triggers, 1-tap phone time sync, and 7-alarm editor. |
| **Matrix Simulator** | [`matrix_clock_simulator.html`](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/matrix_clock_simulator.html) | [Open Simulator](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/matrix_clock_simulator.html) | Full 48×16 browser simulator with pixel rendering, live weather simulation, animations, and virtual BLE sync. |

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
- **Direct 3.3 V:** Red wire (+) → **GPIO 18**, Black wire (-) → **GND**
- **5 V Transistor Driver (Loudest):** Red (+) → 5V, Black (-) → Collector of NPN transistor (2N2222/BC547), Emitter → GND, Base → 1 kΩ resistor → **GPIO 18**.

---

## 📁 Project Structure

```
esp32_matrix_clock_package/
├── include/
│   ├── alarm_manager.h      # 7-alarm scheduling and continuous tone buzzer control
│   ├── ap_html.h            # Responsive web setup portal HTML/CSS/JS
│   ├── ble_manager.h        # Web Bluetooth Low Energy UUIDs and command handlers
│   ├── config.h             # Hardware pins, default settings, data structures, Colijnsplaat coordinates
│   ├── config_manager.h     # Non-volatile EEPROM/Flash storage routines (SETTINGS_MAGIC)
│   ├── display.h            # FastLED 48x16 matrix renderer, coordinate mapping, text engine
│   ├── font.h               # 5x7 digit font and 8-pixel ASCII alphabet
│   ├── ota_updater.h        # GitHub Cloud OTA update engine & matrix progress bar
│   ├── rtc_ntp.h            # Dual RTC (DS3231 + PCF8563) and NTP sync engine
│   ├── weather_icons.h      # High-contrast 16x16 PROGMEM weather condition bitmaps
│   └── wifi_weather.h       # Open-Meteo REST API parser and background hotspot scanner
├── src/
│   ├── alarm_manager.cpp    # Buzzer sequencing, break schedules, continuous long beeps
│   ├── ble_manager.cpp      # BLE GATT service, command parsing, OTA triggers
│   ├── config_manager.cpp   # EEPROM settings serialization and default alarm presets
│   ├── display.cpp          # FastLED driver, XY() mapping, animations, 1-30min weather overlay
│   ├── main.cpp             # System setup, task orchestration, web server & REST APIs
│   ├── ota_updater.cpp      # HTTPS GitHub Pages OTA downloader, integrity check, reboot
│   ├── rtc_ntp.cpp          # DS3231 / PCF8563 / NTP logic and timekeeping
│   └── wifi_weather.cpp     # Open-Meteo client, 5-minute hotspot scan, WMO codes
├── platformio.ini           # PlatformIO project configuration with min_spiffs.csv OTA partition
├── firmware.bin             # Pre-compiled ready-to-flash binary (v2.2.0)
├── version.json             # GitHub Pages cloud version metadata for automated updates
├── ble_clock_app.html       # Offline Web Bluetooth control client with Cloud OTA card
├── matrix_clock_simulator.html # Browser-based 48x16 matrix simulator
├── weather_icon_editor.html # 16x16 pixel art studio pre-loaded with v2.2.0 icons & palette
├── mapping_reference.png    # Physical hardware LED matrix routing diagram
└── README.md                # Comprehensive documentation
```

---

## 🔨 Flashing & Cloud Updates

### One-Time Initial Cable Flash
Connect the LOLIN32 Lite via USB cable and flash using PlatformIO:
```powershell
& "C:\Users\dymek\.platformio\penv\Scripts\pio.exe" run -t upload
```
*(This sets up the dual OTA partition layout `min_spiffs.csv`. After this initial flash, the clock can be updated cable-free over the air.)*

### How Future Updates Work (Cable-Free from Home)
1. Edit code in PlatformIO and bump `FIRMWARE_VERSION` in [include/config.h](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/include/config.h) and [version.json](file:///c:/Users/dymek/Downloads/esp32_matrix_clock_package/version.json) (e.g. `2.3.0`).
2. Build binary:
   ```powershell
   & "C:\Users\dymek\.platformio\penv\Scripts\pio.exe" run
   Copy-Item -Path ".pio\build\lolin32_lite\firmware.bin" -Destination "firmware.bin" -Force
   ```
3. Push to GitHub:
   ```powershell
   git add .
   git commit -m "Release v2.3.0"
   git push origin main
   ```
4. **Done!** When you arrive at work and activate your phone hotspot, the clock discovers the update, displays a live progress bar on the LED matrix, and boots into the new firmware.

---

## 📄 License & Credits

Developed for ESP32 and LOLIN32 Lite. Utilizes [FastLED](https://github.com/FastLED/FastLED), [RTClib](https://github.com/adafruit/RTClib), and [ArduinoJson](https://arduinojson.org/). Weather data powered by [Open-Meteo](https://open-meteo.com/).
