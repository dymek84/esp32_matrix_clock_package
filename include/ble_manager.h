#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#include "config.h"
#include "config_manager.h"
#include "display.h"
#include "rtc_ntp.h"
#include "alarm_manager.h"
#include "ota_updater.h"

#define BLE_DEVICE_NAME "ESP32_Clock"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

extern bool deviceConnected;
extern bool oldDeviceConnected;
extern BLECharacteristic *pCharacteristic;

void initBLE();
void processBLETask();
void sendBLENotification(const String &message);
void sendBLEStatus();
void processIncomingJson(const String &jsonPayload);
