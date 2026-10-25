#pragma once

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Hardware Pin & Matrix Layout Configuration
// -----------------------------------------------------------------------------
#define MATRIX_WIDTH 48
#define MATRIX_HEIGHT 16
#define TOTAL_LEDS 768

#define LED_PIN_1 27
#define LED_PIN_2 25
#define LED_PIN_3 26
#define BUZZER_PIN 18

#define COLOR_ORDER GRB
#define CHIPSET WS2812B

#define MATRIX1_WIDTH 32
#define MATRIX1_HEIGHT 8
#define MATRIX1_LEDS (MATRIX1_WIDTH * MATRIX1_HEIGHT)

#define MATRIX2_WIDTH 16
#define MATRIX2_HEIGHT 16
#define MATRIX2_LEDS (MATRIX2_WIDTH * MATRIX2_HEIGHT)

#define MATRIX3_WIDTH 32
#define MATRIX3_HEIGHT 8
#define MATRIX3_LEDS (MATRIX3_WIDTH * MATRIX3_HEIGHT)

#define MATRIX1_START 0
#define MATRIX2_START MATRIX1_LEDS
#define MATRIX3_START (MATRIX1_LEDS + MATRIX2_LEDS)

#define I2C_SDA 23
#define I2C_SCL 19
#define PCF8563_ADDRESS 0x51

#define SETTINGS_MAGIC 0x4D434C33UL // "MCL3" (v2.2 settings schema)
#define AP_CHANNEL 1
#define MAX_ALARMS 7
#define DEFAULT_WEATHER_INTERVAL_MIN 5

// -----------------------------------------------------------------------------
// Default WiFi & Display Settings
// -----------------------------------------------------------------------------
#define DEFAULT_STA_SSID "Damian"
#define DEFAULT_STA_PASSWORD "11223344"

#define DEFAULT_AP_SSID "MatrixClock-Setup"
#define DEFAULT_AP_PASSWORD "matrixclock"

#define DEFAULT_BRIGHTNESS 36
#define DEFAULT_RED 0
#define DEFAULT_GREEN 110
#define DEFAULT_BLUE 255
#define DEFAULT_BLINK_COLON true
#define DEFAULT_SCROLL_SPEED_MS 5
#define DEFAULT_DIGIT_ANIM_SPEED_MS 15
#define DEFAULT_BEEP_COUNT 2
#define DEFAULT_BEEP_INTERVAL_MS 1000

// -----------------------------------------------------------------------------
// Network Time Protocol (NTP) Settings
// -----------------------------------------------------------------------------
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"
#define GMT_OFFSET_SEC 3600
#define DAYLIGHT_OFFSET_SEC 3600

// -----------------------------------------------------------------------------
// Weather Settings & Expiration Timeout
// -----------------------------------------------------------------------------
#define WEATHER_EXPIRATION_MS (2UL * 60UL * 60UL * 1000UL) // 2 hours (7,200,000 ms)
#define WEATHER_AUTO_INTERVAL_MS (60UL * 60UL * 1000UL)     // 1 hour (3,600,000 ms)
#define HOTSPOT_SEARCH_INTERVAL_MS (5UL * 60UL * 1000UL)    // 5 minutes (300,000 ms)
#define DEFAULT_LATITUDE 51.599f    // Colijnsplaat, Zeeland, Netherlands
#define DEFAULT_LONGITUDE 3.849f

// -----------------------------------------------------------------------------
// Cloud OTA Firmware Update Settings
// -----------------------------------------------------------------------------
#define FIRMWARE_VERSION "2.2.0"
#define GITHUB_VERSION_URL "https://dymek84.github.io/esp32_matrix_clock_package/version.json"
#define GITHUB_FIRMWARE_URL "https://dymek84.github.io/esp32_matrix_clock_package/firmware.bin"

// -----------------------------------------------------------------------------
// Data Structures
// -----------------------------------------------------------------------------
struct AlarmSetting {
  bool enabled;
  uint8_t hour;
  uint8_t minute;
  uint16_t durationSec;
  uint16_t textIntervalSec;
  char text[33];
  uint8_t beepCount;         // Amount of beeps per burst (1..10, default 2)
  uint16_t beepIntervalMs;   // Time between beep bursts in ms (0 = continuous single long beep)
};

struct WeatherData {
  bool valid;
  int16_t temp;              // Temperature in Celsius (e.g. 21)
  char desc[33];             // Detailed condition text (e.g. "Sunny", "Partly Cloudy", "Thunderstorm")
  uint32_t lastUpdateEpoch;  // Timestamp
  bool hasDawnDusk;          // True if dynamic sunrise/sunset is set
  uint8_t sunriseHour;       // Dawn / sunrise hour (e.g. 7)
  uint8_t sunriseMinute;     // Dawn / sunrise minute (e.g. 22)
  uint8_t sunsetHour;        // Dusk / sunset hour (e.g. 17)
  uint8_t sunsetMinute;      // Dusk / sunset minute (e.g. 34)
};

struct DeviceSettings {
  uint32_t magic;
  uint8_t brightness;
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  bool blinkColon;
  char apSsid[33];
  char apPassword[65];
  char staSsid[33];
  char staPassword[65];
  uint8_t weatherIntervalMin; // Periodic weather display on matrix (1..30 min, default 5)
  AlarmSetting alarms[MAX_ALARMS];
};
