#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

#include "config.h"

struct OtaInfo {
  bool updateAvailable;
  String currentVersion;
  String remoteVersion;
  String firmwareUrl;
  String releaseNotes;
};

extern OtaInfo latestOtaInfo;
extern bool isOtaUpdating;

void initOtaUpdater();
bool checkForOtaUpdate(OtaInfo &info);
bool performOtaUpdate(const String &url = "");
void checkOtaBackground();
