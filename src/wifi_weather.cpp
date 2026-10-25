#include "wifi_weather.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#include "config.h"
#include "config_manager.h"
#include "display.h"
#include "rtc_ntp.h"

enum WifiWeatherState {
  WW_STATE_IDLE,
  WW_STATE_SCANNING,
  WW_STATE_CONNECTING
};

static WifiWeatherState wwState = WW_STATE_IDLE;
static uint32_t lastWeatherFetchMs = 0;
static uint32_t lastCheckMs = 0;
static uint32_t connectStartMs = 0;
static bool initialBootAttempt = true;

const char* getWeatherDescFromWmo(int code) {
  switch (code) {
    case 0: return "Sunny";
    case 1: return "Mainly Clear";
    case 2: return "Partly Cloudy";
    case 3: return "Overcast";
    case 45: return "Foggy";
    case 48: return "Freezing Fog";
    case 51: return "Light Drizzle";
    case 53: return "Drizzle";
    case 55: return "Heavy Drizzle";
    case 56:
    case 57: return "Freezing Rain";
    case 61: return "Light Rain";
    case 63: return "Rain";
    case 65: return "Heavy Rain";
    case 66:
    case 67: return "Freezing Rain";
    case 71: return "Light Snow";
    case 73: return "Snow";
    case 75: return "Heavy Snow";
    case 77: return "Snow Grains";
    case 80: return "Light Showers";
    case 81: return "Showers";
    case 82: return "Violent Showers";
    case 85: return "Snow Showers";
    case 86: return "Heavy Snow";
    case 95: return "Thunderstorm";
    case 96:
    case 99: return "Storm";
    default: return "Clear";
  }
}

bool fetchAndDisplayWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[AutoWeather] Cannot fetch: Wi-Fi not connected.");
    return false;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(7000);

  const String url = "http://api.open-meteo.com/v1/forecast?latitude=52.23&longitude=21.01&current=temperature_2m,weather_code,is_day&daily=sunrise,sunset&timezone=auto";
  http.setUserAgent("ESP32-MatrixClock/2.0");

  if (!http.begin(client, url)) {
    Serial.println("[AutoWeather] http.begin failed");
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[AutoWeather] HTTP GET failed, code: %d\n", httpCode);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[AutoWeather] JSON parse failed: %s\n", err.c_str());
    return false;
  }

  float tempF = doc["current"]["temperature_2m"] | 0.0f;
  int16_t temp = (int16_t)round(tempF);
  int wCode = doc["current"]["weather_code"] | 0;
  const char* desc = getWeatherDescFromWmo(wCode);

  uint8_t riseH = 7, riseM = 15, setH = 17, setM = 30;
  if (doc["daily"]["sunrise"].is<JsonArray>() && doc["daily"]["sunrise"].size() > 0) {
    const char* s = doc["daily"]["sunrise"][0];
    if (s) {
      const char* t = strchr(s, 'T');
      if (t) {
        int h = 0, m = 0;
        if (sscanf(t + 1, "%d:%d", &h, &m) == 2) {
          riseH = (uint8_t)h;
          riseM = (uint8_t)m;
        }
      }
    }
  }

  if (doc["daily"]["sunset"].is<JsonArray>() && doc["daily"]["sunset"].size() > 0) {
    const char* s = doc["daily"]["sunset"][0];
    if (s) {
      const char* t = strchr(s, 'T');
      if (t) {
        int h = 0, m = 0;
        if (sscanf(t + 1, "%d:%d", &h, &m) == 2) {
          setH = (uint8_t)h;
          setM = (uint8_t)m;
        }
      }
    }
  }

  Serial.printf("[AutoWeather] Fetched: %d°C '%s' (Dawn %02d:%02d, Dusk %02d:%02d)\n",
                temp, desc, riseH, riseM, setH, setM);

  // Update weather and activate 16x16 icon + text screen on matrix
  updateWeather(temp, desc, riseH, riseM, setH, setM);
  return true;
}

void initWifiWeather() {
  wwState = WW_STATE_IDLE;
  lastWeatherFetchMs = 0;
  lastCheckMs = millis();
  initialBootAttempt = true;
  Serial.printf("[AutoWeather] Service initialized. Hotspot target SSID: '%s'\n", settings.staSsid);
}

