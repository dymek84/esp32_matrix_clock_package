#include "display.h"
#include "font.h"
#include "config_manager.h"
#include "alarm_manager.h"
#include "weather_icons.h"

CRGB matrix1[MATRIX1_LEDS];
CRGB matrix2[MATRIX2_LEDS];
CRGB matrix3[MATRIX3_LEDS];
volatile bool isTextScrolling = false;

// 5 x 7 digit font for clock
const uint8_t DIGITS[10][7] = {
    {0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110}, // 0
    {0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110}, // 1
    {0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111}, // 2
    {0b11110, 0b00001, 0b00001, 0b01110, 0b00001, 0b00001, 0b11110}, // 3
    {0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010}, // 4
    {0b11111, 0b10000, 0b10000, 0b11110, 0b00001, 0b00001, 0b11110}, // 5
    {0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110}, // 6
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000}, // 7
    {0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110}, // 8
    {0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b11100}  // 9
};

void initDisplay() {
  FastLED.addLeds<CHIPSET, LED_PIN_1, COLOR_ORDER>(matrix1, MATRIX1_LEDS);
  FastLED.addLeds<CHIPSET, LED_PIN_2, COLOR_ORDER>(matrix2, MATRIX2_LEDS);
  FastLED.addLeds<CHIPSET, LED_PIN_3, COLOR_ORDER>(matrix3, MATRIX3_LEDS);
  FastLED.setBrightness(settings.brightness);
  clearMatrix();
  FastLED.show();
}

uint16_t XY(uint8_t x, uint8_t y) {
  if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
    return TOTAL_LEDS;  // Sentinel
  }

  if (x < 16) {
    // Matrix 2: screen x 0-15, y 0-15; global index 256-511.
    const uint8_t pair = x >> 1;
    const uint16_t base = MATRIX2_START + (7 - pair) * 32 + 16;
    return (x & 1) ? base - 1 - y : base + y;
  }

  if (y < 8) {
    // Matrix 3: screen x 16-47, y 0-7; global index 512-767.
    const uint8_t localX = x - 16;
    const uint8_t pair = localX >> 1;
    const uint16_t base = MATRIX3_START + pair * 16;
    return (localX & 1) ? base + 15 - y : base + y;
  }

  // Matrix 1: screen x 16-47, y 8-15; global index 0-255.
  const uint8_t localX = x - 16;
  const uint8_t localY = y - 8;
  const uint8_t pair = localX >> 1;
  const uint16_t base = (15 - pair) * 16;
  return (localX & 1) ? base + 7 - localY : base + 8 + localY;
}

void setPhysicalPixel(uint16_t index, const CRGB &color) {
  if (index < MATRIX2_START) {
    matrix1[index] = color;
  } else if (index < MATRIX3_START) {
    matrix2[index - MATRIX2_START] = color;
  } else if (index < TOTAL_LEDS) {
    matrix3[index - MATRIX3_START] = color;
  }
}

void setPixel(int16_t x, int16_t y, const CRGB &color) {
  if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) {
    return;
  }
  const uint16_t index = XY((uint8_t)x, (uint8_t)y);
  if (index < TOTAL_LEDS) {
    setPhysicalPixel(index, color);
  }
}

void clearMatrix() {
  fill_solid(matrix1, MATRIX1_LEDS, CRGB::Black);
  fill_solid(matrix2, MATRIX2_LEDS, CRGB::Black);
  fill_solid(matrix3, MATRIX3_LEDS, CRGB::Black);
}

void drawDigitWithOffset(uint8_t digit, uint8_t startX, int16_t startY, const CRGB &color) {
  if (digit > 9) return;

  for (uint8_t row = 0; row < 7; row++) {
    const int16_t y = startY + (row * 2);
    for (uint8_t column = 0; column < 5; column++) {
      if (DIGITS[digit][row] & (1 << (4 - column))) {
        const int16_t x = startX + (column * 2);
        setPixel(x, y, color);
        setPixel(x + 1, y, color);
        setPixel(x, y + 1, color);
        setPixel(x + 1, y + 1, color);
      }
    }
  }
}

