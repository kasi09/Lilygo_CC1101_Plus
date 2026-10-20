#pragma once
#include <cstddef>
#include <cstdint>

namespace NfcFrame {
// PN532 I2C: ready byte, preamble, LEN/LCS, TFI, payload, DCS, postamble.
constexpr uint8_t sum(const uint8_t* data, size_t first, size_t end) {
  return first == end ? 0 : uint8_t(data[first] + sum(data, first + 1, end));
}
// -1 = invalid/truncated, 0 = no target, 4/7/10 = safe UID length.
constexpr int uidLength(const uint8_t* f, size_t size) {
  return size < 11 || f[0] != 1 || f[1] != 0 || f[2] != 0 || f[3] != 0xFF ||
    uint8_t(f[4] + f[5]) != 0 || f[4] < 3 || size_t(f[4]) + 8 > size ||
    f[6] != 0xD5 || f[7] != 0x4B || sum(f, 6, 7 + f[4]) != 0 || f[7 + f[4]] != 0 ? -1 :
    f[8] == 0 ? (f[4] == 3 ? 0 : -1) :
    f[8] != 1 || f[4] < 8 ? -1 :
    (f[13] != 4 && f[13] != 7 && f[13] != 10) || f[4] < 8 + f[13] ? -1 : f[13];
}
constexpr bool ack(const uint8_t* f, size_t size) {
  return size == 7 && f[0] == 1 && f[1] == 0 && f[2] == 0 && f[3] == 0xFF &&
    f[4] == 0 && f[5] == 0xFF && f[6] == 0;
}
}
