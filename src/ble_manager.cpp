#include "ble_manager.h"

bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic = nullptr;

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    deviceConnected = true;
    Serial.println("[BLE] Client connected!");
  }

  void onDisconnect(BLEServer *pServer) override {
    deviceConnected = false;
    Serial.println("[BLE] Client disconnected!");
  }
};

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      const String jsonPayload = String(rxValue.c_str());
      Serial.printf("[BLE] Received Payload (%d bytes): %s\n", jsonPayload.length(), jsonPayload.c_str());
      processIncomingJson(jsonPayload);
    }
  }
};

void initBLE() {
  Serial.println("[BLE] Initializing BLE GATT Server...");
  BLEDevice::init(BLE_DEVICE_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_WRITE_NR
  );

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.printf("[BLE] GATT Server active! Advertised Name: '%s', Service UUID: %s\n", BLE_DEVICE_NAME, SERVICE_UUID);
}

void processBLETask() {
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("[BLE] Restarted advertising for continuous availability.");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    // Client just connected: send initial status
    sendBLEStatus();
  }
}

void sendBLENotification(const String &message) {
  if (deviceConnected && pCharacteristic != nullptr) {
    pCharacteristic->setValue((uint8_t*)message.c_str(), message.length());
    pCharacteristic->notify();
    Serial.printf("[BLE] Sent notification: %s\n", message.c_str());
  }
}

void sendBLEStatus() {
  if (!deviceConnected || pCharacteristic == nullptr) return;

  // Packet 1: Core status & display settings
  char timeStr[16];
  char dateStr[16];
  if (clockIsValid) {
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", displayedTime.hour(), displayedTime.minute(), displayedTime.second());
    snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", displayedTime.year(), displayedTime.month(), displayedTime.day());
  } else {
    snprintf(timeStr, sizeof(timeStr), "--:--:--");
    snprintf(dateStr, sizeof(dateStr), "----/--/--");
  }

  char colorHex[8];
  snprintf(colorHex, sizeof(colorHex), "#%02X%02X%02X", settings.red, settings.green, settings.blue);

  StaticJsonDocument<256> statusDoc;
  statusDoc["type"] = "status";
  statusDoc["valid"] = clockIsValid;
  statusDoc["date"] = dateStr;
  statusDoc["time"] = timeStr;
  statusDoc["brightness"] = settings.brightness;
  statusDoc["color"] = colorHex;
  statusDoc["weatherIntervalMin"] = settings.weatherIntervalMin;
  statusDoc["firmwareVersion"] = FIRMWARE_VERSION;
  statusDoc["blink"] = settings.blinkColon;
  statusDoc["source"] = getActiveClockSource();

  String statusPayload;
  serializeJson(statusDoc, statusPayload);
  sendBLENotification(statusPayload);
  delay(50);

  // Packet 2: Alarms list with individual lengths and buzzer cadences
  StaticJsonDocument<1536> alarmsDoc;
  alarmsDoc["type"] = "alarms";
  JsonArray array = alarmsDoc.createNestedArray("alarms");

  for (int i = 0; i < MAX_ALARMS; i++) {
    JsonObject aObj = array.createNestedObject();
    aObj["id"] = i + 1;
    aObj["en"] = settings.alarms[i].enabled;
    aObj["h"] = settings.alarms[i].hour;
    aObj["m"] = settings.alarms[i].minute;
    aObj["dur"] = settings.alarms[i].durationSec;
    aObj["beeps"] = settings.alarms[i].beepCount;
    aObj["pause"] = settings.alarms[i].beepIntervalMs;
    aObj["inter"] = settings.alarms[i].textIntervalSec;
    aObj["txt"] = settings.alarms[i].text;
  }

  String alarmsPayload;
  serializeJson(alarmsDoc, alarmsPayload);
  sendBLENotification(alarmsPayload);
}

