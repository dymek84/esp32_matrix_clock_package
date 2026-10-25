#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include "config.h"
#include "config_manager.h"
#include "display.h"
#include "rtc_ntp.h"
#include "ap_html.h"
#include "alarm_manager.h"
#include "ble_manager.h"
#include "wifi_weather.h"
#include "ota_updater.h"

WebServer server(80);
static uint32_t lastFrameMs = 0;

// -----------------------------------------------------------------------------
// Web Server Request Handlers
// -----------------------------------------------------------------------------
String jsonEscape(const String &value) {
  String result;
  for (size_t i = 0; i < value.length(); i++) {
    const char c = value[i];
    if (c == '\\' || c == '"') result += '\\';
    result += c;
  }
  return result;
}

void handleStatus() {
  const String date = clockIsValid ? String(displayedTime.year()) + "-" + (displayedTime.month() < 10 ? "0" : "") + String(displayedTime.month()) + "-" + (displayedTime.day() < 10 ? "0" : "") + String(displayedTime.day()) : "";
  const String time = clockIsValid ? (displayedTime.hour() < 10 ? "0" : "") + String(displayedTime.hour()) + ":" + (displayedTime.minute() < 10 ? "0" : "") + String(displayedTime.minute()) : "";
  char colorHex[8];
  snprintf(colorHex, sizeof(colorHex), "#%02X%02X%02X", settings.red, settings.green, settings.blue);

  String json = "{";
  json += "\"status\":\"" + String(clockIsValid ? "Clock running" : "Time not set") + " | Source: " + String(getActiveClockSource()) + "\",";
  json += "\"rtcSource\":\"" + String(getActiveClockSource()) + "\",";
  json += "\"ds3231\":\"" + String(ds3231Present ? (ds3231TimeValid ? "found (valid)" : "found (lost power)") : "not found") + "\",";
  json += "\"pcf8563\":\"" + String(pcf8563Present ? "found" : "not found") + "\",";
  json += "\"date\":\"" + date + "\",\"time\":\"" + time + "\",";
  json += "\"brightness\":" + String(settings.brightness) + ",\"color\":\"" + colorHex + "\",";
  json += "\"blink\":" + String(settings.blinkColon ? "true" : "false") + ",";
  json += "\"apSsid\":\"" + jsonEscape(settings.apSsid) + "\",";
  json += "\"staSsid\":\"" + jsonEscape(settings.staSsid) + "\",";
  json += "\"apIp\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"staIp\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "") + "\",";
  json += "\"weatherIntervalMin\":" + String(settings.weatherIntervalMin) + ",";
  json += "\"firmwareVersion\":\"" FIRMWARE_VERSION "\",";
  json += "\"updateAvailable\":" + String(latestOtaInfo.updateAvailable ? "true" : "false") + ",";
  json += "\"remoteVersion\":\"" + latestOtaInfo.remoteVersion + "\",";
  json += "\"hostname\":\"clock.local\",";

  json += "\"alarms\":[";
  for (int i = 0; i < MAX_ALARMS; i++) {
    char alarmTimeStr[6];
    snprintf(alarmTimeStr, sizeof(alarmTimeStr), "%02d:%02d", settings.alarms[i].hour, settings.alarms[i].minute);
    json += "{\"id\":" + String(i + 1) + ",";
    json += "\"enabled\":" + String(settings.alarms[i].enabled ? "true" : "false") + ",";
    json += "\"hour\":" + String(settings.alarms[i].hour) + ",";
    json += "\"minute\":" + String(settings.alarms[i].minute) + ",";
    json += "\"time\":\"" + String(alarmTimeStr) + "\",";
    json += "\"length\":" + String(settings.alarms[i].durationSec) + ",";
    json += "\"durationSec\":" + String(settings.alarms[i].durationSec) + ",";
    json += "\"beepCount\":" + String(settings.alarms[i].beepCount) + ",";
    json += "\"beepIntervalMs\":" + String(settings.alarms[i].beepIntervalMs) + ",";
    json += "\"interval\":" + String(settings.alarms[i].textIntervalSec) + ",";
    json += "\"textIntervalSec\":" + String(settings.alarms[i].textIntervalSec) + ",";
    json += "\"text\":\"" + jsonEscape(settings.alarms[i].text) + "\"}";
    if (i < MAX_ALARMS - 1) json += ",";
  }
  json += "]}";

  server.send(200, "application/json", json);
}

