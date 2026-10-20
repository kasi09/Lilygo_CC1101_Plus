#include "board_telemetry.h"
#include "board_pins.h"

namespace {
constexpr uint8_t GaugeAddress = 0x55;
// TI BQ27220 TRM SLUUBD4A, sections 2.6-2.8, 2.19, 2.21 and 5.3.
constexpr uint8_t Status = 0x0A, Voltage = 0x08, Current = 0x0C;
constexpr uint8_t InternalTemperature = 0x28, StateOfCharge = 0x2C;
constexpr uint16_t BatteryPresent = 1u << 3;
constexpr uint16_t gaugeWord(uint8_t low, uint8_t high) {
  return uint16_t(low) | (uint16_t(high) << 8);
}
constexpr int signedWord(uint16_t raw) {
  return raw <= 32767 ? int(raw) : int(raw) - 65536;
}
constexpr float chipTemperature(uint16_t raw) { return raw * 0.1f - 273.15f; }
constexpr unsigned chargeState(uint8_t raw) { return (raw >> 3) & 3; }
static_assert(chargeState(0x04) == 0 && chargeState(0x0C) == 1 &&
              chargeState(0x14) == 2 && chargeState(0x1C) == 3, "BQ25896 REG0B charge status bits");
static_assert(gaugeWord(0xA0, 0x0F) == 4000, "gauge voltage is little endian");
static_assert(signedWord(0xFF9C) == -100, "negative current must remain signed");
static_assert(signedWord(0x0064) == 100, "positive current");
static_assert(signedWord(0x8000) == -32768 && signedWord(0x7FFF) == 32767, "signed endpoints");
static_assert(chipTemperature(2982) > 25.0f && chipTemperature(2982) < 25.1f,
              "temperature conversion is from tenths of kelvin");
}

bool BoardTelemetry::begin() {
  initialized_ = Wire.begin(Pins::I2cSda, Pins::I2cScl, 100000);
  Wire.setTimeOut(20);
  return initialized_;
}
bool BoardTelemetry::readWord(uint8_t command, uint16_t& value) {
  Wire.beginTransmission(GaugeAddress);
  Wire.write(command);  // register pointer only; not a register-data write
  if (Wire.endTransmission(false) != 0) { delayMicroseconds(70); return false; }
  const auto count = Wire.requestFrom(GaugeAddress, size_t(2), true);
  if (count != 2) {
    while (Wire.available()) Wire.read();
    delayMicroseconds(70);
    return false;
  }
  const uint8_t low = Wire.read(), high = Wire.read();
  value = gaugeWord(low, high);
  delayMicroseconds(70);  // TI requires >=66 us bus-free time between packets.
  return true;
}
bool BoardTelemetry::tick(bool requestRefresh) {
  if (!initialized_) return false;
  const uint32_t now = millis();
  // Even repeated user refreshes never issue a complete poll faster than 1 Hz.
  if (sampled_ && now - lastAttempt_ < (requestRefresh ? 1000u : 5000u)) return false;
  sampled_ = true; lastAttempt_ = now;
  BoardReadings next;
  // BQ25896 REG0B is read-only; charger settings and fault registers stay untouched.
  Wire.beginTransmission(uint8_t(0x6B)); Wire.write(uint8_t(0x0B));
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(uint8_t(0x6B), size_t(1), true) == 1) {
    const uint8_t charger = Wire.read();
    next.chargerAvailable = true; next.usbPower = (charger & 4) != 0;
    next.chargeState = chargeState(charger); next.vbusType = charger >> 5;
  }
  while (Wire.available()) Wire.read();
  delayMicroseconds(70);
  uint16_t status = 0;
  if (readWord(Status, status)) {
    next.gaugeReachable = true;
    uint16_t voltage = 0, soc = 0, current = 0, temperature = 0;
    if (status & BatteryPresent) {
      const bool voltageOk = readWord(Voltage, voltage);
      const bool socOk = readWord(StateOfCharge, soc);
      next.batteryAvailable = voltageOk && socOk && voltage >= 2500 && voltage <= 5000 && soc <= 100;
      if (next.batteryAvailable) {
        next.millivolts = voltage; next.percent = soc;
        next.currentAvailable = readWord(Current, current) && current != 0x8000;
        if (next.currentAvailable) next.milliamps = signedWord(current);
      }
    }
    next.temperatureAvailable = readWord(InternalTemperature, temperature) &&
                                temperature >= 2000 && temperature <= 4000;
    if (next.temperatureAvailable) next.chipCelsius = chipTemperature(temperature);
  }
  next.sampledAt = millis();
  data_ = next;
  return true;
}
BoardReadings BoardTelemetry::readings() const {
  BoardReadings result = data_;
  if (!sampled_ || millis() - data_.sampledAt > 15000) {
    result.gaugeReachable = result.batteryAvailable = result.currentAvailable =
      result.temperatureAvailable = result.chargerAvailable = false;
  }
  return result;
}