void drawDigit(uint8_t digit, uint8_t startX, const CRGB &color) {
  drawDigitWithOffset(digit, startX, 1, color);
}

void drawColon(bool visible, const CRGB &color) {
  if (!visible) return;

  for (uint8_t y = 5; y <= 6; y++) {
    setPixel(23, y, color);
    setPixel(24, y, color);
  }
  for (uint8_t y = 10; y <= 11; y++) {
    setPixel(23, y, color);
    setPixel(24, y, color);
  }
}

static uint8_t prevDigits[4] = {255, 255, 255, 255};
static bool hasPrevDigits = false;

void renderClock(bool clockIsValid, const DateTime &displayedTime) {
  if (isTextScrolling) return;  // Do not render clock digits while text is scrolling!

  // Check if Weather Screen is currently active
  if (isWeatherScreenActive() && activeAlarmIndex < 0) {
    renderWeatherScreen(displayedTime);
    hasPrevDigits = false; // Force clean refresh when returning to clock
    return;
  }

  CRGB clockColor(settings.red, settings.green, settings.blue);

  // High-visibility flashing clock effect during active alarms (matches user configured color)
  if (activeAlarmIndex >= 0) {
    const bool flashVisible = (millis() / 250) % 2 == 0;
    if (!flashVisible) {
      clearMatrix();
      FastLED.setBrightness(settings.brightness);
      FastLED.show();
      return;
    }
    clockColor = CRGB(settings.red, settings.green, settings.blue);
  }

  if (!clockIsValid) {
    clearMatrix();
    for (uint8_t x = 11; x <= 18; x++) setPixel(x, 8, CRGB::Red);
    for (uint8_t x = 29; x <= 36; x++) setPixel(x, 8, CRGB::Red);
    FastLED.setBrightness(settings.brightness);
    FastLED.show();
    hasPrevDigits = false;
    return;
  }

  const uint8_t newDigits[4] = {
      (uint8_t)(displayedTime.hour() / 10),
      (uint8_t)(displayedTime.hour() % 10),
      (uint8_t)(displayedTime.minute() / 10),
      (uint8_t)(displayedTime.minute() % 10)
  };

  const uint8_t startXPositions[4] = {0, 11, 27, 38};

  if (!hasPrevDigits) {
    for (uint8_t i = 0; i < 4; i++) prevDigits[i] = newDigits[i];
    hasPrevDigits = true;
  }

  const bool changed[4] = {
      prevDigits[0] != newDigits[0],
      prevDigits[1] != newDigits[1],
      prevDigits[2] != newDigits[2],
      prevDigits[3] != newDigits[3]
  };

  // If any digit changed, perform fast smooth vertical roll animation
  if (changed[0] || changed[1] || changed[2] || changed[3]) {
    for (uint8_t step = 1; step <= 7; step++) {
      const int16_t offset = step * 2;
      clearMatrix();
      drawColon(!settings.blinkColon || (displayedTime.second() % 2 == 0), clockColor);

      for (uint8_t i = 0; i < 4; i++) {
        if (changed[i]) {
          drawDigitWithOffset(prevDigits[i], startXPositions[i], 1 - offset, clockColor);
          drawDigitWithOffset(newDigits[i], startXPositions[i], 1 + 14 - offset, clockColor);
        } else {
          drawDigitWithOffset(newDigits[i], startXPositions[i], 1, clockColor);
        }
      }

      FastLED.setBrightness(settings.brightness);
      FastLED.show();
      delay(DEFAULT_DIGIT_ANIM_SPEED_MS);
    }

    for (uint8_t i = 0; i < 4; i++) prevDigits[i] = newDigits[i];
  }

  // Regular static render frame
  clearMatrix();
  drawColon(!settings.blinkColon || (displayedTime.second() % 2 == 0), clockColor);
  for (uint8_t i = 0; i < 4; i++) {
    drawDigitWithOffset(newDigits[i], startXPositions[i], 1, clockColor);
  }

  FastLED.setBrightness(settings.brightness);
  FastLED.show();
}

