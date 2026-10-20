// Compile-time regression checks; these execute in every firmware build and
// consume no flash or RAM. Hardware acceptance is documented separately.
#include "rf_rules.h"
#include <limits>
using namespace RfRules;
static_assert(receiveFrequency(300) && receiveFrequency(348), "lower RX band");
static_assert(receiveFrequency(387) && receiveFrequency(464), "middle RX band");
static_assert(receiveFrequency(779) && receiveFrequency(928), "upper RX band");
static_assert(!receiveFrequency(299.99f) && !receiveFrequency(928.01f), "RX bounds");
static_assert(!receiveFrequency(350) && !receiveFrequency(500), "RX gaps");
static_assert(!receiveFrequency(std::numeric_limits<float>::quiet_NaN()), "NaN frequency");
static_assert(!receiveFrequency(std::numeric_limits<float>::infinity()), "infinite frequency");
static_assert(transmitFrequency(433.05f) && transmitFrequency(434.79f), "TX endpoints");
static_assert(!transmitFrequency(433.04f) && !transmitFrequency(434.80f), "TX bounds");
static_assert(!transmitFrequency(315) && !transmitFrequency(868), "RX-only bands");
static_assert(!transmitFrequency(std::numeric_limits<float>::quiet_NaN()), "NaN TX");
static_assert(scanRange(433.05f, 434.79f, 0.05f), "433 scan preset");
static_assert(scanRange(863, 870, 0.1f), "868 scan preset");
static_assert(scanRange(300, 348, 0.5f), "all scan first band");
static_assert(scanRange(387, 464, 0.5f), "all scan middle band");
static_assert(scanRange(779, 928, 0.5f), "all scan upper band");
static_assert(!scanRange(348, 387, 0.5f), "cannot bridge unsupported gap");
static_assert(!scanRange(434, 433, 0.05f), "reversed scan");
static_assert(!scanRange(433, 434, 0), "zero step");
static_assert(!scanRange(433, 434, -1), "negative step");
static_assert(!scanRange(779, 928, 0.025f), "scan size bound");
static_assert(!scanRange(433, 434, std::numeric_limits<float>::quiet_NaN()), "NaN step");
static_assert(!scanRange(433, 434, std::numeric_limits<float>::infinity()), "infinite step");
static_assert(rcCode(5393, 24, 1, 350), "common outlet code");
static_assert(rcCode(0xffffffff, 32, 12, 1500), "full 32-bit code");
static_assert(!rcCode(256, 8, 1, 350), "code must fit bits");
static_assert(!rcCode(1, 0, 1, 350) && !rcCode(1, 33, 1, 350), "bit bounds");
static_assert(!rcCode(1, 24, 0, 350) && !rcCode(1, 24, 13, 350), "protocol bounds");
static_assert(!rcCode(1, 24, 1, 79) && !rcCode(1, 24, 1, 1501), "pulse bounds");
static_assert(rcAirtimeUs(24, 1, 350, 6) == 268800, "outlet airtime");
static_assert(rcAirtimeUs(32, 1, 350, 6) == 336000, "32-bit airtime");
static_assert(rcAirtimeUs(32, 8, 1500, 10) > 500000, "long sends exceed budget");
static_assert(rcAirtimeUs(24, 0, 350, 6) == UINT64_MAX, "invalid protocol airtime");
static_assert(rcAirtimeUs(24, 1, 350, 0) == UINT64_MAX, "invalid repeats airtime");
