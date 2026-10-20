#pragma once
// Copy this file to config.local.h. Local credentials are ignored by Git.
#define GW_WIFI_SSID ""
#define GW_WIFI_PASSWORD ""
#define GW_MQTT_HOST ""
#define GW_MQTT_PORT 1883
#define GW_MQTT_USER ""
#define GW_MQTT_PASSWORD ""
// Use a unique lowercase ID per board: letters, digits, underscore, hyphen.
#define GW_DEVICE_ID "lilygo_rf"
#define GW_DISCOVERY_PREFIX "homeassistant"
// Start a password-protected setup hotspot without Wi-Fi or after 30 s offline.
#define GW_SETUP_AP_ENABLED true
// false: use the custom lilygo_t_embed integration; true: legacy MQTT Discovery.
#define GW_MQTT_DISCOVERY false
// Disables CC1101, IR and nRF24 sending when false.
// CC1101 TX is restricted to 433.05 .. 434.79 MHz.
#define GW_TX_ENABLED true
#define GW_TX_POWER_DBM 0
// Compile-time support. Microphone and nRF24 RX still start OFF at runtime.
#define GW_ENABLE_NFC true
#define GW_ENABLE_IR true
#define GW_ENABLE_RGB true
#define GW_ENABLE_AUDIO true
#define GW_ENABLE_MIC true
#define GW_ENABLE_NRF true
#define GW_ENABLE_SD true
