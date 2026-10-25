#pragma once

#include <Arduino.h>
#include <RTClib.h>
#include "config.h"
#include "config_manager.h"
#include "display.h"

extern int activeAlarmIndex;

void initAlarmManager();
void initBuzzer();
void updateBuzzer();
void checkAndRunAlarms(bool clockIsValid, const DateTime &now);
void triggerAlarmTest(uint8_t alarmIndex);
void triggerBuzzerTest(uint32_t durationMs = 3000);
void playSingleBeep(uint16_t durationMs = 200);
void triggerHourlyChime();
void stopAlarm();