void processWifiWeatherTask() {
  const uint32_t now = millis();

  switch (wwState) {
    case WW_STATE_IDLE: {
      if (WiFi.status() == WL_CONNECTED) {
        // Connected: refresh weather every 1 hour (3,600,000 ms)
        if (lastWeatherFetchMs == 0 || (now - lastWeatherFetchMs >= WEATHER_AUTO_INTERVAL_MS)) {
          Serial.println("[AutoWeather] Wi-Fi connected. Fetching hourly weather update...");
          if (fetchAndDisplayWeather()) {
            lastWeatherFetchMs = millis();
            syncNtpTime(2000);
          } else {
            // If fetch failed, retry in 2 minutes
            lastWeatherFetchMs = now - WEATHER_AUTO_INTERVAL_MS + (2UL * 60UL * 1000UL);
          }
        }
        return;
      }

      // Not connected: search for hotspot every 60 seconds (or 3 seconds after boot)
      const uint32_t interval = initialBootAttempt ? 3000UL : HOTSPOT_SEARCH_INTERVAL_MS;
      if (now - lastCheckMs >= interval) {
        lastCheckMs = now;
        initialBootAttempt = false;

        const char* targetSsid = strlen(settings.staSsid) > 0 ? settings.staSsid : DEFAULT_STA_SSID;
        Serial.printf("[AutoWeather] Scanning for hotspot '%s' in background...\n", targetSsid);
        const int scanRes = WiFi.scanNetworks(true, false);
        if (scanRes == WIFI_SCAN_RUNNING) {
          wwState = WW_STATE_SCANNING;
        }
      }
      break;
    }

    case WW_STATE_SCANNING: {
      const int n = WiFi.scanComplete();
      if (n == WIFI_SCAN_RUNNING) {
        // Still scanning asynchronously: continue loop without blocking
        return;
      }
      if (n == WIFI_SCAN_FAILED) {
        Serial.println("[AutoWeather] Background scan failed. Will retry next interval.");
        wwState = WW_STATE_IDLE;
        return;
      }

      const char* targetSsid = strlen(settings.staSsid) > 0 ? settings.staSsid : DEFAULT_STA_SSID;
      bool found = false;
      for (int i = 0; i < n; i++) {
        const String foundSsid = WiFi.SSID(i);
        if (foundSsid == targetSsid || foundSsid == DEFAULT_STA_SSID) {
          found = true;
          Serial.printf("[AutoWeather] Found hotspot '%s' (RSSI: %d dBm, Ch: %d)!\n",
                        foundSsid.c_str(), WiFi.RSSI(i), WiFi.channel(i));
          break;
        }
      }
      WiFi.scanDelete();

      if (found) {
        const char* pass = strlen(settings.staPassword) > 0 ? settings.staPassword : DEFAULT_STA_PASSWORD;
        Serial.printf("[AutoWeather] Connecting to hotspot '%s'...\n", targetSsid);
        WiFi.begin(targetSsid, pass);
        connectStartMs = now;
        wwState = WW_STATE_CONNECTING;
      } else {
        Serial.printf("[AutoWeather] Hotspot '%s' not found. Retrying in %lu s.\n",
                      targetSsid, HOTSPOT_SEARCH_INTERVAL_MS / 1000UL);
        wwState = WW_STATE_IDLE;
      }
      break;
    }

    case WW_STATE_CONNECTING: {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[AutoWeather] Connected to hotspot! IP: %s\n", WiFi.localIP().toString().c_str());
        if (fetchAndDisplayWeather()) {
          lastWeatherFetchMs = millis();
          syncNtpTime(2000);
        } else {
          lastWeatherFetchMs = now - WEATHER_AUTO_INTERVAL_MS + (2UL * 60UL * 1000UL);
        }
        wwState = WW_STATE_IDLE;
        return;
      }

      // Timeout connection attempt after 10 seconds
      if (now - connectStartMs >= 10000UL) {
        Serial.println("[AutoWeather] Hotspot connection attempt timed out.");
        wwState = WW_STATE_IDLE;
      }
      break;
    }
  }
}
