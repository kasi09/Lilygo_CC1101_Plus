#pragma once
#include <cstddef>
#include <cstdint>

namespace NetworkRules {
constexpr uint32_t FallbackMs = 30000;
constexpr uint32_t ReadyCloseMs = 10000;
constexpr bool textCharacters(const char* s, size_t n) {
  return n == 0 || (static_cast<unsigned char>(*s) >= 32 && *s != 127 &&
                    textCharacters(s + 1, n - 1));
}
constexpr bool text(const char* s, size_t n, size_t maximum, bool empty = true) {
  return s && n <= maximum && (empty || n != 0) && textCharacters(s, n);
}
constexpr bool hexCharacters(const char* s, size_t n) {
  return n == 0 || (((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') ||
                    (*s >= 'A' && *s <= 'F')) && hexCharacters(s + 1, n - 1));
}
constexpr bool wifiPassword(const char* s, size_t n) {
  return text(s, n, 64) && (n == 0 || (n >= 8 && n <= 63) ||
                           (n == 64 && hexCharacters(s, n)));
}
constexpr bool idCharacters(const char* s, size_t n) {
  return n == 0 || (((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9') ||
                    *s == '_' || *s == '-') && idCharacters(s + 1, n - 1));
}
constexpr bool deviceId(const char* s, size_t n) {
  return s && n > 0 && n <= 32 && idCharacters(s, n);
}
constexpr bool hostCharacters(const char* s, size_t n) {
  return n == 0 || (((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
                    (*s >= '0' && *s <= '9') || *s == '_' || *s == '-' || *s == '.') &&
                    hostCharacters(s + 1, n - 1));
}
constexpr bool host(const char* s, size_t n) {
  return s && n <= 253 && (n == 0 || (s[0] != '.' && s[n - 1] != '.' && hostCharacters(s, n)));
}
constexpr bool port(unsigned number) { return number >= 1 && number <= 65535; }
constexpr bool fallback(bool configured, bool connected, bool suppressed,
                        uint32_t now, uint32_t offlineSince) {
  return !connected && !suppressed && (!configured || uint32_t(now - offlineSince) >= FallbackMs);
}
}
