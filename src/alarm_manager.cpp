#include "alarm_manager.h"

int activeAlarmIndex = -1;
static uint32_t alarmStartMs = 0;
static uint32_t lastTextScrollMs = 0;
static int lastTriggeredMinute[MAX_ALARMS];

static uint32_t buzzerTestEndMs = 0;
static uint32_t singleBeepEndMs = 0;
static int lastChimedHour = -1;

void initAlarmManager() {
  activeAlarmIndex = -1;
  alarmStartMs = 0;
  lastTextScrollMs = 0;
  buzzerTestEndMs = 0;
  singleBeepEndMs = 0;
  lastChimedHour = -1;
  for (int i = 0; i < MAX_ALARMS; i++) {
    lastTriggeredMinute[i] = -1;
  }
  initBuzzer();
}

void initBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

void triggerBuzzerTest(uint32_t durationMs) {
  buzzerTestEndMs = millis() + durationMs;
  Serial.printf("[Buzzer] Test activated for %u ms\n", durationMs);
}

void playSingleBeep(uint16_t durationMs) {
  singleBeepEndMs = millis() + durationMs;
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.printf("[Buzzer] Single beep triggered for %u ms\n", durationMs);
}

void triggerHourlyChime() {
  playSingleBeep(200);
  Serial.println("[Chime] Hourly chime test (200ms beep).");
}

void stopAlarm() {
  activeAlarmIndex = -1;
  buzzerTestEndMs = 0;
  singleBeepEndMs = 0;
  digitalWrite(BUZZER_PIN, LOW);
  clearMatrix();
  FastLED.show();
  Serial.println("[Alarm] Stopped.");
}