bool parseDateTime(const String &date, const String &time, DateTime &result) {
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
    return false;
  }
  const int parsedTime = sscanf(time.c_str(), "%d:%d:%d", &hour, &minute, &second);
  if (parsedTime < 2) {
    return false;
  }
  if (parsedTime == 2) {
    second = 0;
  }
  if (year < 2024 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
    return false;
  }
  result = DateTime(year, month, day, hour, minute, second);
  return true;
}

void copyFormValue(const char *name, char *destination, size_t destinationLength, bool keepIfEmpty) {
  if (!server.hasArg(name)) return;
  const String value = server.arg(name);
  if (keepIfEmpty && value.length() == 0) return;
  value.toCharArray(destination, destinationLength);
}

void handleSave() {
  bool timeUpdated = false;
  bool blinkUpdated = false;
  bool wifiUpdated = false;
  DateTime requestedTime;

  // 1. Date & Time (Optional)
  if (server.hasArg("date") && server.hasArg("time") &&
      server.arg("date").length() > 0 && server.arg("time").length() > 0) {
    if (parseDateTime(server.arg("date"), server.arg("time"), requestedTime)) {
      setClock(requestedTime);
      timeUpdated = true;
    }
  }

  // 2. Brightness (Optional, immediate visual feedback - no scroll text)
  if (server.hasArg("brightness") && server.arg("brightness").length() > 0) {
    settings.brightness = constrain(server.arg("brightness").toInt(), 0, 255);
  }

  // 3. Display Color (Optional, immediate visual feedback - no scroll text)
  if (server.hasArg("color") && server.arg("color").length() > 0) {
    const String color = server.arg("color");
    if (color.length() == 7 && color[0] == '#') {
      const uint32_t packedColor = strtoul(color.c_str() + 1, nullptr, 16);
      const uint8_t newR = (packedColor >> 16) & 0xFF;
      const uint8_t newG = (packedColor >> 8) & 0xFF;
      const uint8_t newB = packedColor & 0xFF;
      if (settings.red != newR || settings.green != newG || settings.blue != newB) {
        settings.red = newR;
        settings.green = newG;
        settings.blue = newB;
        triggerBuzzerTest(3000);
      }
    }
  }

  // 4. Colon Blink (Optional)
  if (server.hasArg("blink_submitted")) {
    const bool oldBlink = settings.blinkColon;
    settings.blinkColon = server.hasArg("blink");
    if (oldBlink != settings.blinkColon) {
      blinkUpdated = true;
    }
  }

  // 5. AP SSID & Password (Optional)
  if (server.hasArg("apSsid") && server.arg("apSsid").length() > 0) {
    copyFormValue("apSsid", settings.apSsid, sizeof(settings.apSsid), false);
  }
  if (server.hasArg("apPassword") && server.arg("apPassword").length() >= 8) {
    copyFormValue("apPassword", settings.apPassword, sizeof(settings.apPassword), true);
  }

  // 6. Home Wi-Fi Credentials (Optional)
  if (server.hasArg("staSsid") && server.arg("staSsid").length() > 0) {
    copyFormValue("staSsid", settings.staSsid, sizeof(settings.staSsid), false);
    wifiUpdated = true;
  }
  if (server.hasArg("staPassword")) {
    copyFormValue("staPassword", settings.staPassword, sizeof(settings.staPassword), true);
  }

  // 7. Weather Display Interval (Optional: 1..30 min)
  if (server.hasArg("weatherIntervalMin") && server.arg("weatherIntervalMin").length() > 0) {
    settings.weatherIntervalMin = constrain(server.arg("weatherIntervalMin").toInt(), 1, 30);
  }

  saveSettings();
  server.send(200, "text/html", "<meta name=\"viewport\" content=\"width=device-width\"><body style=\"font-family:system-ui;padding:24px\"><h1>Saved</h1><p>Settings saved successfully.</p><p><a href=\"/\">Return to settings</a></p></body>");

  // Scroll notification banners on clock display
  if (timeUpdated) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Time Set: %02d:%02d", requestedTime.hour(), requestedTime.minute());
    scrollText(msg, DEFAULT_SCROLL_SPEED_MS, CRGB::Cyan);
  } else if (wifiUpdated) {
    char msg[64];
    snprintf(msg, sizeof(msg), "WiFi Updated: %s", settings.staSsid);
    scrollText(msg, DEFAULT_SCROLL_SPEED_MS, CRGB::Cyan);
  } else if (blinkUpdated) {
    const String msg = String("Blink: ") + (settings.blinkColon ? "ON" : "OFF");
    scrollText(msg, DEFAULT_SCROLL_SPEED_MS, CRGB::Cyan);
  }
}