void processIncomingJson(const String &jsonPayload) {
  StaticJsonDocument<1536> doc;
  DeserializationError error = deserializeJson(doc, jsonPayload);

  if (error) {
    Serial.printf("[BLE] JSON parse failed: %s\n", error.c_str());
    sendBLENotification("{\"status\":\"error\",\"message\":\"Invalid JSON payload\"}");
    return;
  }

  bool settingsChanged = false;

  // 1. Direct Commands
  if (doc.containsKey("cmd")) {
    const String cmd = doc["cmd"].as<String>();
    if (cmd == "getStatus" || cmd == "status") {
      sendBLEStatus();
      return;
    } else if (cmd == "stopAlarm") {
      stopAlarm();
      sendBLENotification("{\"status\":\"ok\",\"message\":\"Alarm stopped\"}");
      return;
    } else if (cmd == "testAlarm") {
      const uint8_t id = doc.containsKey("id") ? doc["id"].as<uint8_t>() : 1;
      if (id >= 1 && id <= MAX_ALARMS) {
        triggerAlarmTest(id - 1);
        sendBLENotification("{\"status\":\"ok\",\"message\":\"Alarm test triggered\"}");
      }
      return;
    } else if (cmd == "testBuzzer") {
      triggerBuzzerTest(3000);
      sendBLENotification("{\"status\":\"ok\",\"message\":\"Buzzer test triggered\"}");
      return;
    } else if (cmd == "testChime") {
      triggerHourlyChime();
      sendBLENotification("{\"status\":\"ok\",\"message\":\"Hourly chime test (1 beep)\"}");
      return;
    } else if (cmd == "showWeather") {
      const uint32_t duration = doc.containsKey("duration") ? doc["duration"].as<uint32_t>() : 8000;
      showWeatherScreen(duration);
      sendBLENotification("{\"status\":\"ok\",\"message\":\"Weather screen activated\"}");
      return;
    } else if (cmd == "playAnim") {
      const uint8_t variant = doc.containsKey("variant") ? doc["variant"].as<uint8_t>() : (doc.containsKey("id") ? doc["id"].as<uint8_t>() : 255);
      const uint32_t duration = doc.containsKey("duration") ? doc["duration"].as<uint32_t>() : 5000;
      runAnimationVariant(variant, duration);
      sendBLENotification("{\"status\":\"ok\",\"message\":\"Animation completed\"}");
      return;
    } else if (cmd == "checkUpdate") {
      OtaInfo info;
      const bool avail = checkForOtaUpdate(info);
      String resp = "{\"type\":\"otaStatus\",\"available\":" + String(avail ? "true" : "false") +
                    ",\"version\":\"" FIRMWARE_VERSION "\",\"remote\":\"" + info.remoteVersion + "\",\"notes\":\"" + info.releaseNotes + "\"}";
      sendBLENotification(resp);
      return;
    } else if (cmd == "startOta" || cmd == "updateFirmware") {
      sendBLENotification("{\"status\":\"ok\",\"message\":\"OTA Update initiated... Watch matrix!\"}");
      performOtaUpdate(latestOtaInfo.firmwareUrl);
      return;
    }
  }

  // 2. Brightness
  if (doc.containsKey("brightness")) {
    settings.brightness = constrain(doc["brightness"].as<uint8_t>(), 0, 255);
    FastLED.setBrightness(settings.brightness);
    settingsChanged = true;
    Serial.printf("[BLE] Updated brightness: %d\n", settings.brightness);
  }

  // 3. Color
  if (doc["color"].is<JsonObject>()) {
    JsonObject col = doc["color"].as<JsonObject>();
    if (col.containsKey("red")) settings.red = col["red"].as<uint8_t>();
    if (col.containsKey("green")) settings.green = col["green"].as<uint8_t>();
    if (col.containsKey("blue")) settings.blue = col["blue"].as<uint8_t>();
    if (col.containsKey("r")) settings.red = col["r"].as<uint8_t>();
    if (col.containsKey("g")) settings.green = col["g"].as<uint8_t>();
    if (col.containsKey("b")) settings.blue = col["b"].as<uint8_t>();
    settingsChanged = true;
    Serial.printf("[BLE] Updated color: R=%d G=%d B=%d\n", settings.red, settings.green, settings.blue);
  } else if (doc["color"].is<const char*>()) {
    const String colStr = doc["color"].as<String>();
    if (colStr.length() == 7 && colStr[0] == '#') {
      const uint32_t packedColor = strtoul(colStr.c_str() + 1, nullptr, 16);
      settings.red = (packedColor >> 16) & 0xFF;
      settings.green = (packedColor >> 8) & 0xFF;
      settings.blue = packedColor & 0xFF;
      settingsChanged = true;
      Serial.printf("[BLE] Updated color from hex %s -> R=%d G=%d B=%d\n", colStr.c_str(), settings.red, settings.green, settings.blue);
    }
  }

  // 4. Colon Blink
  if (doc.containsKey("blinkColon")) {
    settings.blinkColon = doc["blinkColon"].as<bool>();
    settingsChanged = true;
    Serial.printf("[BLE] Updated blinkColon: %s\n", settings.blinkColon ? "true" : "false");
  }

  // 5. Date & Time Sync (PRECISE UP TO SECONDS)
  if (doc["datetime"].is<JsonObject>()) {
    JsonObject dtObj = doc["datetime"].as<JsonObject>();
    if (dtObj.containsKey("date") && dtObj.containsKey("time")) {
      int y = 0, m = 0, d = 0, hh = 0, mm = 0, ss = 0;
      const char* timeStr = dtObj["time"].as<const char*>();
      const int parsedTime = sscanf(timeStr, "%d:%d:%d", &hh, &mm, &ss);
      if (parsedTime < 3) {
        sscanf(timeStr, "%d:%d", &hh, &mm);
        if (dtObj.containsKey("second")) {
          ss = dtObj["second"].as<int>();
        } else if (dtObj.containsKey("sec")) {
          ss = dtObj["sec"].as<int>();
        } else {
          ss = 0;
        }
      }

      if (sscanf(dtObj["date"].as<const char*>(), "%d-%d-%d", &y, &m, &d) == 3 &&
          y >= 2024 && y <= 2099 && m >= 1 && m <= 12 && d >= 1 && d <= 31 &&
          hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 59) {
        setClock(DateTime(y, m, d, hh, mm, ss));
        Serial.printf("[BLE] Set clock from datetime: %04d-%02d-%02d %02d:%02d:%02d\n", y, m, d, hh, mm, ss);
        sendBLENotification("{\"status\":\"ok\",\"message\":\"Time synced up to seconds\"}");
      }
    }
  } else if (doc.containsKey("epoch") || doc.containsKey("timestamp")) {
    const uint32_t epoch = doc.containsKey("epoch") ? doc["epoch"].as<uint32_t>() : doc["timestamp"].as<uint32_t>();
    if (epoch > 1700000000) {
      setClock(DateTime(epoch));
      Serial.printf("[BLE] Clock updated from epoch: %u\n", epoch);
      sendBLENotification("{\"status\":\"ok\",\"message\":\"Time synced up to seconds\"}");
    }
  }

  // 6. Weather Forecast Update
  if (doc.containsKey("weather") || doc.containsKey("temp")) {
    int16_t temperature = 0;
    int16_t pressure = 0;
    uint8_t humidity = 0;
    uint8_t windSpeed = 0;
    const char* description = "";
    uint8_t sunriseH = 255, sunriseM = 0, sunsetH = 255, sunsetM = 0;

    if (doc["weather"].is<JsonObject>()) {
      JsonObject wObj = doc["weather"].as<JsonObject>();
      if (wObj.containsKey("temp")) temperature = wObj["temp"].as<int16_t>();
      if (wObj.containsKey("temperature")) temperature = wObj["temperature"].as<int16_t>();
      if (wObj.containsKey("pressure")) pressure = wObj["pressure"].as<int16_t>();
      if (wObj.containsKey("humidity")) humidity = wObj["humidity"].as<uint8_t>();
      if (wObj.containsKey("wind")) windSpeed = wObj["wind"].as<uint8_t>();
      if (wObj.containsKey("desc")) description = wObj["desc"].as<const char*>();
      if (wObj.containsKey("description")) description = wObj["description"].as<const char*>();
      if (wObj.containsKey("condition")) description = wObj["condition"].as<const char*>();

      if (wObj.containsKey("sunrise")) {
        const char* s = wObj["sunrise"].as<const char*>();
        int h = 0, m = 0;
        if (sscanf(s, "%d:%d", &h, &m) == 2) { sunriseH = h; sunriseM = m; }
      }
      if (wObj.containsKey("sunset")) {
        const char* s = wObj["sunset"].as<const char*>();
        int h = 0, m = 0;
        if (sscanf(s, "%d:%d", &h, &m) == 2) { sunsetH = h; sunsetM = m; }
      }
    } else {
      if (doc.containsKey("temp")) temperature = doc["temp"].as<int16_t>();
      if (doc.containsKey("pressure")) pressure = doc["pressure"].as<int16_t>();
      if (doc.containsKey("humidity")) humidity = doc["humidity"].as<uint8_t>();
      if (doc.containsKey("wind")) windSpeed = doc["wind"].as<uint8_t>();
      if (doc.containsKey("desc")) description = doc["desc"].as<const char*>();
      if (doc.containsKey("weather") && doc["weather"].is<const char*>()) {
        description = doc["weather"].as<const char*>();
      }
      if (doc.containsKey("sunrise")) {
        const char* s = doc["sunrise"].as<const char*>();
        int h = 0, m = 0;
        if (sscanf(s, "%d:%d", &h, &m) == 2) { sunriseH = h; sunriseM = m; }
      }
      if (doc.containsKey("sunset")) {
        const char* s = doc["sunset"].as<const char*>();
        int h = 0, m = 0;
        if (sscanf(s, "%d:%d", &h, &m) == 2) { sunsetH = h; sunsetM = m; }
      }
    }

    updateWeather(temperature, pressure, humidity, windSpeed, description, sunriseH, sunriseM, sunsetH, sunsetM);
    sendBLENotification("{\"status\":\"ok\",\"message\":\"Weather updated with atmospheric data & dynamic dawn/dusk\"}");
  }


  // 7. Weather Display Interval on Matrix (1..30 min)
  if (doc.containsKey("weatherIntervalMin")) {
    settings.weatherIntervalMin = constrain(doc["weatherIntervalMin"].as<uint8_t>(), 1, 30);
    settingsChanged = true;
    Serial.printf("[BLE] Updated weatherIntervalMin: %d min\n", settings.weatherIntervalMin);
  } else if (doc.containsKey("weatherInterval")) {
    settings.weatherIntervalMin = constrain(doc["weatherInterval"].as<uint8_t>(), 1, 30);
    settingsChanged = true;
    Serial.printf("[BLE] Updated weatherIntervalMin: %d min\n", settings.weatherIntervalMin);
  }

  // 8. Alarms Configuration Array ("alarms": [...])
  if (doc["alarms"].is<JsonArray>()) {
    JsonArray alarmArray = doc["alarms"].as<JsonArray>();
    for (JsonObject aObj : alarmArray) {
      uint8_t id = 0;
      if (aObj.containsKey("id")) id = aObj["id"].as<uint8_t>();
      if (id < 1 || id > MAX_ALARMS) continue;
      const uint8_t idx = id - 1;

      if (aObj.containsKey("enabled")) settings.alarms[idx].enabled = aObj["enabled"].as<bool>();
      if (aObj.containsKey("en")) settings.alarms[idx].enabled = aObj["en"].as<bool>();
      if (aObj.containsKey("hour")) settings.alarms[idx].hour = constrain(aObj["hour"].as<uint8_t>(), 0, 23);
      if (aObj.containsKey("h")) settings.alarms[idx].hour = constrain(aObj["h"].as<uint8_t>(), 0, 23);
      if (aObj.containsKey("minute")) settings.alarms[idx].minute = constrain(aObj["minute"].as<uint8_t>(), 0, 59);
      if (aObj.containsKey("m")) settings.alarms[idx].minute = constrain(aObj["m"].as<uint8_t>(), 0, 59);

      // Alarm duration / length (1 to 3600 seconds)
      if (aObj.containsKey("durationSec")) settings.alarms[idx].durationSec = constrain(aObj["durationSec"].as<uint16_t>(), 1, 3600);
      if (aObj.containsKey("length")) settings.alarms[idx].durationSec = constrain(aObj["length"].as<uint16_t>(), 1, 3600);
      if (aObj.containsKey("dur")) settings.alarms[idx].durationSec = constrain(aObj["dur"].as<uint16_t>(), 1, 3600);

      // Beeps amount (1 to 10)
      if (aObj.containsKey("beepCount")) settings.alarms[idx].beepCount = constrain(aObj["beepCount"].as<uint8_t>(), 1, 10);
      if (aObj.containsKey("beeps")) settings.alarms[idx].beepCount = constrain(aObj["beeps"].as<uint8_t>(), 1, 10);

      // Time between beeps (0 = continuous single tone for duration, up to 10000ms pause)
      if (aObj.containsKey("beepIntervalMs")) settings.alarms[idx].beepIntervalMs = constrain(aObj["beepIntervalMs"].as<uint16_t>(), 0, 10000);
      if (aObj.containsKey("pause")) settings.alarms[idx].beepIntervalMs = constrain(aObj["pause"].as<uint16_t>(), 0, 10000);
      if (aObj.containsKey("pauseMs")) settings.alarms[idx].beepIntervalMs = constrain(aObj["pauseMs"].as<uint16_t>(), 0, 10000);

      // Banner text interval & text
      if (aObj.containsKey("textIntervalSec")) settings.alarms[idx].textIntervalSec = aObj["textIntervalSec"].as<uint16_t>();
      if (aObj.containsKey("inter")) settings.alarms[idx].textIntervalSec = aObj["inter"].as<uint16_t>();
      if (aObj.containsKey("text")) strncpy(settings.alarms[idx].text, aObj["text"].as<const char*>(), sizeof(settings.alarms[idx].text) - 1);
      if (aObj.containsKey("txt")) strncpy(settings.alarms[idx].text, aObj["txt"].as<const char*>(), sizeof(settings.alarms[idx].text) - 1);

      settingsChanged = true;
      Serial.printf("[BLE] Updated Alarm %d: %02d:%02d, len=%ds, beeps=%d, pause=%dms, text='%s', en=%d\n",
                    id, settings.alarms[idx].hour, settings.alarms[idx].minute,
                    settings.alarms[idx].durationSec, settings.alarms[idx].beepCount,
                    settings.alarms[idx].beepIntervalMs, settings.alarms[idx].text,
                    settings.alarms[idx].enabled ? 1 : 0);
    }
  }

  // 9. Single Alarm Configuration Object ("alarm": {...})
  if (doc["alarm"].is<JsonObject>()) {
    JsonObject aObj = doc["alarm"].as<JsonObject>();
    uint8_t id = 1;
    if (aObj.containsKey("id")) id = aObj["id"].as<uint8_t>();
    if (id >= 1 && id <= MAX_ALARMS) {
      const uint8_t idx = id - 1;
      if (aObj.containsKey("enabled")) settings.alarms[idx].enabled = aObj["enabled"].as<bool>();
      if (aObj.containsKey("en")) settings.alarms[idx].enabled = aObj["en"].as<bool>();
      if (aObj.containsKey("hour")) settings.alarms[idx].hour = constrain(aObj["hour"].as<uint8_t>(), 0, 23);
      if (aObj.containsKey("h")) settings.alarms[idx].hour = constrain(aObj["h"].as<uint8_t>(), 0, 23);
      if (aObj.containsKey("minute")) settings.alarms[idx].minute = constrain(aObj["minute"].as<uint8_t>(), 0, 59);
      if (aObj.containsKey("m")) settings.alarms[idx].minute = constrain(aObj["m"].as<uint8_t>(), 0, 59);

      // Alarm duration / length
      if (aObj.containsKey("durationSec")) settings.alarms[idx].durationSec = constrain(aObj["durationSec"].as<uint16_t>(), 1, 3600);
      if (aObj.containsKey("length")) settings.alarms[idx].durationSec = constrain(aObj["length"].as<uint16_t>(), 1, 3600);
      if (aObj.containsKey("dur")) settings.alarms[idx].durationSec = constrain(aObj["dur"].as<uint16_t>(), 1, 3600);

      // Beeps amount
      if (aObj.containsKey("beepCount")) settings.alarms[idx].beepCount = constrain(aObj["beepCount"].as<uint8_t>(), 1, 10);
      if (aObj.containsKey("beeps")) settings.alarms[idx].beepCount = constrain(aObj["beeps"].as<uint8_t>(), 1, 10);

      // Time between beeps
      if (aObj.containsKey("beepIntervalMs")) settings.alarms[idx].beepIntervalMs = constrain(aObj["beepIntervalMs"].as<uint16_t>(), 0, 10000);
      if (aObj.containsKey("pause")) settings.alarms[idx].beepIntervalMs = constrain(aObj["pause"].as<uint16_t>(), 0, 10000);
      if (aObj.containsKey("pauseMs")) settings.alarms[idx].beepIntervalMs = constrain(aObj["pauseMs"].as<uint16_t>(), 0, 10000);

      // Banner text interval & text
      if (aObj.containsKey("textIntervalSec")) settings.alarms[idx].textIntervalSec = aObj["textIntervalSec"].as<uint16_t>();
      if (aObj.containsKey("inter")) settings.alarms[idx].textIntervalSec = aObj["inter"].as<uint16_t>();
      if (aObj.containsKey("text")) strncpy(settings.alarms[idx].text, aObj["text"].as<const char*>(), sizeof(settings.alarms[idx].text) - 1);
      if (aObj.containsKey("txt")) strncpy(settings.alarms[idx].text, aObj["txt"].as<const char*>(), sizeof(settings.alarms[idx].text) - 1);

      settingsChanged = true;
      Serial.printf("[BLE] Updated Alarm %d: %02d:%02d, len=%ds, beeps=%d, pause=%dms, text='%s', en=%d\n",
                    id, settings.alarms[idx].hour, settings.alarms[idx].minute,
                    settings.alarms[idx].durationSec, settings.alarms[idx].beepCount,
                    settings.alarms[idx].beepIntervalMs, settings.alarms[idx].text,
                    settings.alarms[idx].enabled ? 1 : 0);
    }
  }

  // 10. Custom Scrolling Text Banner (Queued safely to main loop to prevent collision with clock digits)
  if (doc.containsKey("text") && !doc.containsKey("alarms") && !doc.containsKey("alarm")) {
    const String textStr = doc["text"].as<String>();
    Serial.printf("[BLE] Queueing scroll text banner: '%s'\n", textStr.c_str());
    queueScrollText(textStr);
  }

  if (settingsChanged) {
    saveSettings();
    Serial.println("[BLE] Settings persisted to NVS.");
  }

  sendBLENotification("{\"status\":\"ok\",\"message\":\"Configuration applied successfully\"}");
}
