#pragma once
#if __has_include("config.local.h")
#include "config.local.h"
#else
#include "config.example.h"
#endif

// Feature switches also work with an existing config.local.h from older builds.
#ifndef GW_SETUP_AP_ENABLED
#define GW_SETUP_AP_ENABLED true
#endif
#ifndef GW_MQTT_DISCOVERY
#define GW_MQTT_DISCOVERY true
#endif
#ifndef GW_ENABLE_NFC
#define GW_ENABLE_NFC true
#endif
#ifndef GW_ENABLE_IR
#define GW_ENABLE_IR true
#endif
#ifndef GW_ENABLE_RGB
#define GW_ENABLE_RGB true
#endif
#ifndef GW_ENABLE_AUDIO
#define GW_ENABLE_AUDIO true
#endif
#ifndef GW_ENABLE_MIC
#define GW_ENABLE_MIC true
#endif
#ifndef GW_ENABLE_NRF
#define GW_ENABLE_NRF true
#endif
#ifndef GW_ENABLE_SD
#define GW_ENABLE_SD true
#endif

namespace Settings {
constexpr unsigned Slots = 16;
constexpr unsigned RawPulses = 256;
constexpr unsigned LearnTimeoutMs = 15000;
constexpr unsigned RepeatCount = 6;
constexpr unsigned MaxTransmitUs = 500000;
constexpr unsigned TransmitCooldownMs = 1000;
constexpr unsigned EventDebounceMs = 500;
constexpr unsigned ScanDwellMs = 20;
constexpr float DefaultFrequency = 433.92f;
constexpr unsigned IrSlots = 8;
constexpr unsigned IrRawPulses = 128;
constexpr const char* DeviceName = "LILYGO T-Embed CC1101 Plus";
constexpr const char* Version = "0.5.2";
}
