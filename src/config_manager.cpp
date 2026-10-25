#include "config_manager.h"
#include <Preferences.h>

DeviceSettings settings;
static Preferences preferences;

static void initAlarmField(AlarmSetting &alarm, bool en, uint8_t h, uint8_t m, uint16_t dur, uint16_t inter, const char* txt, uint8_t beeps = DEFAULT_BEEP_COUNT, uint16_t beepInterval = DEFAULT_BEEP_INTERVAL_MS) {
  alarm.enabled = en;
  alarm.hour = h;
  alarm.minute = m;
  alarm.durationSec = dur;
  alarm.textIntervalSec = inter;
  strlcpy(alarm.text, txt, sizeof(alarm.text));
  alarm.beepCount = beeps;
  alarm.beepIntervalMs = beepInterval;
}

void setDefaultSettings() {
  memset(&settings, 0, sizeof(settings));
  settings.magic = SETTINGS_MAGIC;
  settings.brightness = DEFAULT_BRIGHTNESS;
  settings.red = DEFAULT_RED;
  settings.green = DEFAULT_GREEN;
  settings.blue = DEFAULT_BLUE;
  settings.blinkColon = DEFAULT_BLINK_COLON;
  strlcpy(settings.apSsid, DEFAULT_AP_SSID, sizeof(settings.apSsid));
  strlcpy(settings.apPassword, DEFAULT_AP_PASSWORD, sizeof(settings.apPassword));
  strlcpy(settings.staSsid, DEFAULT_STA_SSID, sizeof(settings.staSsid));
  strlcpy(settings.staPassword, DEFAULT_STA_PASSWORD, sizeof(settings.staPassword));

  // Initialize 4 default alarms (customizable lengths, beeps, intervals)
  initAlarmField(settings.alarms[0], true, 9, 29, 60, 10, "Pause", 2, 1000);
  initAlarmField(settings.alarms[1], true, 12, 29, 60, 10, "Pause", 2, 1000);
  initAlarmField(settings.alarms[2], true, 14, 59, 30, 5, "Coffie", 3, 1500);
  initAlarmField(settings.alarms[3], true, 15, 55, 300, 10, "Fininsh", 2, 1000);
}

void loadSettings() {
  preferences.begin("matrixclock", true);
  const size_t savedLength = preferences.getBytesLength("settings");
  if (savedLength == sizeof(DeviceSettings)) {
    preferences.getBytes("settings", &settings, sizeof(DeviceSettings));
  }
  preferences.end();

  if (settings.magic != SETTINGS_MAGIC || settings.apPassword[0] == '\0') {
    setDefaultSettings();
  }

  // Sanitize alarms to ensure valid beep counts and intervals
  for (int i = 0; i < 4; i++) {
    if (settings.alarms[i].beepCount == 0 || settings.alarms[i].beepCount > 10) {
      settings.alarms[i].beepCount = DEFAULT_BEEP_COUNT;
    }
    if (settings.alarms[i].beepIntervalMs < 100 || settings.alarms[i].beepIntervalMs > 10000) {
      settings.alarms[i].beepIntervalMs = DEFAULT_BEEP_INTERVAL_MS;
    }
    if (settings.alarms[i].durationSec == 0 || settings.alarms[i].durationSec > 3600) {
      settings.alarms[i].durationSec = 60;
    }
  }

  // Ensure default station credentials if none set or if using legacy default
  if (settings.staSsid[0] == '\0' || strcmp(settings.staSsid, "swiatkiewicz") == 0) {
    strlcpy(settings.staSsid, DEFAULT_STA_SSID, sizeof(settings.staSsid));
    strlcpy(settings.staPassword, DEFAULT_STA_PASSWORD, sizeof(settings.staPassword));
  }
}

void saveSettings() {
  settings.magic = SETTINGS_MAGIC;
  preferences.begin("matrixclock", false);
  preferences.putBytes("settings", &settings, sizeof(DeviceSettings));
  preferences.end();
}
