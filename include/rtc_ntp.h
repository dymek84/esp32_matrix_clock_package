#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <RTClib.h>
#include "config.h"

enum TimeSource {
  TIME_SOURCE_NONE,
  TIME_SOURCE_RTC_DS3231,
  TIME_SOURCE_RTC_PCF8563,
  TIME_SOURCE_NTP,
  TIME_SOURCE_MANUAL,
  TIME_SOURCE_ESP32_SYSTEM
};

extern RTC_DS3231 ds3231;
extern bool ds3231Present;
extern bool ds3231TimeValid;
extern bool pcf8563Present;
extern bool clockIsValid;
extern DateTime displayedTime;
extern TimeSource currentTimeSource;

void initialiseRtc();
void testRtcConnections();
bool readActiveClock(DateTime &value);
bool isRTC1Valid();
bool isRTC2Valid();

void setClock(const DateTime &value, TimeSource source = TIME_SOURCE_MANUAL);
void updateClock();
bool syncNtpTime(uint32_t timeoutMs = 5000);
const char* getTimeSourceString(TimeSource source);
const char* getActiveClockSource();
