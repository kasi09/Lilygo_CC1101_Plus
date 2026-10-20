#include "setup_qr.h"
#include <cstring>

String SetupQr::escaped(const String& value) {
  String out;
  out.reserve(value.length() * 2);
  for (unsigned i = 0; i < value.length(); ++i) {
    const char ch = value[i];
    if (ch == '\\' || ch == ';' || ch == ',' || ch == ':' || ch == '"') out += '\\';
    out += ch;
  }
  return out;
}
bool SetupQr::update(const String& ssid, const String& password) {
  if (ssid.isEmpty() || password.isEmpty()) {
    ssid_ = ""; password_ = ""; valid_ = false;
    memset(bytes_, 0, sizeof(bytes_));
    return false;
  }
  if (valid_ && ssid == ssid_ && password == password_) return true;
  // Only the temporary device hotspot belongs in this QR, never the home network.
  const String payload = String("WIFI:T:WPA;S:") + escaped(ssid) + ";P:" + escaped(password) + ";;";
  uint8_t scratch[sizeof(bytes_)];
  valid_ = qrcodegen_encodeText(payload.c_str(), scratch, bytes_, qrcodegen_Ecc_MEDIUM,
                              Version, Version, qrcodegen_Mask_AUTO, true);
  memset(scratch, 0, sizeof(scratch));
  ssid_ = valid_ ? ssid : String(); password_ = valid_ ? password : String();
  return valid_;
}
String SetupQr::hex() const {
  if (!valid_) return "";
  constexpr char digits[] = "0123456789abcdef";
  String out;
  out.reserve(sizeof(bytes_) * 2);
  for (uint8_t byte : bytes_) { out += digits[byte >> 4]; out += digits[byte & 15]; }
  return out;
}
