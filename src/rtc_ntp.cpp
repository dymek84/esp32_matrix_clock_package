#include "rtc_ntp.h"
#include <sys/time.h>

RTC_DS3231 ds3231;
bool ds3231Present = false;
bool ds3231TimeValid = false;
bool pcf8563Present = false;
bool clockIsValid = false;
DateTime displayedTime;
TimeSource currentTimeSource = TIME_SOURCE_NONE;
static uint32_t lastClockReadMs = 0;
static int activeSdaPin = I2C_SDA;
static int activeSclPin = I2C_SCL;

uint8_t bcdToDec(uint8_t value) {
  return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t decToBcd(uint8_t value) {
  return ((value / 10) << 4) | (value % 10);
}

bool deviceAt(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool readPCF8563(DateTime &value) {
  if (!pcf8563Present) return false;

  Wire.beginTransmission(PCF8563_ADDRESS);
  Wire.write(0x02);  // Seconds register.
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)PCF8563_ADDRESS, 7) != 7) {
    return false;
  }

  const uint8_t secondsRaw = Wire.read();
  const uint8_t minutesRaw = Wire.read();
  const uint8_t hoursRaw = Wire.read();
  const uint8_t daysRaw = Wire.read();
  Wire.read();  // Weekday is not needed by DateTime.
  const uint8_t monthsRaw = Wire.read();
  const uint8_t yearsRaw = Wire.read();

  if (secondsRaw & 0x80) return false;

  const uint16_t year = 2000 + bcdToDec(yearsRaw);
  const uint8_t month = bcdToDec(monthsRaw & 0x1F);
  const uint8_t day = bcdToDec(daysRaw & 0x3F);
  const uint8_t hour = bcdToDec(hoursRaw & 0x3F);
  const uint8_t minute = bcdToDec(minutesRaw & 0x7F);
  const uint8_t second = bcdToDec(secondsRaw & 0x7F);

  if (year < 2024 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59) {
    return false;
  }

  value = DateTime(year, month, day, hour, minute, second);
  return true;
}

bool writePCF8563(const DateTime &value) {
  if (!pcf8563Present) return false;

  Wire.beginTransmission(PCF8563_ADDRESS);
  Wire.write(0x02);  // Seconds register.
  Wire.write(decToBcd(value.second()));
  Wire.write(decToBcd(value.minute()));
  Wire.write(decToBcd(value.hour()));
  Wire.write(decToBcd(value.day()));
  Wire.write(decToBcd(value.dayOfTheWeek()));
  Wire.write(decToBcd(value.month()));
  Wire.write(decToBcd(value.year() - 2000));
  return Wire.endTransmission() == 0;
}

bool isRTC1Valid() {
  if (!ds3231Present) return false;
  if (ds3231.lostPower()) return false;
  const DateTime now = ds3231.now();
  return (now.year() >= 2024 && now.year() <= 2100);
}

bool isRTC2Valid() {
  if (!pcf8563Present) return false;
  DateTime temp;
  if (!readPCF8563(temp)) return false;
  return (temp.year() >= 2024 && temp.year() <= 2100);
}

const char* getTimeSourceString(TimeSource source) {
  switch (source) {
    case TIME_SOURCE_RTC_DS3231: return "DS3231 RTC";
    case TIME_SOURCE_RTC_PCF8563: return "PCF8563 RTC";
    case TIME_SOURCE_NTP: return "NTP Internet";
    case TIME_SOURCE_MANUAL: return "Manual Input";
    case TIME_SOURCE_ESP32_SYSTEM: return "ESP32 System Clock";
    default: return "None";
  }
}

const char* getActiveClockSource() {
  return getTimeSourceString(currentTimeSource);
}

bool readActiveClock(DateTime &value) {
  if (isRTC1Valid()) {
    value = ds3231.now();
    if (currentTimeSource == TIME_SOURCE_NONE) {
      currentTimeSource = TIME_SOURCE_RTC_DS3231;
    }
    return true;
  }
  if (isRTC2Valid()) {
    if (readPCF8563(value)) {
      if (currentTimeSource == TIME_SOURCE_NONE) {
        currentTimeSource = TIME_SOURCE_RTC_PCF8563;
      }
      return true;
    }
  }

  // Fallback to internal ESP32 system clock (updated by NTP)
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    if (timeinfo.tm_year + 1900 >= 2024 && timeinfo.tm_year + 1900 <= 2100) {
      value = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                       timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      if (currentTimeSource == TIME_SOURCE_NONE) {
        currentTimeSource = TIME_SOURCE_ESP32_SYSTEM;
      }
      return true;
    }
  }

  return false;
}

