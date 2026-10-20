#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "settings.h"

enum class RemoteKind : uint8_t { Empty = 0, Rc = 1, Raw = 2 };
struct Remote {
  uint32_t magic = 0x52464731;
  RemoteKind kind = RemoteKind::Empty;
  char name[33] = {};
  float frequency = Settings::DefaultFrequency;
  uint32_t code = 0;
  uint16_t pulseUs = 350;
  uint8_t bits = 24;
  uint8_t protocol = 1;
  uint16_t count = 0;
  uint16_t gapUs = 10000;
  bool firstHigh = true;
  uint16_t pulses[Settings::RawPulses] = {};
};

class RemoteStore {
public:
  bool begin();
  bool save(unsigned slot, const Remote& remote);
  bool erase(unsigned slot);
  const Remote& at(unsigned slot) const { return remotes_[slot]; }
  unsigned count() const;
  static bool valid(const Remote& remote);
  static void toJson(JsonObject out, const Remote& remote, unsigned slot, bool raw = true);
private:
  Preferences preferences_;
  Remote remotes_[Settings::Slots];
};
