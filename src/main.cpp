#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <esp_system.h>
#include "radio_gateway.h"
#include "rf_rules.h"
#include "board_telemetry.h"
#include "gateway_display.h"
#include "network_manager.h"

RemoteStore store;
RadioGateway rf;
TFT_eSPI screen;
GatewayDisplay display(screen);
BoardTelemetry board;
DeviceFeatures features(store);
NetworkManager connectivity;
ScanDisplayData scanView;
Remote lastReceivedRecord;
DisplayPage displayPage = DisplayPage::Overview;
class GatewayWiFiClient : public WiFiClient {
public:
  int connect(IPAddress ip, uint16_t port) override {
    return WiFiClient::connect(ip, port, 1000);
  }
  int connect(const char* host, uint16_t port) override {
    return WiFiClient::connect(host, port, 1000);
  }
};
GatewayWiFiClient network;
PubSubClient mqtt(network);
String baseTopic, mqttSession;
String commandRequestId;
String serialLine;
bool serialOverflow = false, storeReady = false, learning = false, rawLearning = false;
bool discoveryPending = false;
bool displayDirty = true;
bool featureStatePending = false;
unsigned learnSlot = 0, selectedSlot = 0, scanPreset = 0;
unsigned selectedIr = 0, selectedControl = 0;
char learnName[33] = {};
uint32_t learnStarted = 0, rxCount = 0, txCount = 0;
uint32_t lastRxAt = 0, lastCode = 0, lastProtocol = 0, lastBits = 0;
float lastRxFrequency = 0;
uint32_t lastNetworkTry = 0, lastStateAt = 0, lastDisplayAt = 0;
uint32_t scanPoints = 0;
float scanPeak = -127, scanPeakFrequency = 0;
String notice = "Bereit";
constexpr const char* scanBands[] = {"433", "868", "all"};

unsigned scanPointCount(float lower, float upper, float step) {
  return unsigned(floorf((upper - lower + 0.0001f) / step)) + 1;
}
void previewScan(unsigned preset) {
  if (preset == 0) scanView.reset(433.05f, 434.79f, scanPointCount(433.05f, 434.79f, 0.05f));
  else if (preset == 1) scanView.reset(863, 870, scanPointCount(863, 870, 0.1f));
  else scanView.reset(300, 928, 551);
}