void handleScrollApi() {
  if (server.hasArg("text")) {
    const String text = server.arg("text");
    const uint16_t speed = server.hasArg("speed") ? server.arg("speed").toInt() : DEFAULT_SCROLL_SPEED_MS;
    server.send(200, "text/plain", "Scrolling text...");
    scrollText(text, speed, CRGB(settings.red, settings.green, settings.blue));
  } else {
    server.send(400, "text/plain", "Missing text parameter");
  }
}

// -----------------------------------------------------------------------------
// Network, mDNS & Web Server Initialization
// -----------------------------------------------------------------------------
bool connectWiFi(uint32_t timeoutMs = 8000) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(settings.apSsid, settings.apPassword, AP_CHANNEL, false, 4);

  if (strlen(settings.staSsid) > 0) {
    Serial.printf("Connecting to Wi-Fi SSID: '%s'...\n", settings.staSsid);
    WiFi.begin(settings.staSsid, settings.staPassword);

    const uint32_t startMs = millis();
    while (millis() - startMs < timeoutMs) {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected to Wi-Fi! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
      }
      delay(250);
      Serial.print(".");
    }
    Serial.println("\nWi-Fi connection timed out. Fallback to AP mode.");
  }
  return false;
}

void startMDNS() {
  if (MDNS.begin("clock")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] Responder started successfully: http://clock.local");
  } else {
    Serial.println("[mDNS] Error setting up mDNS responder!");
  }
}

