#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <IRrecv.h>
#include <IRsend.h>
#include "settings.h"

using FeatureEvent = void (*)(JsonDocument&, const char* topic);
struct IrRemote {
  uint32_t magic = 0x49524731;
  char name[33] = {};
  uint64_t code = 0;
  int16_t protocol = -1;
  uint16_t bits = 0, carrierKhz = 38, count = 0;
  uint16_t pulses[Settings::IrRawPulses] = {};
  bool occupied = false, decoded = false;
};
class IrBridge {
public:
  IrBridge();
  bool begin(FeatureEvent event);
  void tick(bool allowReceive);
  const char* learn(unsigned slot, const char* name);
  const char* send(unsigned slot, unsigned repeats);
  const char* save(unsigned slot, const IrRemote& remote);
  const char* erase(unsigned slot);
  void stop();
  void enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return available_ && enabled_; }
  bool available() const { return available_; }
  bool learning() const { return learning_; }
  unsigned learnSlot() const { return learnSlot_; }
  unsigned secondsLeft() const;
  bool takeChanged();
  const IrRemote& at(unsigned slot) const { return remotes_[slot]; }
  String lastCode() const { return lastCode_; }
  uint32_t receivedCount() const { return receivedCount_; }
  static bool valid(const IrRemote& remote);
  static void toJson(JsonObject out, const IrRemote& remote, unsigned slot);
  static bool fromJson(JsonDocument& doc, IrRemote& remote);
private:
  IRrecv receiver_;
  IRsend sender_;
  Preferences preferences_;
  IrRemote remotes_[Settings::IrSlots];
  FeatureEvent event_ = nullptr;
  bool available_ = false, enabled_ = true, receiverRunning_ = false;
  bool learning_ = false, changed_ = false;
  unsigned learnSlot_ = 0;
  char learnName_[33] = {};
  uint32_t learnStarted_ = 0, lastSeen_ = 0, lastTransmit_ = 0, receivedCount_ = 0;
  String lastCode_;
};
