#pragma once
#include <Arduino.h>
#include <Wire.h>

struct BoardReadings {
  bool gaugeReachable = false;
  bool batteryAvailable = false;
  bool currentAvailable = false;
  bool temperatureAvailable = false;
  bool chargerAvailable = false, usbPower = false;
  unsigned chargeState = 0, vbusType = 0;
  unsigned percent = 0;
  unsigned millivolts = 0;
  int milliamps = 0;
  float chipCelsius = 0;
  uint32_t sampledAt = 0;
};

// Read-only standard commands; no charger, calibration or gauge configuration.
class BoardTelemetry {
public:
  bool begin();
  bool tick(bool requestRefresh = false);
  BoardReadings readings() const;
private:
  bool readWord(uint8_t command, uint16_t& value);
  bool initialized_ = false, sampled_ = false;
  uint32_t lastAttempt_ = 0;
  BoardReadings data_;
};