void startWebServer() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", AP_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/scroll", HTTP_GET, handleScrollApi);
  server.on("/api/testalarm", HTTP_GET, []() {
    if (server.hasArg("id")) {
      const int id = server.arg("id").toInt() - 1;
      if (id >= 0 && id < MAX_ALARMS) {
        triggerAlarmTest(id);
        server.send(200, "text/plain", "Triggered alarm " + String(id + 1));
        return;
      }
    }
    server.send(400, "text/plain", "Usage: /api/testalarm?id=1.." + String(MAX_ALARMS));
  });
  server.on("/api/weatherinterval", HTTP_GET, []() {
    if (server.hasArg("min")) {
      settings.weatherIntervalMin = constrain(server.arg("min").toInt(), 1, 30);
      saveSettings();
      server.send(200, "text/plain", "Weather interval set to " + String(settings.weatherIntervalMin) + " minutes");
      return;
    }
    server.send(200, "text/plain", String(settings.weatherIntervalMin));
  });
  server.on("/api/testbuzzer", HTTP_GET, []() {
    triggerBuzzerTest(3000);
    server.send(200, "text/plain", "Triggered buzzer test for 3 seconds");
  });
  server.on("/api/testchime", HTTP_GET, []() {
    triggerHourlyChime();
    server.send(200, "text/plain", "Triggered hourly chime (1 beep)");
  });
  server.on("/api/fetchweather", HTTP_GET, []() {
    if (fetchAndDisplayWeather()) {
      server.send(200, "text/plain", "Weather fetched and displayed!");
    } else {
      server.send(500, "text/plain", "Failed to fetch weather. Check Wi-Fi connection.");
    }
  });
  server.on("/api/checkupdate", HTTP_GET, []() {
    OtaInfo info;
    const bool available = checkForOtaUpdate(info);
    String resp = "{";
    resp += "\"available\":" + String(available ? "true" : "false") + ",";
    resp += "\"current\":\"" FIRMWARE_VERSION "\",";
    resp += "\"remote\":\"" + info.remoteVersion + "\",";
    resp += "\"notes\":\"" + jsonEscape(info.releaseNotes) + "\",";
    resp += "\"url\":\"" + info.firmwareUrl + "\"}";
    server.send(200, "application/json", resp);
  });
  server.on("/api/update", HTTP_GET, []() {
    if (latestOtaInfo.firmwareUrl.length() == 0) {
      checkForOtaUpdate(latestOtaInfo);
    }
    server.send(200, "text/plain", "Starting OTA Firmware Update from GitHub... Check Matrix LEDs for progress!");
    delay(200);
    performOtaUpdate(latestOtaInfo.firmwareUrl);
  });
  server.on("/save", HTTP_POST, handleSave);
  server.on("/restart", HTTP_POST, []() {
    server.send(200, "text/html", "<p>Restarting…</p>");
    delay(250);
    ESP.restart();
  });
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

// -----------------------------------------------------------------------------
// Setup & Loop
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n--- ESP32 Matrix Clock v. " FIRMWARE_VERSION " Starting ---");

  loadSettings();
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  initDisplay();
  initialiseRtc();
  initAlarmManager();
  initBLE();
  initOtaUpdater();

  const bool wifiConnected = connectWiFi(8000);
  startMDNS();
  initWifiWeather();

  if (wifiConnected) {
    syncNtpTime(5000);
    fetchAndDisplayWeather();
    // Check for OTA updates on boot if connected
    OtaInfo bootOta;
    if (checkForOtaUpdate(bootOta)) {
      Serial.printf("[Boot OTA] New firmware v%s found! Performing auto-update...\n", bootOta.remoteVersion.c_str());
      performOtaUpdate(bootOta.firmwareUrl);
    }
  } else {
    // No Wi-Fi available: fetch time from active hardware RTC
    if (readActiveClock(displayedTime)) {
      clockIsValid = true;
      Serial.printf("[Boot] No Wi-Fi. Successfully loaded RTC time: %04d-%02d-%02d %02d:%02d:%02d (%s)\n",
                    displayedTime.year(), displayedTime.month(), displayedTime.day(),
                    displayedTime.hour(), displayedTime.minute(), displayedTime.second(),
                    getActiveClockSource());
    } else {
      Serial.println("[Boot] No Wi-Fi and no valid RTC hardware time found!");
    }
  }

  // 5-second random showcase animation to demonstrate matrix possibilities (3 variants)
  playStartupAnimation();

  const String startupMsg = "Clock v. " FIRMWARE_VERSION " ";
  Serial.printf("Startup scroll: %s\n", startupMsg.c_str());
  scrollText(startupMsg, DEFAULT_SCROLL_SPEED_MS, CRGB(settings.red, settings.green, settings.blue), 2);

  startWebServer();
  Serial.println("Web server running.");
}

void loop() {
  server.handleClient();
  updateClock();
  checkAndRunAlarms(clockIsValid, displayedTime);
  updateBuzzer();
  processBLETask();
  processWifiWeatherTask();
  checkOtaBackground();
  processPendingScroll();
  checkPeriodicWeatherDisplay();

  if (millis() - lastFrameMs >= 40) {  // 25 FPS
    lastFrameMs = millis();
    renderClock(clockIsValid, displayedTime);
  }
}