void setClock(const DateTime &value, TimeSource source) {
  Serial.printf("[RTC Debug] Setting clock (%s) to %04d-%02d-%02d %02d:%02d:%02d\n",
                getTimeSourceString(source),
                value.year(), value.month(), value.day(), value.hour(), value.minute(), value.second());

  Wire.begin(activeSdaPin, activeSclPin);

  // Update DS3231 RTC
  if (deviceAt(0x68) || ds3231Present) {
    ds3231Present = ds3231.begin(&Wire);
    Wire.begin(activeSdaPin, activeSclPin);
    ds3231.adjust(value);
    ds3231TimeValid = true;
    Serial.println("[RTC Debug] DS3231 RTC updated with new time.");
  } else {
    Serial.println("[RTC Debug] DS3231 RTC not found on I2C bus.");
  }

  // Update PCF8563 RTC
  if (deviceAt(PCF8563_ADDRESS) || pcf8563Present) {
    pcf8563Present = true;
    if (writePCF8563(value)) {
      Serial.println("[RTC Debug] PCF8563 RTC updated with new time.");
    } else {
      Serial.println("[RTC Debug] PCF8563 RTC write failed.");
    }
  } else {
    Serial.println("[RTC Debug] PCF8563 RTC not found on I2C bus.");
  }

  displayedTime = value;
  clockIsValid = true;
  currentTimeSource = source;
}

void scanI2CBus() {
  Serial.println("\n[I2C Pin Auto-Detection Probe]");

  const int candidateSda[] = {I2C_SDA, 21, 19, 18, 4, 5, 26, 27, 32, 33};
  const int candidateScl[] = {I2C_SCL, 22, 23, 5, 4, 18, 27, 26, 33, 32};
  const int count = sizeof(candidateSda) / sizeof(candidateSda[0]);

  for (int i = 0; i < count; i++) {
    const int sda = candidateSda[i];
    const int scl = candidateScl[i];

    Wire.end();
    delay(5);
    Wire.begin(sda, scl);
    Wire.setClock(100000);
    delay(15);

    const bool found68 = deviceAt(0x68);
    const bool found51 = deviceAt(0x51);

    if (found68 || found51) {
      activeSdaPin = sda;
      activeSclPin = scl;
      Serial.printf(" -> SUCCESS: RTC detected on pin pair (SDA=%d, SCL=%d)! [DS3231: %s | PCF8563: %s]\n",
                    activeSdaPin, activeSclPin,
                    found68 ? "FOUND" : "NOT FOUND",
                    found51 ? "FOUND" : "NOT FOUND");
      return;
    }
  }

  // Fallback to configured pins
  activeSdaPin = I2C_SDA;
  activeSclPin = I2C_SCL;
  Wire.end();
  delay(5);
  Wire.begin(activeSdaPin, activeSclPin);
  Wire.setClock(400000);
  Serial.printf(" -> WARNING: No RTC responded on probed pin pairs. Defaulting to SDA=%d, SCL=%d\n", activeSdaPin, activeSclPin);
}