void drawDegree(int16_t x, int16_t y, const CRGB &color, uint8_t scale, int16_t minX, int16_t maxX) {
  // 3x3 hollow degree circle bitmap at rows 0, 1, 2
  static const uint8_t DEGREE_BITMAP[3] = {
    0b010,
    0b101,
    0b010
  };

  for (uint8_t row = 0; row < 3; row++) {
    const uint8_t b = DEGREE_BITMAP[row];
    for (uint8_t col = 0; col < 3; col++) {
      if (b & (1 << (2 - col))) {
        if (scale == 1) {
          const int16_t px = x + col;
          const int16_t py = y + row;
          if (px >= minX && px <= maxX && py >= 0 && py < MATRIX_HEIGHT) {
            setPixel(px, py, color);
          }
        } else {
          const int16_t px = x + (col * scale);
          const int16_t py = y + (row * scale);
          for (uint8_t dx = 0; dx < scale; dx++) {
            for (uint8_t dy = 0; dy < scale; dy++) {
              const int16_t spx = px + dx;
              const int16_t spy = py + dy;
              if (spx >= minX && spx <= maxX && spy >= 0 && spy < MATRIX_HEIGHT) {
                setPixel(spx, spy, color);
              }
            }
          }
        }
      }
    }
  }
}

void drawChar(char c, int16_t x, int16_t y, const CRGB &color, uint8_t scale) {
  const uint8_t uc = (uint8_t)c;
  if (uc == 0xB0 || uc == 176 || uc == '^') {
    drawDegree(x, y, color, scale, 0, 47);
    return;
  }
  if (c < 32 || c > 126) c = '?';
  const uint8_t idx = c - 32;

  for (uint8_t row = 0; row < 7; row++) {
    const uint8_t b = pgm_read_byte(&(FONT5x7[idx][row]));
    for (uint8_t col = 0; col < 5; col++) {
      if (b & (1 << (4 - col))) {
        if (scale == 1) {
          setPixel(x + col, y + row, color);
        } else {
          const int16_t px = x + (col * scale);
          const int16_t py = y + (row * scale);
          for (uint8_t dx = 0; dx < scale; dx++) {
            for (uint8_t dy = 0; dy < scale; dy++) {
              setPixel(px + dx, py + dy, color);
            }
          }
        }
      }
    }
  }
}

void drawText(const String &text, int16_t x, int16_t y, const CRGB &color, uint8_t scale) {
  const int16_t charWidth = 5 * scale;
  const int16_t spacing = 1 * scale;
  const int16_t degWidth = 3 * scale;
  int16_t cursorX = x;

  for (size_t i = 0; i < text.length(); i++) {
    const uint8_t c = (uint8_t)text[i];
    // Check for UTF-8 degree sign sequence: 0xC2 0xB0 ("°")
    if (c == 0xC2 && i + 1 < text.length() && (uint8_t)text[i + 1] == 0xB0) {
      drawDegree(cursorX, y, color, scale, 0, 47);
      cursorX += degWidth + spacing;
      i++; // Skip the second byte of UTF-8 character
    } else if (c == 0xB0 || c == 0xDF || c == 176 || c == '^') {
      drawDegree(cursorX, y, color, scale, 0, 47);
      cursorX += degWidth + spacing;
    } else {
      drawChar((char)c, cursorX, y, color, scale);
      cursorX += charWidth + spacing;
    }
  }
}

int16_t getTextWidth(const String &text, uint8_t scale) {
  if (text.length() == 0) return 0;
  const int16_t charWidth = 5 * scale;
  const int16_t spacing = 1 * scale;
  const int16_t degWidth = 3 * scale;
  int16_t totalW = 0;

  for (size_t i = 0; i < text.length(); i++) {
    const uint8_t c = (uint8_t)text[i];
    if (c == 0xC2 && i + 1 < text.length() && (uint8_t)text[i + 1] == 0xB0) {
      totalW += degWidth + spacing;
      i++;
    } else if (c == 0xB0 || c == 0xDF || c == 176 || c == '^') {
      totalW += degWidth + spacing;
    } else {
      totalW += charWidth + spacing;
    }
  }
  return totalW;
}

