#pragma once
#include <cstdint>
#include <cstddef>
namespace FeatureRules {
constexpr int hexDigit(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
         c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}
inline bool hexBytes(const char* text, uint8_t* bytes, size_t capacity, size_t& count) {
  count = 0;
  if (!text) return false;
  while (*text) {
    if (count >= capacity || !text[1]) return false;
    const int high = hexDigit(text[0]), low = hexDigit(text[1]);
    if (high < 0 || low < 0) return false;
    bytes[count++] = (high << 4) | low;
    text += 2;
  }
  return count != 0;
}
inline bool hex64(const char* text, uint64_t& number) {
  number = 0;
  if (!text) return false;
  if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) text += 2;
  size_t digits = 0;
  while (*text) {
    const int digit = hexDigit(*text++);
    if (digit < 0 || ++digits > 16) return false;
    number = (number << 4) | unsigned(digit);
  }
  return digits != 0;
}
constexpr bool nrfChannel(unsigned channel) { return channel <= 83; }
constexpr bool tone(unsigned hz, unsigned ms, unsigned volume) {
  return hz >= 200 && hz <= 4000 && ms >= 20 && ms <= 300 && volume <= 50;
}
static_assert(hexDigit('F') == 15 && hexDigit('a') == 10 && hexDigit('9') == 9, "hex values");
static_assert(hexDigit('G') == -1 && hexDigit('/') == -1, "invalid hex characters");
static_assert(nrfChannel(0) && nrfChannel(83) && !nrfChannel(84), "2.4 GHz channel bounds");
static_assert(tone(880, 150, 20) && !tone(0, 150, 20), "audible tone bounds");
static_assert(!tone(880, 301, 20) && !tone(880, 100, 51), "bounded tone duration and volume");
}