void updateBuzzer() {
  const bool isBuzzerTesting = (millis() < buzzerTestEndMs);
  const bool isSingleBeep = (millis() < singleBeepEndMs);

  if (activeAlarmIndex >= 0 && activeAlarmIndex < MAX_ALARMS) {
    const AlarmSetting &alarm = settings.alarms[activeAlarmIndex];

    // Continuous tone: if beepIntervalMs is 0, sound buzzer continuously for full duration
    if (alarm.beepIntervalMs == 0) {
      digitalWrite(BUZZER_PIN, HIGH);
      return;
    }

    const uint8_t beeps = (alarm.beepCount > 0 && alarm.beepCount <= 10) ? alarm.beepCount : DEFAULT_BEEP_COUNT;
    const uint16_t pauseMs = (alarm.beepIntervalMs >= 100 && alarm.beepIntervalMs <= 10000) ? alarm.beepIntervalMs : DEFAULT_BEEP_INTERVAL_MS;

    // Cadence: Each beep is 100ms ON, 100ms OFF.
    const uint16_t beepPeriod = 200;
    const uint32_t burstDurationMs = beeps * beepPeriod;
    const uint32_t totalCycleMs = burstDurationMs + pauseMs;

    const uint32_t cyclePos = (millis() - alarmStartMs) % totalCycleMs;
    if (cyclePos < burstDurationMs) {
      const uint16_t beepPhase = cyclePos % beepPeriod;
      if (beepPhase < 100) {
        digitalWrite(BUZZER_PIN, HIGH);
      } else {
        digitalWrite(BUZZER_PIN, LOW);
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else if (isBuzzerTesting) {
    // Default test double-beep: 100ms ON, 100ms OFF, 100ms ON, 700ms OFF
    const uint32_t phase = millis() % 1000;
    if ((phase < 100) || (phase >= 200 && phase < 300)) {
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(BUZZER_PIN, LOW);
    }
  } else if (isSingleBeep) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void triggerAlarmTest(uint8_t alarmIndex) {
  if (alarmIndex >= MAX_ALARMS) return;
  activeAlarmIndex = alarmIndex;
  alarmStartMs = millis();
  const uint16_t interval = settings.alarms[alarmIndex].textIntervalSec > 0 ? settings.alarms[alarmIndex].textIntervalSec : 10;
  lastTextScrollMs = millis() - (interval * 1000);
  Serial.printf("[Alarm Test] Triggered Alarm %d (%02d:%02d) - Text: '%s', Duration: %ds, Beeps: %d, BeepPause: %dms, Enabled: %s\n",
                alarmIndex + 1, settings.alarms[alarmIndex].hour, settings.alarms[alarmIndex].minute,
                settings.alarms[alarmIndex].text, settings.alarms[alarmIndex].durationSec,
                settings.alarms[alarmIndex].beepCount, settings.alarms[alarmIndex].beepIntervalMs,
                settings.alarms[alarmIndex].enabled ? "YES" : "NO");
}

void checkAndRunAlarms(bool clockIsValid, const DateTime &now) {
  if (!clockIsValid) {
    updateBuzzer();
    return;
  }

  const uint8_t curHour = now.hour();
  const uint8_t curMin = now.minute();

  // 1. Hourly Chime: exactly 1 beep on every full hour (XX:00)
  if (curMin == 0) {
    if (lastChimedHour != curHour) {
      lastChimedHour = curHour;
      if (activeAlarmIndex == -1) {
        playSingleBeep(200);
        Serial.printf("[Chime] *** Hourly Chime at %02d:00 (1 beep) ***\n", curHour);
      }
    }
  } else {
    lastChimedHour = -1;
  }

  // 2. Check if any scheduled alarm should trigger
  if (activeAlarmIndex == -1) {
    for (int i = 0; i < MAX_ALARMS; i++) {
      const AlarmSetting &alarm = settings.alarms[i];
      if (alarm.enabled && curHour == alarm.hour && curMin == alarm.minute) {
        if (lastTriggeredMinute[i] != curMin) {
          activeAlarmIndex = i;
          alarmStartMs = millis();
          const uint16_t interval = alarm.textIntervalSec > 0 ? alarm.textIntervalSec : 10;
          lastTextScrollMs = millis() - (interval * 1000);
          lastTriggeredMinute[i] = curMin;
          Serial.printf("[Alarm] Triggered Alarm %d (%02d:%02d) - Text: '%s', Duration: %ds, Beeps: %d, BeepPause: %dms\n",
                        i + 1, alarm.hour, alarm.minute, alarm.text, alarm.durationSec, alarm.beepCount, alarm.beepIntervalMs);
          break;
        }
      } else {
        if (lastTriggeredMinute[i] != curMin) {
          lastTriggeredMinute[i] = -1;
        }
      }
    }
  }

  // 2. Process currently active alarm
  if (activeAlarmIndex >= 0 && activeAlarmIndex < MAX_ALARMS) {
    const AlarmSetting &alarm = settings.alarms[activeAlarmIndex];
    const uint32_t elapsedMs = millis() - alarmStartMs;

    // Check if individual alarm duration has completed
    const uint32_t totalDurationMs = (alarm.durationSec > 0) ? ((uint32_t)alarm.durationSec * 1000) : 60000;
    if (elapsedMs >= totalDurationMs) {
      Serial.printf("[Alarm] Alarm %d duration finished (%u ms).\n", activeAlarmIndex + 1, totalDurationMs);
      activeAlarmIndex = -1;
      updateBuzzer();
      return;
    }

    // Check if alarm has valid, non-empty banner text to scroll
    bool hasText = false;
    for (size_t c = 0; c < sizeof(alarm.text) && alarm.text[c] != '\0'; c++) {
      if (alarm.text[c] > ' ') {
        hasText = true;
        break;
      }
    }

    if (hasText) {
      const uint16_t interval = alarm.textIntervalSec > 0 ? alarm.textIntervalSec : 10;
      // Check text scroll interval
      if ((millis() - lastTextScrollMs) >= ((uint32_t)interval * 1000)) {
        lastTextScrollMs = millis();
        Serial.printf("[Alarm] Scrolling alarm banner: '%s'\n", alarm.text);
        // Match the configured clock color
        const CRGB alarmTextColor(settings.red, settings.green, settings.blue);
        scrollText(alarm.text, DEFAULT_SCROLL_SPEED_MS, alarmTextColor, 2);
      }
    }
  }

  updateBuzzer();
}
