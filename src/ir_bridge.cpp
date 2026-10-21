#include "ir_bridge.h"
#include "board_pins.h"
#include "feature_rules.h"
#include "persistent_store.h"
#include <IRutils.h>
#include <cstring>

namespace {
String codeHex(uint64_t code) {
  char value[19]; snprintf(value, sizeof(value), "0x%016llX", (unsigned long long)code);
  return value;
}
uint32_t rawDuration(const IrRemote& r) {
  uint32_t total = 0;
  for (unsigned i = 0; i < r.count; ++i) total += r.pulses[i];
  return total;
}
bool sameRaw(const IrRemote& a, const IrRemote& b) {
  if (a.count != b.count || !a.count) return false;
  for (unsigned i = 0; i < a.count; ++i) {
    const int tolerance = max(100, int(a.pulses[i]) / 4);
    if (abs(int(a.pulses[i]) - int(b.pulses[i])) > tolerance) return false;
  }
  return true;
}
}
IrBridge::IrBridge() : receiver_(Pins::IrRx, 1024, 50, true), sender_(Pins::IrTx) {}
bool IrBridge::begin(FeatureEvent event) {
  event_ = event;
  if (!GW_ENABLE_IR) return false;
  sender_.begin();
  if (!RemoteStorage::begin(preferences_, "ir-remotes", remotes_, valid)) return false;
  available_ = true;
  receiver_.enableIRIn(); receiverRunning_ = true;
  return true;
}
bool IrBridge::valid(const IrRemote& r) {
  if (r.magic != 0x49524731 || !r.occupied || !memchr(r.name, '\0', sizeof(r.name)) ||
      r.carrierKhz < 30 || r.carrierKhz > 60 || r.count > Settings::IrRawPulses) return false;
  if (r.decoded && (r.protocol <= 0 || r.protocol > kLastDecodeType || r.bits < 1 || r.bits > 64 ||
                    hasACState(decode_type_t(r.protocol)))) return false;
  if (r.decoded && r.bits < 64 && (r.code >> r.bits) != 0) return false;
  if (!r.decoded && r.count < 8) return false;
  for (unsigned i = 0; i < r.count; ++i) if (r.pulses[i] < 20) return false;
  return rawDuration(r) <= 500000;
}
const char* IrBridge::save(unsigned slot, const IrRemote& r) {
  if (slot >= Settings::IrSlots || !valid(r)) return "invalid_ir_remote";
  char key[8]; snprintf(key, sizeof(key), "s%u", slot);
  if (preferences_.putBytes(key, &r, sizeof(r)) != sizeof(r)) return "storage_write_failed";
  remotes_[slot] = r; changed_ = true;
  return nullptr;
}
const char* IrBridge::erase(unsigned slot) {
  if (slot >= Settings::IrSlots) return "invalid_ir_slot";
  char key[8]; snprintf(key, sizeof(key), "s%u", slot);
  if (preferences_.isKey(key) && !preferences_.remove(key)) return "storage_write_failed";
  remotes_[slot] = IrRemote{}; changed_ = true;
  return nullptr;
}
const char* IrBridge::learn(unsigned slot, const char* name) {
  if (!available_ || !enabled_) return "ir_disabled";
  if (slot >= Settings::IrSlots || !name || strlen(name) > 32) return "invalid_ir_slot";
  learnSlot_ = slot; strlcpy(learnName_, name, sizeof(learnName_));
  if (!learnName_[0]) snprintf(learnName_, sizeof(learnName_), "IR %u", slot);
  learnStarted_ = millis(); learning_ = true; changed_ = true;
  receiver_.resume();
  return nullptr;
}
void IrBridge::stop() { learning_ = false; changed_ = true; }
unsigned IrBridge::secondsLeft() const {
  const uint32_t elapsed = millis() - learnStarted_;
  return learning_ && elapsed < Settings::LearnTimeoutMs ? (Settings::LearnTimeoutMs - elapsed + 999) / 1000 : 0;
}
bool IrBridge::takeChanged() { const bool value = changed_; changed_ = false; return value; }
void IrBridge::tick(bool allowReceive) {
  const bool run = available_ && enabled_ && allowReceive;
  if (run != receiverRunning_) {
    if (run) receiver_.enableIRIn(); else receiver_.disableIRIn();
    receiverRunning_ = run;
  }
  if (learning_ && !secondsLeft()) {
    stop(); JsonDocument doc; doc["event"] = "result"; doc["action"] = "ir_learn";
    doc["ok"] = false; doc["error"] = "learn_timeout"; if (event_) event_(doc, "events");
  }
  if (!run) return;
  decode_results result;
  if (!receiver_.decode(&result)) return;
  IrRemote captured;
  captured.occupied = true;
  captured.decoded = result.decode_type != UNKNOWN && !hasACState(result.decode_type) &&
                     result.bits > 0 && result.bits <= 64;
  captured.protocol = result.decode_type; captured.bits = result.bits;
  if (captured.decoded) captured.code = result.value;
  bool rawValid = !result.overflow && result.rawlen > 1 && result.rawlen - 1 <= Settings::IrRawPulses;
  if (rawValid) for (unsigned i = 1; i < result.rawlen; ++i) {
    const uint32_t duration = uint32_t(result.rawbuf[i]) * kRawTick;
    if (duration < 20 || duration > UINT16_MAX) { rawValid = false; break; }
    captured.pulses[captured.count++] = duration;
  }
  if (!rawValid) captured.count = 0;
  const bool repeat = result.repeat;
  receiver_.resume();
  if (repeat) return;
  uint32_t rawHash = 2166136261u;
  for (unsigned i = 0; i < captured.count; ++i) rawHash = (rawHash ^ (captured.pulses[i] / 100)) * 16777619u;
  String identity = captured.decoded ? typeToString(decode_type_t(captured.protocol)) + ":" + codeHex(captured.code) :
    String("RAW:") + String(rawHash, HEX);
  if (!captured.decoded) for (unsigned i = 0; i < Settings::IrSlots; ++i) {
    if (remotes_[i].occupied && !remotes_[i].decoded && sameRaw(remotes_[i], captured)) {
      // Use a stable identity so normal timing jitter cannot bypass debounce.
      identity = String("RAW:slot_") + i; break;
    }
  }
  if (!learning_ && identity == lastCode_ && millis() - lastSeen_ < 350) { lastSeen_ = millis(); return; }
  lastCode_ = identity; lastSeen_ = millis(); ++receivedCount_;
  if (learning_) {
    strlcpy(captured.name, learnName_, sizeof(captured.name));
    const char* error = valid(captured) ? save(learnSlot_, captured) : "ir_frame_too_long_or_invalid";
    if (error) {
      JsonDocument doc; doc["event"] = "result"; doc["action"] = "ir_learn";
      doc["ok"] = false; doc["error"] = error; if (event_) event_(doc, "events");
      return;
    }
    learning_ = false;
    JsonDocument doc; toJson(doc.to<JsonObject>(), captured, learnSlot_);
    doc["event"] = "ir_learned"; if (event_) event_(doc, "events");
  } else {
    JsonDocument doc; doc["event"] = "ir_received"; doc["code"] = identity;
    doc["bits"] = captured.bits;
    if (event_) event_(doc, "ir/events");
    for (unsigned i = 0; i < Settings::IrSlots; ++i) {
      const IrRemote& saved = remotes_[i];
      if (!saved.occupied) continue;
      if ((saved.decoded && captured.decoded && saved.protocol == captured.protocol &&
           saved.bits == captured.bits && saved.code == captured.code) ||
          (!saved.decoded && sameRaw(saved, captured))) {
        JsonDocument match; match["event"] = "press"; match["slot"] = i;
        match["name"] = saved.name;
        if (event_) event_(match, "ir/trigger");
      }
    }
  }
  changed_ = true;
}
const char* IrBridge::send(unsigned slot, unsigned repeats) {
  if (!GW_TX_ENABLED) return "tx_disabled";
  if (!available_) return "ir_unavailable";
  if (slot >= Settings::IrSlots || !valid(remotes_[slot])) return "invalid_ir_remote";
  if (repeats > 3) return "invalid_repeats";
  if (lastTransmit_ && millis() - lastTransmit_ < 500) return "tx_cooldown";
  const IrRemote& r = remotes_[slot];
  const uint32_t estimate = r.count ? rawDuration(r) + 20000 : 20000 + r.bits * 3000;
  if (uint64_t(estimate) * (repeats + 1) > 500000) return "tx_airtime_exceeded";
  if (receiverRunning_) receiver_.disableIRIn();
  bool sent = r.decoded && sender_.send(decode_type_t(r.protocol), r.code, r.bits, repeats);
  if (!sent && r.count) {
    for (unsigned i = 0; i <= repeats; ++i) { sender_.sendRaw(r.pulses, r.count, r.carrierKhz); delay(20); }
    sent = true;
  }
  if (receiverRunning_) receiver_.enableIRIn();
  lastTransmit_ = millis();
  return sent ? nullptr : "ir_protocol_not_supported_for_send";
}
void IrBridge::toJson(JsonObject out, const IrRemote& r, unsigned slot) {
  out["slot"] = slot; out["occupied"] = r.occupied; out["name"] = r.name;
  if (!r.occupied) return;
  out["decoded"] = r.decoded; out["protocol"] = r.protocol;
  out["code_hex"] = codeHex(r.code); out["bits"] = r.bits; out["carrier_khz"] = r.carrierKhz;
  JsonArray pulses = out["pulses_us"].to<JsonArray>();
  for (unsigned i = 0; i < r.count; ++i) pulses.add(r.pulses[i]);
}
bool IrBridge::fromJson(JsonDocument& doc, IrRemote& r) {
  if (!doc["decoded"].is<bool>() || !doc["name"].is<const char*>() ||
      strlen(doc["name"].as<const char*>()) > 32 || !doc["carrier_khz"].is<uint16_t>()) return false;
  r = IrRemote{}; r.occupied = true; r.decoded = doc["decoded"];
  strlcpy(r.name, doc["name"], sizeof(r.name)); r.carrierKhz = doc["carrier_khz"];
  if (r.decoded) {
    if (!doc["protocol"].is<int16_t>() || !doc["bits"].is<uint16_t>() ||
        !FeatureRules::hex64(doc["code_hex"] | "", r.code)) return false;
    r.protocol = doc["protocol"]; r.bits = doc["bits"];
  }
  if (!doc["pulses_us"].isUnbound() && !doc["pulses_us"].is<JsonArrayConst>()) return false;
  JsonArrayConst pulses = doc["pulses_us"].as<JsonArrayConst>();
  if (!pulses.isNull()) for (JsonVariantConst pulse : pulses) {
    if (!pulse.is<uint16_t>() || r.count >= Settings::IrRawPulses) return false;
    r.pulses[r.count++] = pulse.as<uint16_t>();
  }
  return valid(r);
}