bool publish(const String& suffix, const String& payload, bool retain = false) {
  return mqtt.connected() && mqtt.publish((baseTopic + "/" + suffix).c_str(), payload.c_str(), retain);
}
void emit(JsonDocument& doc, const char* suffix = "events") {
  String payload;
  serializeJson(doc, payload);
  Serial.println(payload);
  publish(suffix, payload);
}
void result(const char* action, const char* error = nullptr) {
  JsonDocument doc;
  doc["event"] = "result"; doc["action"] = action;
  doc["ok"] = error == nullptr;
  if (commandRequestId.length()) doc["request_id"] = commandRequestId;
  if (error) doc["error"] = error;
  notice = GatewayDisplay::resultText(action, error);
  displayDirty = true;
  emit(doc);
}
void featureEvent(JsonDocument& doc, const char* suffix) {
  const char* event = doc["event"] | "";
  if (!strcmp(event, "ir_learned")) {
    discoveryPending = true; notice = String("IR gespeichert: ") + (doc["name"] | "");
  } else if (!strcmp(event, "tag_scanned")) notice = String("NFC ") + (doc["uid"] | "");
  else if (!strcmp(event, "ir_received")) notice = String("IR ") + (doc["code"] | "");
  else if (!strcmp(event, "result")) notice = GatewayDisplay::resultText(doc["action"] | "", doc["error"] | "");
  else if (!strcmp(event, "press") && doc["slot"].is<unsigned>())
    publish(String("ir/remote/") + doc["slot"].as<unsigned>() + "/event", "press");
  emit(doc, suffix); displayDirty = true; featureStatePending = true;
}
void state() {
  JsonDocument doc;
  doc["event"] = "state"; doc["version"] = Settings::Version;
  doc["mode"] = features.ir.learning() ? "learn_ir" : learning ? (rawLearning ? "learn_raw" : "learn_rc") : rf.modeName();
  doc["frequency_mhz"] = rf.frequency(); doc["rssi_dbm"] = rf.rssi();
  doc["rx_count"] = rxCount; doc["tx_count"] = txCount;
  doc["saved"] = store.count(); doc["storage_ok"] = storeReady;
  doc["radio_error"] = rf.error(); doc["uptime_s"] = millis() / 1000;
  doc["wifi_rssi_dbm"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127;
  doc["session"] = mqttSession;
  doc["discovery_enabled"] = bool(connectivity.profile().discovery);
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt_connected"] = mqtt.connected();
  doc["setup_active"] = connectivity.portalActive();
  doc["network_config_saved"] = connectivity.saved();
  doc["network_storage_ok"] = connectivity.storageReady();
  doc["free_heap_bytes"] = ESP.getFreeHeap();
  features.appendState(doc);
  const BoardReadings values = board.readings();
  doc["battery_available"] = values.batteryAvailable;
  doc["current_available"] = values.currentAvailable;
  doc["chip_temperature_available"] = values.temperatureAvailable;
  doc["gauge_reachable"] = values.gaugeReachable;
  // Explicit nulls keep absent/stale hardware readings from becoming fake zeroes.
  if (values.batteryAvailable) {
    doc["battery_pct"] = values.percent;
    doc["battery_voltage_v"] = values.millivolts / 1000.0f;
  } else { doc["battery_pct"] = nullptr; doc["battery_voltage_v"] = nullptr; }
  if (values.currentAvailable) doc["battery_current_ma"] = values.milliamps;
  else doc["battery_current_ma"] = nullptr;
  if (values.temperatureAvailable) doc["gauge_temperature_c"] = values.chipCelsius;
  else doc["gauge_temperature_c"] = nullptr;
  doc["charger_available"] = values.chargerAvailable;
  if (values.chargerAvailable) {
    doc["usb_power"] = values.usbPower;
    doc["charging"] = values.chargeState == 1 || values.chargeState == 2;
    constexpr const char* states[] = {"idle", "precharge", "charging", "done"};
    doc["charge_status"] = states[values.chargeState];
  } else { doc["usb_power"] = nullptr; doc["charging"] = nullptr; doc["charge_status"] = nullptr; }
  emit(doc, "state");
  JsonDocument light; features.lightState(light);
  String lightPayload; serializeJson(light, lightPayload); publish("light/state", lightPayload, true);
  featureStatePending = false;
  lastStateAt = millis();
}
void device(JsonObject out) {
  out["identifiers"].to<JsonArray>().add(connectivity.profile().deviceId);
  out["name"] = Settings::DeviceName;
  out["manufacturer"] = "LILYGO"; out["model"] = "T-Embed CC1101 Plus";
  out["sw_version"] = Settings::Version;
}
void discoveryBase(JsonDocument& doc, const String& id, const String& name) {
  doc["name"] = name;
  doc["unique_id"] = String(connectivity.profile().deviceId) + "_" + id;
  doc["availability_topic"] = baseTopic + "/availability";
  device(doc["device"].to<JsonObject>());
}
void discoveryPublish(const char* component, const String& id, JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  mqtt.publish((String(GW_DISCOVERY_PREFIX) + "/" + component + "/" + connectivity.profile().deviceId +
                "/" + id + "/config").c_str(), payload.c_str(), true);
}
void removeDiscovery(const char* component, const String& id) {
  mqtt.publish((String(GW_DISCOVERY_PREFIX) + "/" + component + "/" + connectivity.profile().deviceId +
                "/" + id + "/config").c_str(), "", true);
}
void discoverButton(const String& id, const String& name, const char* action, int slot = -1) {
  JsonDocument doc, command;
  discoveryBase(doc, id, name);
  doc["command_topic"] = baseTopic + "/command";
  command["action"] = action; command["session"] = mqttSession;
  if (slot >= 0) command["slot"] = slot;
  if (!strcmp(action, "scan")) command["band"] = "433";
  String payload; serializeJson(command, payload);
  doc["payload_press"] = payload;
  discoveryPublish("button", id, doc);
}
void conditionalAvailability(JsonDocument& doc, const char* flag) {
  if (!flag) return;
  doc.remove("availability_topic"); doc["availability_mode"] = "all";
  JsonArray availability = doc["availability"].to<JsonArray>();
  availability.add<JsonObject>()["topic"] = baseTopic + "/availability";
  JsonObject measurement = availability.add<JsonObject>();
  measurement["topic"] = baseTopic + "/state";
  measurement["value_template"] = String("{{ 'online' if (value_json.") + flag +
    " | default(false)) else 'offline' }}";
}
void discoverTrigger(const String& id, const String& topic, const char* payload) {
  JsonDocument doc;
  doc["automation_type"] = "trigger"; doc["topic"] = topic;
  doc["payload"] = payload; doc["type"] = id.endsWith("_hold") ? "button_long_press" : "button_short_press"; doc["subtype"] = id;
  device(doc["device"].to<JsonObject>()); discoveryPublish("device_automation", id, doc);
}
void discoverFeatures() {
  struct Sensor { const char* id; const char* name; const char* unit; const char* valid; };
  const Sensor sensors[] = {
    {"nfc_uid", "Letzter NFC Tag", nullptr, "nfc_available"},
    {"nfc_count", "NFC Tags", nullptr, "nfc_available"},
    {"ir_last_code", "Letzter IR Code", nullptr, "ir_available"},
    {"ir_count", "IR Telegramme", nullptr, "ir_available"},
    {"nrf_last_payload", "Letztes nRF24 Paket", nullptr, "nrf_available"},
    {"nrf_count", "nRF24 Pakete", nullptr, "nrf_available"},
    {"noise_dbfs", "Relativer Mikrofonpegel", "dBFS", "noise_available"},
    {"charge_status", "Ladestatus", nullptr, "charger_available"},
    {"sd_total_mb", "SD Kapazitaet", "MiB", "sd_mounted"},
    {"sd_used_mb", "SD belegt", "MiB", "sd_mounted"},
    {"uptime_s", "Betriebszeit", "s", nullptr},
    {"free_heap_bytes", "Freier RAM", "B", nullptr}
  };
  for (const auto& sensor : sensors) {
    JsonDocument doc; discoveryBase(doc, sensor.id, sensor.name);
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = String("{{ value_json.") + sensor.id + " }}";
    doc["entity_category"] = "diagnostic";
    if (sensor.unit) { doc["unit_of_measurement"] = sensor.unit; doc["state_class"] = "measurement"; }
    conditionalAvailability(doc, sensor.valid); discoveryPublish("sensor", sensor.id, doc);
  }
  struct Binary { const char* id; const char* name; const char* deviceClass; const char* valid; };
  const Binary binaries[] = {
    {"usb_power", "USB Versorgung", "power", "charger_available"},
    {"charging", "Akku laedt", "battery_charging", "charger_available"},
    {"sd_mounted", "SD Karte eingebunden", nullptr, nullptr}
  };
  for (const auto& sensor : binaries) {
    JsonDocument doc; discoveryBase(doc, sensor.id, sensor.name);
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = String("{{ 'ON' if value_json.") + sensor.id + " else 'OFF' }}";
    if (sensor.deviceClass) doc["device_class"] = sensor.deviceClass;
    doc["entity_category"] = "diagnostic";
    conditionalAvailability(doc, sensor.valid); discoveryPublish("binary_sensor", sensor.id, doc);
  }
  struct Switch { const char* feature; const char* flag; const char* name; };
  const Switch switches[] = {
    {"nfc", "nfc_enabled", "NFC Leser"}, {"ir", "ir_enabled", "IR Empfang"},
    {"nrf", "nrf_enabled", "nRF24 Empfang"}, {"microphone", "microphone_enabled", "Mikrofonpegel"},
    {"screen", "screen_on", "Display"}
  };
  for (const auto& sw : switches) {
    JsonDocument doc; discoveryBase(doc, sw.flag, sw.name);
    doc["command_topic"] = baseTopic + "/command"; doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = String("{{ 'ON' if value_json.") + sw.flag + " else 'OFF' }}";
    doc["state_on"] = "ON"; doc["state_off"] = "OFF";
    for (bool on : {false, true}) {
      JsonDocument command; command["session"] = mqttSession;
      command["action"] = !strcmp(sw.feature, "screen") ? "screen_set" : "feature_set";
      command["feature"] = sw.feature; command["enabled"] = on;
      String payload; serializeJson(command, payload); doc[on ? "payload_on" : "payload_off"] = payload;
    }
    discoveryPublish("switch", sw.flag, doc);
  }
  if (GW_ENABLE_NFC) {
    JsonDocument doc; device(doc["device"].to<JsonObject>());
    doc["topic"] = baseTopic + "/nfc/tag"; doc["value_template"] = "{{ value_json.uid }}";
    discoveryPublish("tag", "nfc_reader", doc);
  }
  if (GW_ENABLE_RGB) {
    JsonDocument doc; discoveryBase(doc, "rgb", "RGB Ring");
    doc["schema"] = "json"; doc["command_topic"] = baseTopic + "/light/set";
    doc["state_topic"] = baseTopic + "/light/state"; doc["brightness"] = true;
    doc["supported_color_modes"].to<JsonArray>().add("rgb");
    doc["effect"] = true; JsonArray effects = doc["effect_list"].to<JsonArray>();
    effects.add("Statisch"); effects.add("Status"); discoveryPublish("light", "rgb", doc);
  }
  discoverButton("beep", "Signalton", "beep");
  discoverButton("backup", "Codes auf SD sichern", "backup");
  discoverButton("probe_features", "Hardware erneut pruefen", "probe_features");
  discoverButton("nrf_scan", "2.4 GHz Aktivitaet pruefen", "nrf_scan");
  for (unsigned slot = 0; slot < Settings::IrSlots; ++slot) {
    const String id = String("ir_slot_") + slot;
    if (!features.ir.at(slot).occupied) {
      removeDiscovery("button", id); removeDiscovery("device_automation", id); continue;
    }
    discoverButton(id, String(features.ir.at(slot).name) + " senden", "ir_send", slot);
    discoverTrigger(id, baseTopic + "/ir/remote/" + slot + "/event", "press");
  }
  for (const char* input : {"encoder_left", "encoder_right", "encoder_click", "encoder_hold", "side_click", "side_hold"})
    discoverTrigger(input, baseTopic + "/input", input);
}
void discovery() {
  if (!connectivity.profile().discovery) { discoveryPending = false; state(); return; }
  struct Sensor { const char* id; const char* name; const char* unit; };
  const Sensor sensors[] = {
    {"frequency_mhz", "Empfangsfrequenz", "MHz"}, {"rssi_dbm", "Funkpegel", "dBm"},
    {"rx_count", "Empfangene Telegramme", nullptr}, {"tx_count", "Gesendete Befehle", nullptr},
    {"mode", "Betriebsmodus", nullptr}, {"saved", "Gespeicherte Codes", nullptr},
    {"wifi_rssi_dbm", "WLAN Signal", "dBm"}
  };
  for (const auto& sensor : sensors) {
    JsonDocument doc;
    discoveryBase(doc, sensor.id, sensor.name);
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = String("{{ value_json.") + sensor.id + " }}";
    if (sensor.unit) { doc["unit_of_measurement"] = sensor.unit; doc["state_class"] = "measurement"; }
    if (!strcmp(sensor.id, "wifi_rssi_dbm")) doc["device_class"] = "signal_strength";
    doc["entity_category"] = "diagnostic";
    discoveryPublish("sensor", sensor.id, doc);
  }
  struct BoardSensor { const char* id; const char* name; const char* unit; const char* deviceClass; const char* valid; };
  const BoardSensor boardSensors[] = {
    {"battery_pct", "Akku", "%", "battery", "battery_available"},
    {"battery_voltage_v", "Akkuspannung", "V", "voltage", "battery_available"},
    {"battery_current_ma", "Akkustrom", "mA", "current", "current_available"},
    {"gauge_temperature_c", "Akku-IC Chiptemperatur", "\xC2\xB0" "C", "temperature", "chip_temperature_available"}
  };
  for (const auto& sensor : boardSensors) {
    JsonDocument doc;
    discoveryBase(doc, sensor.id, sensor.name);
    doc["state_topic"] = baseTopic + "/state";
    doc["value_template"] = String("{{ value_json.") + sensor.id + " }}";
    doc["unit_of_measurement"] = sensor.unit; doc["device_class"] = sensor.deviceClass;
    doc["state_class"] = "measurement"; doc["entity_category"] = "diagnostic";
    doc.remove("availability_topic");
    doc["availability_mode"] = "all";
    JsonArray availability = doc["availability"].to<JsonArray>();
    availability.add<JsonObject>()["topic"] = baseTopic + "/availability";
    JsonObject measurement = availability.add<JsonObject>();
    measurement["topic"] = baseTopic + "/state";
    measurement["value_template"] = String("{{ 'online' if (value_json.") + sensor.valid +
      " | default(false)) else 'offline' }}";
    discoveryPublish("sensor", sensor.id, doc);
  }
  discoverButton("scan_433", "433 MHz scannen", "scan");
  discoverButton("stop", "Empfang fortsetzen", "stop");
  discoverFeatures();
  for (unsigned slot = 0; slot < Settings::Slots; ++slot) {
    const String id = String("slot_") + slot;
    const Remote& remote = store.at(slot);
    if (remote.kind == RemoteKind::Empty) {
      removeDiscovery("button", id);
      removeDiscovery("device_automation", id);
      continue;
    }
    discoverButton(id, String(remote.name) + " senden", "send", slot);
    if (remote.kind != RemoteKind::Rc) {
      removeDiscovery("device_automation", id);
      continue;
    }
    JsonDocument doc;
    doc["automation_type"] = "trigger";
    doc["topic"] = baseTopic + "/remote/" + slot + "/event";
    doc["payload"] = "press"; doc["type"] = "button_short_press";
    doc["subtype"] = id;
    device(doc["device"].to<JsonObject>());
    discoveryPublish("device_automation", id, doc);
  }
  discoveryPending = false;
  state();
}

bool slotFrom(JsonDocument& doc, unsigned& slot) {
  if (!doc["slot"].is<unsigned>()) return false;
  slot = doc["slot"].as<unsigned>();
  return slot < Settings::Slots;
}
bool decimalCode(JsonVariantConst value, uint32_t& out) {
  if (value.is<uint32_t>()) { out = value.as<uint32_t>(); return true; }
  if (!value.is<const char*>()) return false;
  const char* text = value.as<const char*>();
  if (!*text || strlen(text) > 10) return false;
  uint64_t number = 0;
  for (; *text; ++text) {
    if (*text < '0' || *text > '9') return false;
    number = number * 10 + (*text - '0');
    if (number > UINT32_MAX) return false;
  }
  out = number;
  return true;
}
bool remoteFrom(JsonDocument& doc, Remote& remote) {
  if ((!doc["frequency_mhz"].isUnbound() && !doc["frequency_mhz"].is<float>()) ||
      (!doc["kind"].isUnbound() && !doc["kind"].is<const char*>()) ||
      (!doc["name"].isUnbound() && !doc["name"].is<const char*>())) return false;
  const char* kind = doc["kind"] | "rc";
  remote = Remote{};
  remote.frequency = doc["frequency_mhz"] | Settings::DefaultFrequency;
  const char* name = doc["name"] | "Code";
  if (strlen(name) > 32) return false;
  strlcpy(remote.name, name, sizeof(remote.name));
  if (!strcmp(kind, "rc")) {
    if (!decimalCode(doc["code"], remote.code) || !doc["bits"].is<uint8_t>() ||
        !doc["protocol"].is<uint8_t>() || !doc["pulse_us"].is<uint16_t>()) return false;
    remote.kind = RemoteKind::Rc;
    remote.bits = doc["bits"]; remote.protocol = doc["protocol"]; remote.pulseUs = doc["pulse_us"];
  } else if (!strcmp(kind, "raw")) {
    JsonArrayConst pulses = doc["pulses_us"].as<JsonArrayConst>();
    if (pulses.isNull() || pulses.size() > Settings::RawPulses ||
        !doc["first_high"].is<bool>() || !doc["gap_us"].is<uint16_t>()) return false;
    remote.kind = RemoteKind::Raw;
    remote.firstHigh = doc["first_high"]; remote.gapUs = doc["gap_us"];
    for (JsonVariantConst pulse : pulses) {
      if (!pulse.is<uint16_t>()) return false;
      remote.pulses[remote.count++] = pulse.as<uint16_t>();
    }
  } else return false;
  return RemoteStore::valid(remote);
}
void cancelLearn() { learning = false; rawLearning = false; }
void command(JsonDocument& doc) {
  const char* action = doc["action"] | "";
  unsigned slot = 0;
  if (!strcmp(action, "status")) { state(); return; }
  if (!strcmp(action, "list")) {
    for (unsigned i = 0; i < Settings::Slots; ++i) {
      JsonDocument out;
      RemoteStore::toJson(out.to<JsonObject>(), store.at(i), i);
      out["event"] = "remote";
      emit(out, "remotes");
    }
    return;
  }
  if (!strcmp(action, "stop")) {
    if (rf.mode() == RadioGateway::Mode::Scan) scanView.aborted = true;
    features.ir.stop(); cancelLearn(); result(action, rf.stop() ? nullptr : "radio_error"); state(); return;
  }
  const char* featureError = nullptr;
  if (features.command(doc, featureError, learning || rf.mode() == RadioGateway::Mode::Scan)) {
    if (!featureError && (!strcmp(action, "ir_store") || !strcmp(action, "ir_delete"))) discoveryPending = true;
    if (!featureError && !strcmp(action, "ir_learn")) {
      selectedIr = features.ir.learnSlot(); displayPage = DisplayPage::Infrared;
    }
    result(action, featureError); state(); return;
  }
  if (features.ir.learning()) { result(action, "device_busy"); return; }
  if (!strcmp(action, "tune")) {
    if (!doc["frequency_mhz"].is<float>() || !RfRules::receiveFrequency(doc["frequency_mhz"])) {
      result(action, "invalid_frequency"); return;
    }
    const bool ok = rf.tune(doc["frequency_mhz"]);
    if (ok) cancelLearn();
    result(action, ok ? nullptr : "radio_error"); state(); return;
  }
  if (!strcmp(action, "scan")) {
    bool ok;
    if (doc["start_mhz"].is<float>() && doc["end_mhz"].is<float>() && doc["step_mhz"].is<float>())
      ok = rf.scan(doc["start_mhz"], doc["end_mhz"], doc["step_mhz"]);
    else if (!doc["start_mhz"].isUnbound() || !doc["end_mhz"].isUnbound() || !doc["step_mhz"].isUnbound())
      ok = false;
    else ok = rf.scan(doc["band"] | "433");
    if (ok) {
      cancelLearn(); scanPoints = 0; scanPeak = -127; scanPeakFrequency = 0;
      displayPage = DisplayPage::Scan;
      if (doc["start_mhz"].is<float>()) {
        scanView.reset(doc["start_mhz"], doc["end_mhz"],
          scanPointCount(doc["start_mhz"], doc["end_mhz"], doc["step_mhz"]));
      } else {
        const char* band = doc["band"] | "433";
        scanPreset = !strcmp(band, "868") ? 1 : !strcmp(band, "all") ? 2 : 0;
        previewScan(scanPreset);
      }
    }
    result(action, ok ? nullptr : "scan_rejected"); state(); return;
  }
  if (!strcmp(action, "learn")) {
    if (!storeReady) { result(action, "storage_unavailable"); return; }
    if (!slotFrom(doc, slot)) { result(action, "invalid_slot"); return; }
    if ((!doc["kind"].isUnbound() && !doc["kind"].is<const char*>()) ||
        (!doc["name"].isUnbound() && !doc["name"].is<const char*>())) {
      result(action, "invalid_learn_fields"); return;
    }
    const char* kind = doc["kind"] | "rc";
    if (strcmp(kind, "rc") && strcmp(kind, "raw")) { result(action, "invalid_kind"); return; }
    const char* name = doc["name"] | "";
    if (strlen(name) > 32) { result(action, "name_too_long"); return; }
    bool raw = !strcmp(kind, "raw");
    if (!(raw ? rf.startRaw() : rf.stop())) { result(action, "radio_error"); return; }
    learnSlot = slot; selectedSlot = slot; rawLearning = raw; learning = true;
    displayPage = DisplayPage::Learn;
    if (*name) strlcpy(learnName, name, sizeof(learnName));
    else snprintf(learnName, sizeof(learnName), "Code %u", slot);
    learnStarted = millis();
    result(action); notice = "Taste der Fernbedienung druecken"; state(); return;
  }
  if (!strcmp(action, "store")) {
    if (!storeReady) { result(action, "storage_unavailable"); return; }
    if (!slotFrom(doc, slot)) { result(action, "invalid_slot"); return; }
    Remote remote;
    if (!remoteFrom(doc, remote)) { result(action, "invalid_remote"); return; }
    const bool ok = store.save(slot, remote);
    if (ok) discoveryPending = true;
    result(action, ok ? nullptr : "storage_write_failed"); return;
  }
  if (!strcmp(action, "delete")) {
    if (!storeReady) { result(action, "storage_unavailable"); return; }
    if (!slotFrom(doc, slot)) { result(action, "invalid_slot"); return; }
    const bool ok = store.erase(slot);
    if (ok) { discoveryPending = true; if (learning && learnSlot == slot) { cancelLearn(); rf.stop(); } }
    result(action, ok ? nullptr : "storage_write_failed"); return;
  }
  if (!strcmp(action, "send")) {
    if (!slotFrom(doc, slot)) { result(action, "invalid_slot"); return; }
    if (!doc["repeats"].isUnbound() && !doc["repeats"].is<unsigned>()) {
      result(action, "invalid_repeats"); return;
    }
    const char* error = rf.send(store.at(slot), doc["repeats"] | Settings::RepeatCount);
    if (!error) { ++txCount; cancelLearn(); }
    result(action, error); state(); return;
  }
  result(action, "unknown_action");
}
void parseCommand(const char* payload, size_t size, bool fromMqtt) {
  commandRequestId = "";
  JsonDocument doc;
  auto error = deserializeJson(doc, payload, size);
  if (error || !doc.is<JsonObject>()) { result("parse", "invalid_json"); return; }
  // Optional correlation for native HA commands, never copied into later events.
  const char* requestId = doc["request_id"] | "";
  bool validRequestId = strlen(requestId) <= 32;
  for (const char* p = requestId; *p; ++p)
    if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) validRequestId = false;
  if (!validRequestId) { result("parse", "invalid_request_id"); return; }
  commandRequestId = requestId;
  const char* action = doc["action"] | "";
  if (!strcmp(action, "network_setup") || !strcmp(action, "setup_info") || !strcmp(action, "display_info") || !strcmp(action, "network_info")) {
    if (fromMqtt) { result(action, "usb_only"); }
    else if (!strcmp(action, "network_info")) {
      const bool busy = learning || features.ir.learning() || rf.mode() == RadioGateway::Mode::Scan;
      JsonDocument info; auto details = info.to<JsonObject>(); details["event"] = "network_info";
      if (doc["scan"] == true) {
        const char* error = busy ? "device_busy" : connectivity.requestScan();
        if (error) details["error"] = error;
      }
      connectivity.diagnostics(details, doc["page"] == true, doc["networks"] == true);
      serializeJson(info, Serial); Serial.println();
    }
    else if (!strcmp(action, "display_info")) {
      JsonDocument info; auto details = info.to<JsonObject>();
      details["event"] = "display_info"; display.diagnostics(details);
      serializeJson(info, Serial); Serial.println();
    }
    else if (!strcmp(action, "network_setup")) {
      const bool busy = learning || features.ir.learning() || rf.mode() == RadioGateway::Mode::Scan;
      if (!doc["enabled"].isUnbound() && !doc["enabled"].is<bool>()) result(action, "invalid_enabled");
      else if (busy) result(action, "device_busy");
      else if (doc["enabled"].is<bool>() && !doc["enabled"].as<bool>()) {
        connectivity.closePortal(true); result(action);
      }
      else {
        const bool opened = connectivity.openPortal();
        if (opened) { features.screenOn(true); displayPage = DisplayPage::Network; }
        result(action, opened ? nullptr : "setup_unavailable");
      }
    } else {
      // Explicit local USB request only; temporary AP password never reaches MQTT/status/logs.
      JsonDocument info; info["event"] = "setup_info";
      info["active"] = connectivity.portalActive();
      if (connectivity.portalActive()) {
        info["ssid"] = connectivity.portalSsid(); info["password"] = connectivity.portalPassword();
        info["url"] = "http://192.168.4.1/";
        if (doc["qr"].is<bool>() && doc["qr"].as<bool>()) display.setupQrInfo(info.as<JsonObject>());
      }
      serializeJson(info, Serial); Serial.println();
    }
    commandRequestId = ""; return;
  }
  // A new session on EVERY MQTT reconnect prevents retained old sends from running.
  if (fromMqtt && (!doc["session"].is<const char*>() ||
      mqttSession != doc["session"].as<const char*>())) {
    result("mqtt", "stale_or_missing_session"); commandRequestId = ""; return;
  }
  command(doc);
  commandRequestId = "";
}
void mqttMessage(char* topic, byte* payload, unsigned int length) {
  if (String(topic) == String(GW_DISCOVERY_PREFIX) + "/status") {
    if (length == 6 && !memcmp(payload, "online", 6)) discoveryPending = true;
  } else if (String(topic) == baseTopic + "/command") {
    parseCommand(reinterpret_cast<const char*>(payload), length, true);
  } else if (String(topic) == baseTopic + "/light/set") {
    // HA's JSON-light schema controls only this reversible light state.
    JsonDocument doc;
    const auto error = deserializeJson(doc, payload, length);
    result("light_set", error || !doc.is<JsonObject>() ? "invalid_json" : features.setLight(doc));
    state();
  }
}
void networkTick() {
  if (WiFi.status() != WL_CONNECTED || !connectivity.profile().mqttHost[0]) return;
  if (!mqtt.connected() && millis() - lastNetworkTry >= 5000) {
    lastNetworkTry = millis();
    char token[24]; snprintf(token, sizeof(token), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    mqttSession = token;
    const String availability = baseTopic + "/availability";
    if (mqtt.connect(connectivity.profile().deviceId, connectivity.profile().mqttUser, connectivity.profile().mqttPassword,
                     availability.c_str(), 1, true, "offline")) {
      publish("availability", "online", true);
      mqtt.subscribe((baseTopic + "/command").c_str());
      mqtt.subscribe((baseTopic + "/light/set").c_str());
      mqtt.subscribe((String(GW_DISCOVERY_PREFIX) + "/status").c_str());
      discoveryPending = true;
    }
  }
  mqtt.loop();
  if (mqtt.connected() && discoveryPending) discovery();
}
void serialTick() {
  // Limit work per loop so a continuous serial sender cannot starve RX/scan.
  for (unsigned budget = 0; Serial.available() && budget < 512; ++budget) {
    const char c = Serial.read();
    if (c == '\n') {
      if (serialOverflow) result("parse", "command_too_long");
      else if (serialLine.length()) parseCommand(serialLine.c_str(), serialLine.length(), false);
      serialLine = ""; serialOverflow = false;
    } else if (c != '\r') {
      if (serialLine.length() >= 4096) serialOverflow = true;
      else if (!serialOverflow) serialLine += c;
    }
  }
}
void radioTick() {
  if (learning && millis() - learnStarted >= Settings::LearnTimeoutMs) {
    cancelLearn(); rf.stop(); result("learn", "learn_timeout"); state();
  }
  Remote received;
  bool found = rawLearning ? rf.rawReceived(received) : rf.received(received);
  if (found) {
    lastReceivedRecord = received;
    const bool duplicate = received.kind == RemoteKind::Rc && lastCode == received.code &&
      lastBits == received.bits && lastProtocol == received.protocol &&
      fabsf(lastRxFrequency - received.frequency) < 0.001f &&
      millis() - lastRxAt < Settings::EventDebounceMs;
    if (learning) {
      strlcpy(received.name, learnName, sizeof(received.name));
      const bool saved = store.save(learnSlot, received);
      cancelLearn();
      if (saved) {
        JsonDocument doc;
        RemoteStore::toJson(doc.to<JsonObject>(), received, learnSlot);
        doc["event"] = "learned"; emit(doc);
        notice = String("Gespeichert: ") + received.name;
        displayDirty = true;
        discoveryPending = true;
      } else result("learn", "storage_write_failed");
      state();
    } else if (!duplicate) {
      ++rxCount;
      JsonDocument doc;
      RemoteStore::toJson(doc.to<JsonObject>(), received, 0, false);
      doc.remove("slot"); doc["event"] = "received";
      doc["rssi_dbm"] = rf.rssi();
      for (unsigned slot = 0; slot < Settings::Slots; ++slot) {
        const Remote& known = store.at(slot);
        if (known.kind == RemoteKind::Rc && known.code == received.code &&
            known.bits == received.bits && known.protocol == received.protocol &&
            fabsf(known.frequency - received.frequency) < 0.001f) {
          doc["slot"] = slot; doc["name"] = known.name;
          publish(String("remote/") + slot + "/event", "press");
        }
      }
      emit(doc);
      notice = String("RX ") + received.code;
      displayDirty = true;
    }
    if (received.kind == RemoteKind::Rc) {
      lastCode = received.code; lastProtocol = received.protocol; lastBits = received.bits; lastRxAt = millis();
      lastRxFrequency = received.frequency;
    }
  }
  RadioGateway::Sample sample;
  if (rf.scanTick(sample)) {
    scanView.add(sample.frequency, sample.rssi);
    ++scanPoints;
    if (sample.rssi > scanPeak) { scanPeak = sample.rssi; scanPeakFrequency = sample.frequency; }
    JsonDocument doc;
    doc["event"] = "scan_sample"; doc["frequency_mhz"] = sample.frequency;
    doc["rssi_dbm"] = sample.rssi;
    emit(doc, "scan");
    if (sample.finished) {
      scanView.finished = true;
      scanView.failed = rf.mode() == RadioGateway::Mode::Error;
      JsonDocument summary;
      summary["event"] = "scan_finished"; summary["points"] = scanPoints;
      summary["peak_mhz"] = scanPeakFrequency; summary["peak_dbm"] = scanPeak;
      summary["ok"] = rf.mode() != RadioGateway::Mode::Error;
      emit(summary, "scan");
      notice = String("Scan: Peak ") + String(scanPeakFrequency, 2) + " MHz";
      displayDirty = true;
      state();
    }
  }
}
struct KeyEvents { bool click = false, held = false; };
class DebouncedKey {
public:
  KeyEvents update(bool down, uint32_t now) {
    KeyEvents events;
    if (down != raw_) { raw_ = down; changedAt_ = now; }
    if (raw_ != stable_ && now - changedAt_ >= 25) {
      stable_ = raw_;
      if (stable_) { pressedAt_ = now; holdReported_ = false; }
      else if (!holdReported_) events.click = true;
    }
    if (stable_ && !holdReported_ && now - pressedAt_ >= 800) {
      holdReported_ = true; events.held = true;
    }
    return events;
  }
private:
  bool raw_ = false, stable_ = false, holdReported_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0;
};
void inputTick() {
  static uint8_t previous = 0;
  static bool encoderInitialized = false;
  static int movement = 0;
  static DebouncedKey encoderKey, sideKey;
  static const int8_t transitions[] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
  const uint8_t current = (digitalRead(Pins::EncoderA) << 1) | digitalRead(Pins::EncoderB);
  if (encoderInitialized) movement += transitions[(previous << 2) | current];
  previous = current; encoderInitialized = true;
  const bool encoderDown = !digitalRead(Pins::EncoderKey);
  const KeyEvents encoder = encoderKey.update(encoderDown, millis());
  const KeyEvents side = sideKey.update(!digitalRead(Pins::UserKey), millis());
  const int delta = movement >= 4 ? 1 : movement <= -4 ? -1 : 0;
  if (delta) movement = 0;
  if (!features.screenOn()) {
    if (delta || encoder.click || encoder.held || side.click || side.held) {
      features.screenOn(true); displayDirty = true;
    }
    return;
  }
  if (delta) publish("input", delta > 0 ? "encoder_right" : "encoder_left");
  if (encoder.click) publish("input", "encoder_click");
  if (encoder.held) publish("input", "encoder_hold");
  if (side.click) publish("input", "side_click");
  if (side.held) publish("input", "side_hold");
  const bool busy = learning || features.ir.learning() || rf.mode() == RadioGateway::Mode::Scan;
  if (side.held || (busy && (side.click || encoder.click || encoder.held))) {
    JsonDocument doc; doc["action"] = "stop"; command(doc);
    if (side.held) displayPage = DisplayPage::Overview;
    displayDirty = true; movement = 0; return;
  }
  if (busy) return;
  if (side.click) {
    displayPage = DisplayPage((unsigned(displayPage) + 1) % unsigned(DisplayPage::Count));
    displayDirty = true;
    return;
  }
  if (delta && !encoderDown) {
    if (displayPage == DisplayPage::Codes || displayPage == DisplayPage::Learn)
      selectedSlot = (selectedSlot + Settings::Slots + delta) % Settings::Slots;
    else if (displayPage == DisplayPage::Scan) {
      scanPreset = (scanPreset + 3 + delta) % 3;
      previewScan(scanPreset);
    }
    else if (displayPage == DisplayPage::Infrared)
      selectedIr = (selectedIr + Settings::IrSlots + delta) % Settings::IrSlots;
    else if (displayPage == DisplayPage::Controls)
      selectedControl = (selectedControl + DisplayControlCount + delta) % DisplayControlCount;
    displayDirty = true;
  }
  if (!encoder.click && !encoder.held) return;
  JsonDocument doc;
  doc["slot"] = selectedSlot;
  switch (displayPage) {
    case DisplayPage::Overview:
      if (encoder.click) displayPage = DisplayPage::Codes;
      break;
    case DisplayPage::Codes:
      if (encoder.click) { doc["action"] = "send"; command(doc); }
      break;
    case DisplayPage::Learn:
      doc["action"] = "learn"; doc["kind"] = encoder.held ? "raw" : "rc";
      if (store.at(selectedSlot).name[0]) doc["name"] = store.at(selectedSlot).name;
      command(doc);
      break;
    case DisplayPage::Scan:
      if (encoder.click) { doc["action"] = "scan"; doc["band"] = scanBands[scanPreset]; command(doc); }
      break;
    case DisplayPage::Device:
      if (encoder.click) {
        board.tick(true);
        notice = board.readings().gaugeReachable ? "Akkuwerte aktualisiert / Temperatur im Chip" :
                                                 "Akku-IC antwortet nicht / Werte fehlen";
        state();
      }
      break;
    case DisplayPage::Network:
      if (encoder.click && connectivity.portalActive()) connectivity.closePortal(true);
      else if (!connectivity.portalActive()) {
        connectivity.openPortal();
        features.screenOn(true);
      }
      break;
    case DisplayPage::Infrared:
      doc["action"] = encoder.held ? "ir_learn" : "ir_send"; doc["slot"] = selectedIr;
      if (features.ir.at(selectedIr).name[0]) doc["name"] = features.ir.at(selectedIr).name;
      command(doc); break;
    case DisplayPage::Controls: {
      if (!encoder.click) break;
      const FeatureSnapshot f = features.snapshot(selectedIr);
      if (selectedControl == 0) doc["action"] = "beep";
      if (selectedControl == 1) { doc["action"] = "light_set"; doc["state"] = f.rgbOn ? "OFF" : "ON"; }
      if (selectedControl >= 2 && selectedControl <= 4) {
        doc["action"] = "feature_set";
        doc["feature"] = selectedControl == 2 ? "microphone" : selectedControl == 3 ? "nfc" : "nrf";
        doc["enabled"] = selectedControl == 2 ? !f.micEnabled : selectedControl == 3 ? !f.nfcEnabled : !f.nrfEnabled;
      }
      if (selectedControl == 5) doc["action"] = "backup";
      if (selectedControl == 6) doc["action"] = "probe_features";
      command(doc); break;
    }
    default: break;
  }
  displayDirty = true;
}
void displayTick() {
  if (!features.screenOn()) return;
  const bool scanning = rf.mode() == RadioGateway::Mode::Scan;
  const uint32_t interval = learning || scanning || features.ir.learning() ? 250 : 1000;
  const uint32_t elapsed = millis() - lastDisplayAt;
  if (elapsed < (displayDirty ? 100u : interval)) return;
  lastDisplayAt = millis(); displayDirty = false;
  DisplaySnapshot snapshot;
  snapshot.page = displayPage;
  snapshot.wifi = WiFi.status() == WL_CONNECTED; snapshot.mqtt = mqtt.connected();
  snapshot.storage = storeReady; snapshot.radioOk = rf.mode() != RadioGateway::Mode::Error;
  snapshot.frequency = rf.frequency(); snapshot.rssi = rf.rssi();
  snapshot.wifiRssi = snapshot.wifi ? WiFi.RSSI() : -127;
  snapshot.learning = learning; snapshot.rawLearning = rawLearning; snapshot.scanning = scanning;
  snapshot.slot = learning ? learnSlot : selectedSlot;
  snapshot.saved = store.count(); snapshot.scanPreset = scanPreset;
  const uint32_t passed = millis() - learnStarted;
  snapshot.secondsLeft = learning && passed < Settings::LearnTimeoutMs ?
    (Settings::LearnTimeoutMs - passed + 999) / 1000 : 0;
  snapshot.rxCount = rxCount; snapshot.txCount = txCount;
  snapshot.remote = &store.at(snapshot.slot);
  if (lastReceivedRecord.kind != RemoteKind::Empty) snapshot.lastReceived = &lastReceivedRecord;
  snapshot.scan = &scanView; snapshot.board = board.readings(); snapshot.notice = notice;
  snapshot.features = features.snapshot(selectedIr); snapshot.irSlot = selectedIr; snapshot.control = selectedControl;
  snapshot.setupActive = connectivity.portalActive();
  // Password is drawn only on the local setup page, never in a status object.
  if (snapshot.setupActive && displayPage == DisplayPage::Network) {
    snapshot.setupSsid = connectivity.portalSsid(); snapshot.setupPassword = connectivity.portalPassword();
  }
  snapshot.networkIp = snapshot.wifi ? WiFi.localIP().toString() : "";
  display.draw(snapshot);
}
void setup() {
  Serial.begin(115200);
  serialLine.reserve(4096);
  for (int pin : {Pins::EncoderA, Pins::EncoderB, Pins::EncoderKey, Pins::UserKey}) pinMode(pin, INPUT_PULLUP);
  const bool radioReady = rf.begin();
  board.begin(); board.tick();
  previewScan(scanPreset);
  storeReady = store.begin();
  features.begin(featureEvent);
  // Initialize the panel after the other shared SPI clients; GatewayDisplay sets its geometry.
  pinMode(Pins::Backlight, OUTPUT); digitalWrite(Pins::Backlight, LOW);
  screen.init(); display.begin(); screen.fillScreen(TFT_BLACK);
  digitalWrite(Pins::Backlight, HIGH);
  mqtt.setCallback(mqttMessage);
  mqtt.setBufferSize(6144); mqtt.setSocketTimeout(1); mqtt.setKeepAlive(30);
  connectivity.begin([]() {
    if (mqtt.connected()) publish("availability", "offline", true);
    mqtt.disconnect();
  });
  baseTopic = String("lilygo/") + connectivity.profile().deviceId;
  mqtt.setServer(connectivity.profile().mqttHost, connectivity.profile().mqttPort);
  if (connectivity.portalActive()) displayPage = DisplayPage::Network;
  notice = radioReady ? "Bereit - Code anlernen" : String("CC1101 Fehler: ") + rf.error();
  state();
}
void loop() {
  inputTick();
  serialTick();
  radioTick();
  features.tick(learning || rf.mode() == RadioGateway::Mode::Scan);
  if (features.takeChanged()) { displayDirty = true; featureStatePending = true; }
  const bool networkAllowed = !learning && !features.ir.learning() && rf.mode() != RadioGateway::Mode::Scan;
  connectivity.tick(networkAllowed, mqtt.connected(), mqtt.state());
  if (connectivity.takeChanged()) {
    baseTopic = String("lilygo/") + connectivity.profile().deviceId;
    mqtt.setServer(connectivity.profile().mqttHost, connectivity.profile().mqttPort);
    lastNetworkTry = millis() - 5000;
    displayDirty = true; featureStatePending = true;
    if (connectivity.portalActive()) {
      features.screenOn(true); displayPage = DisplayPage::Network;
      notice = "WLAN/MQTT im Browser einrichten";
    } else notice = WiFi.status() == WL_CONNECTED ? "WLAN verbunden" : "WLAN offline / USB und Funk weiter nutzbar";
  }
  // RF capture and scanning take priority over blocking TCP connection attempts.
  if (!learning && !features.ir.learning() && rf.mode() != RadioGateway::Mode::Scan) networkTick();
  else if (mqtt.connected()) mqtt.loop();
  if (!learning && !features.ir.learning() && rf.mode() != RadioGateway::Mode::Scan && board.tick()) displayDirty = true;
  if (millis() - lastStateAt >= 15000 || (featureStatePending && millis() - lastStateAt >= 1000)) state();
  displayTick();
  delay(1);
}