void testRtcConnections() {
  scanI2CBus();

  Serial.println("\n==================================================");
  Serial.println("         I2C HARDWARE & RTC DIAGNOSTICS          ");
  Serial.println("==================================================");

  // Re-apply clock speed after bus scan
  Wire.setClock(400000);

  // Probe I2C addresses
  const bool found0x68 = deviceAt(0x68);  // DS3231 RTC
  const bool found0x57 = deviceAt(0x57);  // DS3231 24C32 EEPROM
  const bool found0x51 = deviceAt(0x51);  // PCF8563 RTC

  Serial.println("[I2C Bus Probe]");
  Serial.printf(" - 0x68 (DS3231 RTC)   : %s\n", found0x68 ? "FOUND (Responding)" : "NOT FOUND (No ACK)");
  Serial.printf(" - 0x57 (24C32 EEPROM) : %s\n", found0x57 ? "FOUND (Responding)" : "NOT FOUND (No ACK)");
  Serial.printf(" - 0x51 (PCF8563 RTC)  : %s\n", found0x51 ? "FOUND (Responding)" : "NOT FOUND (No ACK)");

  Serial.println("\n[DS3231 Detailed Check]");
  if (found0x68) {
    ds3231Present = ds3231.begin(&Wire);
    Wire.begin(activeSdaPin, activeSclPin);
    Wire.setClock(400000);
    if (ds3231Present) {
      const bool valid = isRTC1Valid();
      ds3231TimeValid = valid;
      const float tempC = ds3231.getTemperature();
      const DateTime dsTime = ds3231.now();

      Serial.println(" - Initialization       : SUCCESS");
      Serial.printf(" - Power Loss / Validity: %s\n", valid ? "OK (Valid)" : "WARNING (Invalid date or power lost)");
      Serial.printf(" - On-chip Temperature  : %.2f C\n", tempC);
      Serial.printf(" - Current DS3231 Time  : %04d-%02d-%02d %02d:%02d:%02d\n",
                    dsTime.year(), dsTime.month(), dsTime.day(),
                    dsTime.hour(), dsTime.minute(), dsTime.second());
    } else {
      Serial.println(" - Initialization       : FAILED (ds3231.begin returned false)");
    }
  } else {
    ds3231Present = false;
    ds3231TimeValid = false;
    Serial.println(" - Device missing on I2C bus.");
  }

  Serial.println("\n[PCF8563 Detailed Check]");
  if (found0x51) {
    pcf8563Present = true;
    DateTime pcfTime;
    if (readPCF8563(pcfTime)) {
      Serial.println(" - Register Read       : SUCCESS");
      Serial.printf(" - Current PCF8563 Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    pcfTime.year(), pcfTime.month(), pcfTime.day(),
                    pcfTime.hour(), pcfTime.minute(), pcfTime.second());
    } else {
      Serial.println(" - Register Read       : FAILED (Invalid data or oscillator stopped)");
    }
  } else {
    pcf8563Present = false;
    Serial.println(" - Device missing on I2C bus.");
  }

  // Cross-RTC sync if DS3231 lost power but PCF8563 is valid
  DateTime fallback;
  if (ds3231Present && !ds3231TimeValid && readPCF8563(fallback)) {
    Serial.println("\n[RTC Sync] Restoring DS3231 clock from PCF8563 fallback...");
    ds3231.adjust(fallback);
    ds3231TimeValid = true;
  }

  clockIsValid = readActiveClock(displayedTime);
  Serial.println("\n[Diagnostic Summary]");
  Serial.printf(" - Primary Selected Clock Source : %s\n", getActiveClockSource());
  if (clockIsValid) {
    Serial.printf(" - System Active Time            : %04d-%02d-%02d %02d:%02d:%02d\n",
                  displayedTime.year(), displayedTime.month(), displayedTime.day(),
                  displayedTime.hour(), displayedTime.minute(), displayedTime.second());
  } else {
    Serial.println(" - System Active Time            : UNSET / INVALID");
  }
  Serial.println("==================================================\n");
}

void initialiseRtc() {
  testRtcConnections();
}

void updateClock() {
  if (millis() - lastClockReadMs < 250) return;
  lastClockReadMs = millis();

  DateTime current;
  if (readActiveClock(current)) {
    displayedTime = current;
    clockIsValid = true;
  }
}

bool syncNtpTime(uint32_t timeoutMs) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[NTP Debug] Cannot sync NTP: Wi-Fi is NOT connected.");
    return false;
  }

  Serial.println("[NTP Debug] Syncing time via NTP...");
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);

  struct tm timeinfo;
  const uint32_t startMs = millis();
  while (millis() - startMs < timeoutMs) {
    if (getLocalTime(&timeinfo, 200)) {
      if (timeinfo.tm_year + 1900 >= 2024 && timeinfo.tm_year + 1900 <= 2100) {
        DateTime ntpTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        setClock(ntpTime, TIME_SOURCE_NTP);
        Serial.printf("[NTP Debug] NTP sync SUCCESS: %04d-%02d-%02d %02d:%02d:%02d\n",
                      ntpTime.year(), ntpTime.month(), ntpTime.day(),
                      ntpTime.hour(), ntpTime.minute(), ntpTime.second());
        return true;
      }
    }
    delay(100);
  }

  Serial.println("[NTP Debug] NTP time sync timed out.");
  return false;
}