void scrollText(const String &text, uint16_t speedMs, const CRGB &color, uint8_t scale) {
  isTextScrolling = true;
  const int16_t textWidth = getTextWidth(text, scale);
  const int16_t y = (scale == 2) ? 1 : 4;

  for (int16_t x = MATRIX_WIDTH; x >= -textWidth; x--) {
    clearMatrix();
    drawText(text, x, y, color, scale);
    FastLED.setBrightness(settings.brightness);
    FastLED.show();
    updateBuzzer();
    delay(speedMs);
  }

  clearMatrix();
  FastLED.show();
  hasPrevDigits = false; // Force clean redraw of time when scrolling completes
  isTextScrolling = false;
}

static String pendingScrollText = "";
static volatile bool pendingScrollAvailable = false;

void queueScrollText(const String &text) {
  pendingScrollText = text;
  pendingScrollAvailable = true;
}

void processPendingScroll() {
  if (pendingScrollAvailable) {
    pendingScrollAvailable = false;
    const CRGB textColor(settings.red, settings.green, settings.blue);
    scrollText(pendingScrollText, DEFAULT_SCROLL_SPEED_MS, textColor, 2);
  }
}

void drawIcon16x16(int16_t startX, int16_t startY, const uint32_t* iconData) {
  if (!iconData) return;
  for (int y = 0; y < 16; y++) {
    for (int x = 0; x < 16; x++) {
      const int16_t screenX = startX + x;
      const int16_t screenY = startY + y;

      if (screenX >= 0 && screenX < MATRIX_WIDTH && screenY >= 0 && screenY < MATRIX_HEIGHT) {
        const uint32_t color = pgm_read_dword(&(iconData[y * 16 + x]));
        if (color != 0x000000) {
          const uint8_t r = (color >> 16) & 0xFF;
          const uint8_t g = (color >> 8) & 0xFF;
          const uint8_t b = color & 0xFF;
          setPixel(screenX, screenY, CRGB(r, g, b));
        }
      }
    }
  }
}

const uint32_t* getWeatherIcon(const char* desc, bool isDay) {
  if (!desc || desc[0] == '\0') return isDay ? ICON_SUN : ICON_MOON;

  String s = String(desc);
  s.toLowerCase();

  if (s.indexOf("thunder") >= 0 || s.indexOf("storm") >= 0) {
    return isDay ? ICON_THUNDER_DAY : ICON_THUNDER_NIGHT;
  }
  if (s.indexOf("snow") >= 0 || s.indexOf("blizzard") >= 0 || s.indexOf("ice") >= 0) {
    return isDay ? ICON_SNOW : ICON_SNOW_NIGHT;
  }
  if (s.indexOf("drizzle") >= 0 || s.indexOf("shower") >= 0) {
    return isDay ? ICON_RAIN_SUN : ICON_RAIN_MOON;
  }
  if (s.indexOf("rain") >= 0 || s.indexOf("sleet") >= 0) {
    return isDay ? ICON_RAIN : ICON_RAIN_NIGHT;
  }
  if (s.indexOf("fog") >= 0 || s.indexOf("mist") >= 0 || s.indexOf("haze") >= 0) {
    return isDay ? ICON_FOG : ICON_FOG_NIGHT;
  }
  if (s.indexOf("partly") >= 0 || s.indexOf("few") >= 0 || s.indexOf("scattered") >= 0) {
    return isDay ? ICON_PARTLY_CLOUDY_DAY : ICON_PARTLY_CLOUDY_NIGHT;
  }
  if (s.indexOf("cloud") >= 0 || s.indexOf("overcast") >= 0) {
    return isDay ? ICON_CLOUDY : ICON_CLOUDY_NIGHT;
  }
  if (s.indexOf("clear") >= 0 || s.indexOf("sun") >= 0) {
    return isDay ? ICON_SUN : ICON_MOON;
  }

  return isDay ? ICON_SUN : ICON_MOON;
}

static uint32_t weatherScreenUntilMs = 0;

void showWeatherScreen(uint32_t durationMs) {
  if (!currentWeather.valid) return;
  if (millis() - currentWeather.lastUpdateEpoch >= WEATHER_EXPIRATION_MS) {
    currentWeather.valid = false;
    return;
  }
  weatherScreenUntilMs = millis() + durationMs;
}

