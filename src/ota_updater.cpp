#include "ota_updater.h"
#include "display.h"

OtaInfo latestOtaInfo = { false, FIRMWARE_VERSION, "", "", "" };
bool isOtaUpdating = false;

static uint32_t lastOtaCheckMs = 0;
static const uint32_t OTA_CHECK_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL; // 24 hours

static bool parseVersionNumbers(const String &verStr, int &major, int &minor, int &patch) {
  major = 0; minor = 0; patch = 0;
  String v = verStr;
  v.trim();
  if (v.startsWith("v") || v.startsWith("V")) {
    v = v.substring(1);
  }
  const int firstDot = v.indexOf('.');
  if (firstDot < 0) return false;
  major = v.substring(0, firstDot).toInt();

  const int secondDot = v.indexOf('.', firstDot + 1);
  if (secondDot < 0) {
    minor = v.substring(firstDot + 1).toInt();
    patch = 0;
  } else {
    minor = v.substring(firstDot + 1, secondDot).toInt();
    patch = v.substring(secondDot + 1).toInt();
  }
  return true;
}

static bool isRemoteVersionNewer(const String &remoteVer, const String &localVer) {
  int rMaj = 0, rMin = 0, rPatch = 0;
  int lMaj = 0, lMin = 0, lPatch = 0;

  if (!parseVersionNumbers(remoteVer, rMaj, rMin, rPatch)) return false;
  if (!parseVersionNumbers(localVer, lMaj, lMin, lPatch)) return false;

  if (rMaj > lMaj) return true;
  if (rMaj < lMaj) return false;

  if (rMin > lMin) return true;
  if (rMin < lMin) return false;

  return (rPatch > lPatch);
}

void initOtaUpdater() {
  latestOtaInfo.currentVersion = FIRMWARE_VERSION;
  latestOtaInfo.updateAvailable = false;
  lastOtaCheckMs = 0;
}

bool checkForOtaUpdate(OtaInfo &info) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] Cannot check for updates: Wi-Fi not connected.");
    return false;
  }

  Serial.println("[OTA] Checking GitHub for new firmware version...");
  WiFiClientSecure client;
  client.setInsecure(); // GitHub Pages / CDN uses standard TLS

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(8000);
  http.setUserAgent("ESP32-MatrixClock-OTA/" FIRMWARE_VERSION);

  if (!http.begin(client, GITHUB_VERSION_URL)) {
    Serial.println("[OTA] HTTP begin failed for version check URL");
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Version check HTTP GET failed, code: %d\n", httpCode);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
    return false;
  }

  const String remoteVer = doc["version"] | "";
  const String fUrl = doc["firmware_url"] | GITHUB_FIRMWARE_URL;
  const String notes = doc["notes"] | "";

  info.currentVersion = FIRMWARE_VERSION;
  info.remoteVersion = remoteVer;
  info.firmwareUrl = fUrl;
  info.releaseNotes = notes;

  if (remoteVer.length() > 0 && isRemoteVersionNewer(remoteVer, FIRMWARE_VERSION)) {
    info.updateAvailable = true;
    Serial.printf("[OTA] NEW VERSION AVAILABLE! Current: v%s -> Remote: v%s\n", FIRMWARE_VERSION, remoteVer.c_str());
    Serial.printf("[OTA] Firmware URL: %s\n", fUrl.c_str());
    Serial.printf("[OTA] Release Notes: %s\n", notes.c_str());
  } else {
    info.updateAvailable = false;
    Serial.printf("[OTA] Firmware is up-to-date (v%s).\n", FIRMWARE_VERSION);
  }

  latestOtaInfo = info;
  return info.updateAvailable;
}

