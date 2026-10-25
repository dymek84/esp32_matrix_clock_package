#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <RTClib.h>
#include "config.h"

extern CRGB matrix1[MATRIX1_LEDS];
extern CRGB matrix2[MATRIX2_LEDS];
extern CRGB matrix3[MATRIX3_LEDS];
extern volatile bool isTextScrolling;
extern WeatherData currentWeather;

void initDisplay();
uint16_t XY(uint8_t x, uint8_t y);
void setPhysicalPixel(uint16_t index, const CRGB &color);
void setPixel(int16_t x, int16_t y, const CRGB &color);
void clearMatrix();

void drawDigit(uint8_t digit, uint8_t startX, const CRGB &color);
void drawDigitWithOffset(uint8_t digit, uint8_t startX, int16_t startY, const CRGB &color);
void drawColon(bool visible, const CRGB &color);

void renderClock(bool clockIsValid, const DateTime &displayedTime);

void drawChar(char c, int16_t x, int16_t y, const CRGB &color, uint8_t scale = 2);
void drawDegree(int16_t x, int16_t y, const CRGB &color, uint8_t scale = 1, int16_t minX = 0, int16_t maxX = 47);
void drawText(const String &text, int16_t x, int16_t y, const CRGB &color, uint8_t scale = 2);
int16_t getTextWidth(const String &text, uint8_t scale = 2);
void scrollText(const String &text, uint16_t speedMs = DEFAULT_SCROLL_SPEED_MS, const CRGB &color = CRGB::White, uint8_t scale = 2);
void queueScrollText(const String &text);
void processPendingScroll();
void updateWeather(int16_t temp, const char* desc, uint8_t sunriseH = 255, uint8_t sunriseM = 0, uint8_t sunsetH = 255, uint8_t sunsetM = 0);
bool isDayTime(const DateTime &displayedTime);
void calculateSolarDawnDusk(const DateTime &dt, uint8_t &riseH, uint8_t &riseM, uint8_t &setH, uint8_t &setM);
void checkPeriodicWeatherDisplay();
void drawIcon16x16(int16_t startX, int16_t startY, const uint32_t* iconData);
void showWeatherScreen(uint32_t durationMs = 7000);
bool isWeatherScreenActive();
void renderWeatherScreen(const DateTime &displayedTime);
void runAnimationVariant(uint8_t variant = 255, uint32_t durationMs = 5000);
void playStartupAnimation(uint8_t variant = 255);