bool isWeatherScreenActive() {
  if (!currentWeather.valid) return false;
  if (millis() - currentWeather.lastUpdateEpoch >= WEATHER_EXPIRATION_MS) {
    currentWeather.valid = false;
    return false;
  }
  return (millis() < weatherScreenUntilMs);
}

static void drawCharClipped(char c, int16_t x, int16_t y, const CRGB &color, int16_t minX, int16_t maxX) {
  const uint8_t uc = (uint8_t)c;
  if (uc == 0xB0 || uc == 176 || uc == '^') {
    drawDegree(x, y, color, 1, minX, maxX);
    return;
  }
  if (c < 32 || c > 126) c = '?';
  const uint8_t idx = c - 32;

  for (uint8_t row = 0; row < 7; row++) {
    const uint8_t b = pgm_read_byte(&(FONT5x7[idx][row]));
    for (uint8_t col = 0; col < 5; col++) {
      if (b & (1 << (4 - col))) {
        const int16_t px = x + col;
        const int16_t py = y + row;
        if (px >= minX && px <= maxX) {
          setPixel(px, py, color);
        }
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Dynamic Dawn / Dusk (Sunrise / Sunset) Calculator
// -----------------------------------------------------------------------------
static uint16_t getDayOfYear(const DateTime &dt) {
  static const uint16_t daysBeforeMonth[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  uint8_t m = dt.month();
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  uint16_t doy = daysBeforeMonth[m - 1] + dt.day();
  if (m > 2 && (dt.year() % 4 == 0 && (dt.year() % 100 != 0 || dt.year() % 400 == 0))) {
    doy++;
  }
  return doy;
}

static bool isEU_DST(const DateTime &dt) {
  const int m = dt.month();
  if (m < 3 || m > 10) return false;
  if (m > 3 && m < 10) return true;
  const int d = dt.day();
  const int dow = dt.dayOfTheWeek(); // 0 = Sunday
  if (m == 3) return (d - dow >= 25);
  if (m == 10) return (d - dow < 25);
  return false;
}

void calculateSolarDawnDusk(const DateTime &dt, uint8_t &riseH, uint8_t &riseM, uint8_t &setH, uint8_t &setM) {
  const uint16_t doy = getDayOfYear(dt);
  const bool dst = isEU_DST(dt);

  // Solar declination in radians
  const float rad = 3.14159265f / 180.0f;
  const float delta = -23.44f * cosf((2.0f * 3.14159265f * (doy + 10)) / 365.25f) * rad;
  const float phi = 52.0f * rad; // ~52°N Central Europe

  // Hour angle for sunrise / sunset taking atmospheric refraction (-0.833°) into account
  float cosH0 = (sinf(-0.833f * rad) - sinf(phi) * sinf(delta)) / (cosf(phi) * cosf(delta));
  if (cosH0 > 1.0f) cosH0 = 1.0f;
  if (cosH0 < -1.0f) cosH0 = -1.0f;
  const float H0_deg = acosf(cosH0) * (180.0f / 3.14159265f);

  // Half-day duration in minutes
  const float halfDayMin = (H0_deg / 15.0f) * 60.0f;

  // Local solar noon in minutes from midnight (CET = ~705 min [11:45], CEST = ~765 min [12:45])
  const float noonMin = dst ? 765.0f : 705.0f;

  const int sunriseTotalMin = (int)(noonMin - halfDayMin + 0.5f);
  const int sunsetTotalMin  = (int)(noonMin + halfDayMin + 0.5f);

  riseH = constrain(sunriseTotalMin / 60, 0, 23);
  riseM = constrain(sunriseTotalMin % 60, 0, 59);
  setH  = constrain(sunsetTotalMin / 60, 0, 23);
  setM  = constrain(sunsetTotalMin % 60, 0, 59);
}

bool isDayTime(const DateTime &displayedTime) {
  uint8_t riseH = 0, riseM = 0, setH = 0, setM = 0;

  if (currentWeather.valid && (millis() - currentWeather.lastUpdateEpoch >= WEATHER_EXPIRATION_MS)) {
    currentWeather.valid = false;
  }

  if (currentWeather.valid && currentWeather.hasDawnDusk) {
    riseH = currentWeather.sunriseHour;
    riseM = currentWeather.sunriseMinute;
    setH  = currentWeather.sunsetHour;
    setM  = currentWeather.sunsetMinute;
  } else {
    // Dynamic astronomical calculation for the current date
    calculateSolarDawnDusk(displayedTime, riseH, riseM, setH, setM);
  }

  const int currentMin = displayedTime.hour() * 60 + displayedTime.minute();
  const int riseMin = riseH * 60 + riseM;
  const int setMin  = setH * 60 + setM;

  return (currentMin >= riseMin && currentMin < setMin);
}

void renderWeatherScreen(const DateTime &displayedTime) {
  clearMatrix();

  const bool isDay = isDayTime(displayedTime);
  const uint32_t* icon = getWeatherIcon(currentWeather.desc, isDay);

  // 1. Draw 16x16 icon on physical left panel (screen coordinates x: 0..15, y: 0..15)
  drawIcon16x16(0, 0, icon);

  // 2. Right side (screen coordinates x: 16..47, y: 0..15)
  // Top row (y: 1): Temperature (e.g. " 21°C" or "-5°C")
  char tempStr[16];
  snprintf(tempStr, sizeof(tempStr), "%d°C", currentWeather.temp);
  const int16_t tempWidth = getTextWidth(tempStr, 1);
  const int16_t tempX = 16 + (32 - tempWidth) / 2;
  const CRGB clockColor(settings.red, settings.green, settings.blue);
  drawText(tempStr, tempX, 1, clockColor, 1);

  // Bottom row (y: 9): Description text
  const String descStr = String(currentWeather.desc);
  const int16_t descWidth = getTextWidth(descStr, 1);

  if (descWidth <= 32) {
    const int16_t descX = 16 + (32 - descWidth) / 2;
    drawText(descStr, descX, 9, CRGB::White, 1);
  } else {
    // Smooth marquee scroll in the right 32-pixel window (x: 16..47)
    const uint32_t scrollTime = millis() / 65;
    const int16_t totalRange = descWidth + 14;
    const int16_t offset = scrollTime % totalRange;
    const int16_t descX = 47 - offset;

    for (size_t i = 0; i < descStr.length(); i++) {
      const int16_t charX = descX + (i * 6);
      if (charX + 5 >= 16 && charX < 48) {
        drawCharClipped(descStr[i], charX, 9, CRGB::White, 16, 47);
      }
    }
  }

  FastLED.setBrightness(settings.brightness);
  FastLED.show();
}

WeatherData currentWeather = { false, 0, "", 0, false, 7, 0, 18, 0 };
static uint32_t lastWeatherScrollMs = 0;

void updateWeather(int16_t temp, const char* desc, uint8_t sunriseH, uint8_t sunriseM, uint8_t sunsetH, uint8_t sunsetM) {
  currentWeather.valid = true;
  currentWeather.temp = temp;
  strlcpy(currentWeather.desc, desc ? desc : "", sizeof(currentWeather.desc));
  currentWeather.lastUpdateEpoch = millis();

  if (sunriseH <= 23 && sunsetH <= 23) {
    currentWeather.hasDawnDusk = true;
    currentWeather.sunriseHour = sunriseH;
    currentWeather.sunriseMinute = sunriseM;
    currentWeather.sunsetHour = sunsetH;
    currentWeather.sunsetMinute = sunsetM;
    Serial.printf("[Weather] Dynamic Dawn/Dusk set: Sunrise %02d:%02d, Sunset %02d:%02d\n",
                  sunriseH, sunriseM, sunsetH, sunsetM);
  }

  // Activate the weather screen with icon for 8 seconds
  showWeatherScreen(8000);
  lastWeatherScrollMs = millis();
  Serial.printf("[Weather] Received: %dC '%s' -> Displaying weather screen with icon\n", temp, currentWeather.desc);
}

void checkPeriodicWeatherDisplay() {
  if (!currentWeather.valid) return;
  if (millis() - currentWeather.lastUpdateEpoch >= WEATHER_EXPIRATION_MS) {
    currentWeather.valid = false;
    Serial.println("[Weather] 2 hours elapsed without update -> Weather display disabled");
    return;
  }
  if (activeAlarmIndex >= 0 || isTextScrolling) return;

  // Periodically show weather screen (configurable 1..30 min, default 5 min)
  const uint32_t intervalMin = (settings.weatherIntervalMin >= 1 && settings.weatherIntervalMin <= 30)
                                ? settings.weatherIntervalMin : DEFAULT_WEATHER_INTERVAL_MIN;
  const uint32_t intervalMs = intervalMin * 60000UL;

  if (millis() - lastWeatherScrollMs >= intervalMs) {
    lastWeatherScrollMs = millis();
    showWeatherScreen(7000);
  }
}

// -----------------------------------------------------------------------------
// 5-Second Matrix Display Possibility Animations (3 Variants)
// -----------------------------------------------------------------------------

// Variant 0: "Plasma Ocean / Aurora Swirl" - Smooth multi-frequency 2D wave synthesis
static void runAnimationPlasma(uint32_t durationMs) {
  const uint32_t startMs = millis();
  uint16_t timeStep = 0;

  while (millis() - startMs < durationMs) {
    const uint32_t elapsed = millis() - startMs;
    timeStep += 3;

    // Smooth fade envelope (in during first 350ms, out during last 400ms)
    uint8_t globalDim = 255;
    if (elapsed < 350) {
      globalDim = (elapsed * 255) / 350;
    } else if (durationMs - elapsed < 400) {
      globalDim = ((durationMs - elapsed) * 255) / 400;
    }

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      for (int x = 0; x < MATRIX_WIDTH; x++) {
        const uint8_t w1 = sin8((x * 7) + (timeStep * 2));
        const uint8_t w2 = sin8((y * 18) - (timeStep * 3));
        const uint8_t w3 = sin8(((x + y) * 6) + timeStep);

        const int8_t cx = (int8_t)x - 24 + ((int8_t)sin8(timeStep) - 128) / 16;
        const int8_t cy = (int8_t)y - 8 + ((int8_t)cos8(timeStep) - 128) / 20;
        const uint8_t dist = sqrt16((cx * cx * 4) + (cy * cy * 16));
        const uint8_t w4 = sin8((dist * 9) - (timeStep * 2));

        const uint8_t hue = (w1 + w2 + w3 + w4) / 2 + (timeStep / 3);
        const uint8_t sat = 230 + sin8(x * 5 + timeStep) / 10;
        const uint8_t val = scale8(255, globalDim);

        setPixel(x, y, CHSV(hue, sat, val));
      }
    }

    FastLED.setBrightness(settings.brightness);
    FastLED.show();
    delay(25);
  }

  clearMatrix();
  FastLED.show();
}

// Variant 1: "Digital Cyber Rain / Matrix Cascade" - Cyberpunk falling streams with fading phosphor trails
static void runAnimationMatrixRain(uint32_t durationMs) {
  const uint32_t startMs = millis();

  struct RainDrop {
    float y;
    float speed;
    uint8_t length;
    uint8_t hue;
  };

  RainDrop drops[MATRIX_WIDTH];
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    drops[x].y = -(float)(esp_random() % 24);
    drops[x].speed = 0.35f + ((esp_random() % 100) / 100.0f) * 0.45f;
    drops[x].length = 5 + (esp_random() % 8);
    drops[x].hue = (esp_random() % 2 == 0) ? 96 : 135;
  }

  clearMatrix();

  while (millis() - startMs < durationMs) {
    const uint32_t elapsed = millis() - startMs;

    uint8_t globalDim = 255;
    if (elapsed < 300) {
      globalDim = (elapsed * 255) / 300;
    } else if (durationMs - elapsed < 400) {
      globalDim = ((durationMs - elapsed) * 255) / 400;
    }

    // Decay previous frame by 50 for persistent phosphor glow
    fadeToBlackBy(matrix1, MATRIX1_LEDS, 50);
    fadeToBlackBy(matrix2, MATRIX2_LEDS, 50);
    fadeToBlackBy(matrix3, MATRIX3_LEDS, 50);

    for (int x = 0; x < MATRIX_WIDTH; x++) {
      drops[x].y += drops[x].speed;
      const int headY = (int)drops[x].y;

      for (int i = 0; i < drops[x].length; i++) {
        const int trailY = headY - i;
        if (trailY >= 0 && trailY < MATRIX_HEIGHT) {
          if (i == 0) {
            CRGB headCol = CRGB(220, 255, 240);
            headCol.nscale8(globalDim);
            setPixel(x, trailY, headCol);
          } else {
            const uint8_t trailVal = scale8(255 - ((i * 240) / drops[x].length), globalDim);
            setPixel(x, trailY, CHSV(drops[x].hue, 250, trailVal));
          }
        }
      }

      if (drops[x].y - drops[x].length >= MATRIX_HEIGHT) {
        drops[x].y = -(float)(esp_random() % 12);
        drops[x].speed = 0.35f + ((esp_random() % 100) / 100.0f) * 0.45f;
        drops[x].length = 5 + (esp_random() % 8);
        const uint8_t r = esp_random() % 3;
        drops[x].hue = (r == 0) ? 96 : (r == 1 ? 135 : 190);
      }
    }

    // Subtle glint pixel
    if ((esp_random() % 4) == 0) {
      const int rx = esp_random() % MATRIX_WIDTH;
      const int ry = esp_random() % MATRIX_HEIGHT;
      CRGB glint = CRGB(255, 255, 255);
      glint.nscale8(globalDim);
      setPixel(rx, ry, glint);
    }

    FastLED.setBrightness(settings.brightness);
    FastLED.show();
    delay(28);
  }

  clearMatrix();
  FastLED.show();
}

// Variant 2: "Hypnotic Supernova / Cosmic Shockwave" - Expanding radial rainbow waves with star glitter
static void runAnimationSupernova(uint32_t durationMs) {
  const uint32_t startMs = millis();
  uint16_t timeStep = 0;

  clearMatrix();

  while (millis() - startMs < durationMs) {
    const uint32_t elapsed = millis() - startMs;
    timeStep += 4;

    uint8_t globalDim = 255;
    if (elapsed < 350) {
      globalDim = (elapsed * 255) / 350;
    } else if (durationMs - elapsed < 400) {
      globalDim = ((durationMs - elapsed) * 255) / 400;
    }

    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      for (int x = 0; x < MATRIX_WIDTH; x++) {
        const int16_t dx = (x - 24);
        const int16_t dy = (y - 8) * 2;
        const uint16_t dist = sqrt16(dx * dx + dy * dy);

        const uint8_t wave = sin8((dist * 14) - (timeStep * 6));
        const uint8_t hue = (dist * 7) + (timeStep * 2);
        const uint8_t val = scale8(wave, globalDim);

        setPixel(x, y, CHSV(hue, 240, val));
      }
    }

    // Overlay brilliant twinkling star sparkles
    for (int i = 0; i < 4; i++) {
      const int sx = esp_random() % MATRIX_WIDTH;
      const int sy = esp_random() % MATRIX_HEIGHT;
      CRGB sparkle = (esp_random() % 2 == 0) ? CRGB(255, 255, 255) : CRGB(255, 230, 180);
      sparkle.nscale8(globalDim);
      setPixel(sx, sy, sparkle);
    }

    FastLED.setBrightness(settings.brightness);
    FastLED.show();
    delay(25);
  }

  clearMatrix();
  FastLED.show();
}

void runAnimationVariant(uint8_t variant, uint32_t durationMs) {
  if (variant > 2) {
    variant = esp_random() % 3;
  }

  Serial.printf("[Animation] Playing variant %d (%u ms)\n", variant, durationMs);

  switch (variant) {
    case 0:
      runAnimationPlasma(durationMs);
      break;
    case 1:
      runAnimationMatrixRain(durationMs);
      break;
    case 2:
    default:
      runAnimationSupernova(durationMs);
      break;
  }
}

void playStartupAnimation(uint8_t variant) {
  runAnimationVariant(variant, 5000);
}