bool performOtaUpdate(const String &url) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[OTA] Cannot perform update: Wi-Fi disconnected.");
    return false;
  }

  isOtaUpdating = true;
  const String targetUrl = (url.length() > 0) ? url : GITHUB_FIRMWARE_URL;
  Serial.printf("[OTA] Starting Over-The-Air firmware download from: %s\n", targetUrl.c_str());

  char otaMsg[64];
  snprintf(otaMsg, sizeof(otaMsg), "OTA Update: v%s", latestOtaInfo.remoteVersion.length() > 0 ? latestOtaInfo.remoteVersion.c_str() : "New");
  scrollText(otaMsg, 4, CRGB::Yellow);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(25000); // 25s timeout for downloading binary
  http.setUserAgent("ESP32-MatrixClock-OTA/" FIRMWARE_VERSION);

  if (!http.begin(client, targetUrl)) {
    Serial.println("[OTA] http.begin failed for firmware download!");
    scrollText("OTA Failed: Connection Error", 5, CRGB::Red);
    isOtaUpdating = false;
    return false;
  }

  const int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Download HTTP GET failed, code: %d\n", httpCode);
    http.end();
    scrollText("OTA Failed: HTTP Error", 5, CRGB::Red);
    isOtaUpdating = false;
    return false;
  }

  const int contentLength = http.getSize();
  if (contentLength <= 0) {
    Serial.printf("[OTA] Invalid content length: %d\n", contentLength);
    http.end();
    scrollText("OTA Failed: Invalid Size", 5, CRGB::Red);
    isOtaUpdating = false;
    return false;
  }

  Serial.printf("[OTA] Firmware binary size: %d bytes (%.2f MB)\n", contentLength, (float)contentLength / (1024.0f * 1024.0f));

  if (!Update.begin(contentLength)) {
    Serial.printf("[OTA] Not enough space for OTA! Error: %d\n", Update.getError());
    http.end();
    scrollText("OTA Failed: No Space", 5, CRGB::Red);
    isOtaUpdating = false;
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  size_t written = 0;
  uint8_t buff[1024];
  int lastProgressPct = -1;

  while (http.connected() && (written < (size_t)contentLength)) {
    const size_t availableBytes = stream->available();
    if (availableBytes > 0) {
      const int bytesToRead = min(availableBytes, sizeof(buff));
      const int bytesRead = stream->readBytes(buff, bytesToRead);
      if (bytesRead > 0) {
        Update.write(buff, bytesRead);
        written += bytesRead;

        const int pct = (written * 100) / contentLength;
        if (pct != lastProgressPct && pct % 15 == 0) {
          lastProgressPct = pct;
          Serial.printf("[OTA Progress] %d%% (%d / %d bytes)\n", pct, written, contentLength);

          // Render live OTA progress bar on the LED matrix
          clearMatrix();
          const uint8_t ledsFilled = (pct * MATRIX_WIDTH) / 100;
          for (int x = 0; x < ledsFilled; x++) {
            setPixel(x, 7, CRGB::Cyan);
            setPixel(x, 8, CRGB::Cyan);
          }
          FastLED.show();
        }
      }
    } else {
      delay(5);
    }
  }

  http.end();

  if (written == (size_t)contentLength) {
    Serial.println("[OTA] Written full image successfully!");
  } else {
    Serial.printf("[OTA] Written only %d of %d bytes!\n", written, contentLength);
  }

  if (Update.end()) {
    if (Update.isFinished()) {
      Serial.println("[OTA] Firmware Update SUCCESSFUL! Rebooting into new firmware...");
      scrollText("Update Success! Rebooting...", 4, CRGB::Green);
      delay(1000);
      ESP.restart();
      return true;
    } else {
      Serial.println("[OTA] Update not finished?");
    }
  } else {
    Serial.printf("[OTA] Update.end failed! Error #: %d\n", Update.getError());
  }

  scrollText("OTA Failed: Verify Error", 5, CRGB::Red);
  isOtaUpdating = false;
  return false;
}

void checkOtaBackground() {
  if (WiFi.status() != WL_CONNECTED || isOtaUpdating) return;

  const uint32_t now = millis();
  // Check on first connect (after 10s boot grace period) or every 24 hours
  if (lastOtaCheckMs == 0 || (now - lastOtaCheckMs >= OTA_CHECK_INTERVAL_MS)) {
    lastOtaCheckMs = now;
    OtaInfo info;
    if (checkForOtaUpdate(info)) {
      Serial.printf("[OTA] Auto-triggering cloud update to version %s!\n", info.remoteVersion.c_str());
      performOtaUpdate(info.firmwareUrl);
    }
  }
}
