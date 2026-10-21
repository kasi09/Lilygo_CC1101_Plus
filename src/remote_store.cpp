#include "remote_store.h"
#include "rf_rules.h"
#include "persistent_store.h"
#include <cstring>

bool RemoteStore::valid(const Remote& r) {
  if (r.magic != 0x52464731 || !memchr(r.name, '\0', sizeof(r.name)) ||
      !RfRules::receiveFrequency(r.frequency)) return false;
  if (r.kind == RemoteKind::Rc)
    return RfRules::rcCode(r.code, r.bits, r.protocol, r.pulseUs);
  if (r.kind != RemoteKind::Raw || r.count < 16 || r.count > Settings::RawPulses ||
      r.gapUs < 8000 || r.gapUs > 40000) return false;
  uint32_t total = r.gapUs;
  for (unsigned i = 0; i < r.count; ++i) {
    if (r.pulses[i] < 80 || r.pulses[i] > 40000) return false;
    total += r.pulses[i];
  }
  return total <= Settings::MaxTransmitUs;
}

bool RemoteStore::begin() {
  return RemoteStorage::begin(preferences_, "rf-remotes", remotes_, valid);
}
bool RemoteStore::save(unsigned slot, const Remote& r) {
  if (slot >= Settings::Slots || !valid(r)) return false;
  char key[8]; snprintf(key, sizeof(key), "s%u", slot);
  if (preferences_.putBytes(key, &r, sizeof(r)) != sizeof(r)) return false;
  remotes_[slot] = r;
  return true;
}
bool RemoteStore::erase(unsigned slot) {
  if (slot >= Settings::Slots) return false;
  char key[8]; snprintf(key, sizeof(key), "s%u", slot);
  if (preferences_.isKey(key) && !preferences_.remove(key)) return false;
  remotes_[slot] = Remote{};
  return true;
}
unsigned RemoteStore::count() const {
  unsigned count = 0;
  for (const auto& r : remotes_) if (r.kind != RemoteKind::Empty) ++count;
  return count;
}
void RemoteStore::toJson(JsonObject out, const Remote& r, unsigned slot, bool raw) {
  out["slot"] = slot;
  out["name"] = r.name;
  out["kind"] = r.kind == RemoteKind::Rc ? "rc" : r.kind == RemoteKind::Raw ? "raw" : "empty";
  if (r.kind == RemoteKind::Empty) return;
  out["frequency_mhz"] = r.frequency;
  if (r.kind == RemoteKind::Rc) {
    out["code"] = String(r.code);
    out["bits"] = r.bits;
    out["protocol"] = r.protocol;
    out["pulse_us"] = r.pulseUs;
  } else {
    out["first_high"] = r.firstHigh;
    out["gap_us"] = r.gapUs;
    out["pulse_count"] = r.count;
    if (raw) {
      JsonArray pulses = out["pulses_us"].to<JsonArray>();
      for (unsigned i = 0; i < r.count; ++i) pulses.add(r.pulses[i]);
    }
  }
}
