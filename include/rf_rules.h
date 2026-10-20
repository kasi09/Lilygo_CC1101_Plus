#pragma once
#include <cstdint>
#include <cmath>

namespace RfRules {
constexpr bool receiveFrequency(float mhz) {
  return std::isfinite(mhz) && ((mhz >= 300 && mhz <= 348) ||
         (mhz >= 387 && mhz <= 464) || (mhz >= 779 && mhz <= 928));
}
constexpr bool transmitFrequency(float mhz) {
  return std::isfinite(mhz) && mhz >= 433.05f && mhz <= 434.79f;
}
constexpr bool scanRange(float start, float end, float step) {
  return receiveFrequency(start) && receiveFrequency(end) && start <= end &&
         std::isfinite(step) && step >= 0.025f && step <= 2.0f &&
         ((start >= 300 && end <= 348) || (start >= 387 && end <= 464) ||
          (start >= 779 && end <= 928)) && (end - start) / step <= 2000;
}
constexpr bool rcCode(uint32_t code, unsigned bits, unsigned protocol, unsigned pulse) {
  return bits >= 1 && bits <= 32 && protocol >= 1 && protocol <= 12 &&
         pulse >= 80 && pulse <= 1500 && (bits == 32 || code < (uint32_t(1) << bits));
}
// Timing factors from rc-switch 2.6.4's protocol definitions (LGPL-2.1+).
// This computes an upper bound before entering its blocking send routine.
namespace Detail {
constexpr unsigned Sync[] = {32, 11, 101, 7, 20, 24, 64, 133, 137, 19, 37, 37};
constexpr unsigned Bit[] = {4, 3, 15, 4, 3, 3, 7, 23, 23, 4, 3, 3};
}
constexpr uint64_t rcAirtimeUs(unsigned bits, unsigned protocol, unsigned pulse,
                              unsigned repeats) {
  return protocol < 1 || protocol > 12 || bits < 1 || bits > 32 ||
         pulse < 80 || pulse > 1500 || repeats < 1 || repeats > 10 ? UINT64_MAX :
    uint64_t(Detail::Sync[protocol - 1] + bits * Detail::Bit[protocol - 1]) * pulse * repeats;
}
}
