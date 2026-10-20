#pragma once
#include <Arduino.h>
#include <qrcodegen.h>

// Fixed size keeps modules aligned to whole display pixels, with a four-module border.
class SetupQr {
public:
  static constexpr int Version = 4;
  static constexpr int Modules = Version * 4 + 17;
  static constexpr int Quiet = 4, Scale = 3;
  static constexpr int Pixels = (Modules + Quiet * 2) * Scale;
  bool update(const String& ssid, const String& password);
  bool valid() const { return valid_; }
  bool module(int x, int y) const { return valid_ && qrcodegen_getModule(bytes_, x, y); }
  String hex() const;
private:
  static String escaped(const String& value);
  uint8_t bytes_[qrcodegen_BUFFER_LEN_FOR_VERSION(Version)] = {};
  String ssid_, password_;
  bool valid_ = false;
};
