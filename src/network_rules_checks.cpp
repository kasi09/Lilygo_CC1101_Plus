#include "network_rules.h"

using namespace NetworkRules;
static_assert(text("WLAN", 4, 32, false), "SSID text");
static_assert(!text("", 0, 32, false), "SSID required");
static_assert(!text("012345678901234567890123456789012", 33, 32), "SSID byte limit");
static_assert(!text("bad\ntext", 8, 32), "control characters rejected");
static_assert(!text("a\0b", 3, 32), "embedded NUL rejected");
static_assert(wifiPassword("", 0), "explicit open network allowed");
static_assert(!wifiPassword("1234567", 7), "short WPA password rejected");
static_assert(wifiPassword("12345678", 8), "WPA lower bound");
static_assert(wifiPassword("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 64), "hex PSK");
static_assert(!wifiPassword("z123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 64), "invalid hex PSK");
static_assert(deviceId("lilygo_rf-2", 11), "topic-safe ID");
static_assert(!deviceId("bad/id", 6) && !deviceId("Bad", 3), "unsafe device ID");
static_assert(host("192.168.1.2", 11) && host("broker.local", 12), "host names and IPv4");
static_assert(!host("mqtt://broker", 13) && !host("a/b", 3), "host without URL scheme");
static_assert(port(1883) && port(65535) && !port(0) && !port(65536), "TCP port bounds");
static_assert(fallback(false, false, false, 0, 0), "unconfigured starts immediately");
static_assert(!fallback(true, false, false, 29999, 0), "connection grace period");
static_assert(fallback(true, false, false, 30000, 0), "fallback after grace period");
static_assert(!fallback(false, true, false, 60000, 0) && !fallback(false, false, true, 60000, 0), "connected or dismissed");
static_assert(fallback(true, false, false, 20000, UINT32_MAX - 10000), "millis wrap");
