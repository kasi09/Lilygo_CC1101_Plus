#include "device_features.h"
#include "board_pins.h"
#include "feature_rules.h"
#include "nfc_frame.h"
#include <SD.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <cmath>
#include <cstring>

namespace {
String hexString(const uint8_t* data, size_t count) {
  String text; text.reserve(count * 2);
  constexpr char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < count; ++i) { text += hex[data[i] >> 4]; text += hex[data[i] & 15]; }
  return text;
}
bool slot(JsonDocument& doc, unsigned& out) {
  if (!doc["slot"].is<unsigned>()) return false;
  out = doc["slot"].as<unsigned>(); return out < Settings::IrSlots;
}
bool optionalUnsigned(JsonDocument& doc, const char* key, unsigned fallback, unsigned& out) {
  if (doc[key].isUnbound()) { out = fallback; return true; }
  if (!doc[key].is<unsigned>()) return false;
  out = doc[key]; return true;
}
}
DeviceFeatures::DeviceFeatures(RemoteStore& store) : rfStore_(store),
  nfc_(Pins::NfcIrq, Pins::NfcReset, &Wire), pixels_(Pins::RgbCount, Pins::Rgb, NEO_GRB + NEO_KHZ800),
  spi_(HSPI), nrf_(Pins::NrfCe, Pins::NrfCs) {}

bool DeviceFeatures::initNfc() {
  nfcArmed_ = nfcAckPending_ = false;
  if (!GW_ENABLE_NFC || !nfcEnabled_) return false;
  if (!nfc_.begin()) return false;
  if (!nfc_.getFirmwareVersion() || !nfc_.SAMConfig() || !nfc_.setPassiveActivationRetries(0xFF)) return false;
  // Adafruit's retry setter consumes only ACK; drain RFConfiguration's response
  // before sending InListPassiveTarget, otherwise it can be mistaken for its ACK.
  constexpr uint8_t expected[] = {1,0,0,0xFF,2,0xFE,0xD5,0x33,0xF8,0};
  uint8_t response[sizeof(expected)] = {};
  const size_t received = Wire.requestFrom(uint8_t(PN532_I2C_ADDRESS), sizeof(response), true);
  for (size_t i = 0; i < received && i < sizeof(response); ++i) response[i] = Wire.read();
  while (Wire.available()) Wire.read();
  if (received != sizeof(response) || memcmp(response, expected, sizeof(response)) != 0) return false;
  return true;
}
void DeviceFeatures::probe() {
  ir.stop();
  if (GW_ENABLE_NFC) nfcPresent_ = initNfc();
  if (GW_ENABLE_NRF) {
    if (nrfPresent_) nrf_.stopListening();
    nrfListening_ = false;
    nrfPresent_ = nrf_.begin(&spi_);
    if (nrfPresent_) {
      nrf_.setAutoAck(false); nrf_.setRetries(0, 0); nrf_.setPALevel(RF24_PA_LOW);
      nrf_.setDataRate(RF24_1MBPS); nrf_.setCRCLength(RF24_CRC_16);
      nrf_.setAddressWidth(5); nrf_.enableDynamicPayloads();
      nrf_.setChannel(nrfChannel_); nrf_.openReadingPipe(1, nrfAddress_);
      nrf_.powerDown();
    }
  }
  if (GW_ENABLE_SD) {
    if (sdMounted_) SD.end();
    sdMounted_ = SD.begin(Pins::SdCs, spi_, 4000000, "/sd", 5, false) && SD.cardType() != CARD_NONE;
    sdTotalMb_ = sdMounted_ ? SD.totalBytes() / (1024 * 1024) : 0;
    sdUsedMb_ = sdMounted_ ? SD.usedBytes() / (1024 * 1024) : 0;
  }
  changed_ = true;
}
void DeviceFeatures::begin(FeatureEvent event) {
  event_ = event;
  spi_.begin(Pins::Sck, Pins::Miso, Pins::Mosi);
  if (GW_ENABLE_RGB) { pixels_.begin(); pixels_.clear(); pixels_.show(); }
  ir.begin(event);
  probe();
}
void DeviceFeatures::screenOn(bool on) {
  screenOn_ = on; digitalWrite(Pins::Backlight, on ? HIGH : LOW); changed_ = true;
}
void DeviceFeatures::nfcTick(bool busy) {
  if (!GW_ENABLE_NFC || !nfcEnabled_) return;
  if (busy && !nfcPaused_) {
    digitalWrite(Pins::NfcReset, LOW); nfcPaused_ = true; nfcArmed_ = false;
    changed_ = true;
  }
  if (busy) return;
  if (nfcPaused_) { nfcPresent_ = initNfc(); nfcPaused_ = false; changed_ = true; }
  if (!nfcPresent_) return;
  auto fault = [&]() {
    nfcArmed_ = nfcAckPending_ = false; nfcPresent_ = false; changed_ = true;
    digitalWrite(Pins::NfcReset, LOW);
    JsonDocument doc; doc["event"] = "result"; doc["action"] = "nfc_receive";
    doc["ok"] = false; doc["error"] = "nfc_io_error";
    if (event_) event_(doc, "events");
  };
  if (nfcAckPending_ && millis() - nfcCommandAt_ > 250 && digitalRead(Pins::NfcIrq) != LOW) {
    fault(); return;
  }
  if (nfcArmed_ && digitalRead(Pins::NfcIrq) == LOW) {
    // Read only after IRQ: the library's start helper waits for a tag response
    // after ACK and repeatedly polls I2C, which stalls other devices on this board.
    uint8_t frame[64] = {};
    const size_t expected = nfcAckPending_ ? 7 : sizeof(frame);
    const size_t received = Wire.requestFrom(uint8_t(PN532_I2C_ADDRESS), expected, true);
    for (size_t i = 0; i < received && i < sizeof(frame); ++i) frame[i] = Wire.read();
    while (Wire.available()) Wire.read();
    if (received != expected) { fault(); return; }
    if (nfcAckPending_) {
      if (!NfcFrame::ack(frame, received)) { fault(); return; }
      nfcAckPending_ = false;
      return;
    }
    const int count = NfcFrame::uidLength(frame, received);
    nfcArmed_ = false;
    if (count < 0) { fault(); return; }
    if (count) {
      const String id = hexString(frame + 14, count);
      if (id != nfcUid_ || millis() - nfcLastSeen_ >= 1500) {
        nfcUid_ = id; ++nfcCount_; changed_ = true;
        JsonDocument doc; doc["event"] = "tag_scanned"; doc["uid"] = id;
        if (event_) event_(doc, "nfc/tag");
      }
      nfcLastSeen_ = millis();
    }
  }
  if (!nfcArmed_ && millis() - nfcLastArm_ >= 100) {
    nfcLastArm_ = millis();
    constexpr uint8_t command[] = {0,0,0xFF,4,0xFC,0xD4,0x4A,1,0,0xE1,0};
    Wire.beginTransmission(uint8_t(PN532_I2C_ADDRESS)); Wire.write(command, sizeof(command));
    if (Wire.endTransmission(true) != 0) { fault(); return; }
    nfcCommandAt_ = millis(); nfcArmed_ = nfcAckPending_ = true;
  }
}
void DeviceFeatures::nrfTick(bool busy) {
  if (!nrfPresent_) return;
  const bool listen = nrfEnabled_ && !busy;
  if (listen != nrfListening_) {
    if (listen) nrf_.startListening(); else { nrf_.stopListening(); nrf_.powerDown(); }
    nrfListening_ = listen;
  }
  if (!listen || !nrf_.available()) return;
  const uint8_t length = nrf_.getDynamicPayloadSize();
  if (!length || length > 32) { nrf_.flush_rx(); return; }
  uint8_t bytes[32]; nrf_.read(bytes, length);
  nrfPayload_ = hexString(bytes, length); ++nrfCount_; changed_ = true;
  if (nrfLastEvent_ && millis() - nrfLastEvent_ < 100) { ++nrfDropped_; return; }
  nrfLastEvent_ = millis();
  JsonDocument doc; doc["event"] = "nrf_received"; doc["payload_hex"] = nrfPayload_;
  doc["channel"] = nrfChannel_; doc["length"] = length;
  if (event_) event_(doc, "nrf/events");
}
bool DeviceFeatures::startMic() {
  if (!GW_ENABLE_MIC) return false;
  i2s_config_t config = {};
  config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  config.sample_rate = 16000; config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; config.communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1; config.dma_buf_count = 4; config.dma_buf_len = 256;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = pins.bck_io_num = pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.ws_io_num = Pins::MicClock; pins.data_in_num = Pins::MicData;
  if (i2s_driver_install(I2S_NUM_0, &config, 0, nullptr) != ESP_OK) return false;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK ||
      i2s_set_clk(I2S_NUM_0, 16000, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0); return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0); micInstalled_ = true; micPaused_ = false; noiseAt_ = 0;
  return true;
}
void DeviceFeatures::stopMic() {
  if (micInstalled_) i2s_driver_uninstall(I2S_NUM_0);
  micInstalled_ = micPaused_ = false; noiseAt_ = 0;
}
void DeviceFeatures::micTick(bool busy) {
  if (!micEnabled_ || !micInstalled_) return;
  if (busy != micPaused_) {
    if (busy) i2s_stop(I2S_NUM_0); else { i2s_zero_dma_buffer(I2S_NUM_0); i2s_start(I2S_NUM_0); }
    micPaused_ = busy; noiseAt_ = 0; changed_ = true;
  }
  if (busy || millis() - micLastRead_ < 1000) return;
  micLastRead_ = millis();
  int16_t samples[256]; size_t bytes = 0;
  if (i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytes, 0) != ESP_OK || bytes < 64) return;
  const size_t count = bytes / sizeof(samples[0]);
  double sum = 0, squares = 0;
  for (size_t i = 0; i < count; ++i) { sum += samples[i]; squares += double(samples[i]) * samples[i]; }
  const double mean = sum / count;
  const double rms = sqrt(max(0.0, squares / count - mean * mean));
  noiseDbfs_ = rms > 0 ? max(-96.0f, float(20 * log10(rms / 32768))) : -96;
  noiseAt_ = millis(); changed_ = true;
}
const char* DeviceFeatures::beep(unsigned hz, unsigned ms, unsigned volume) {
  if (!GW_ENABLE_AUDIO) return "speaker_disabled";
  if (!FeatureRules::tone(hz, ms, volume)) return "invalid_tone";
  i2s_config_t config = {};
  config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = 16000; config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT; config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.dma_buf_count = 4; config.dma_buf_len = 128; config.tx_desc_auto_clear = true;
  i2s_pin_config_t pins = {};
  pins.mck_io_num = pins.data_in_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = Pins::SpeakerBck; pins.ws_io_num = Pins::SpeakerWs; pins.data_out_num = Pins::SpeakerData;
  if (i2s_driver_install(I2S_NUM_1, &config, 0, nullptr) != ESP_OK) return "speaker_init_failed";
  if (i2s_set_pin(I2S_NUM_1, &pins) != ESP_OK) { i2s_driver_uninstall(I2S_NUM_1); return "speaker_init_failed"; }
  const unsigned total = 16000 * ms / 1000;
  unsigned phase = 0, done = 0;
  int16_t samples[256]; const int amplitude = 32767 * volume / 100;
  bool ok = true;
  while (done < total) {
    const unsigned chunk = min(128u, total - done);
    for (unsigned i = 0; i < chunk; ++i) {
      phase = (phase + hz) % 16000;
      samples[i * 2] = samples[i * 2 + 1] = phase < 8000 ? amplitude : -amplitude;
    }
    size_t written = 0;
    if (i2s_write(I2S_NUM_1, samples, chunk * 4, &written, pdMS_TO_TICKS(100)) != ESP_OK || written != chunk * 4) {
      ok = false; break;
    }
    done += chunk;
  }
  // Drain the final DMA buffers before releasing the output port.
  if (ok) delay(40);
  i2s_zero_dma_buffer(I2S_NUM_1); i2s_driver_uninstall(I2S_NUM_1);
  return ok ? nullptr : "speaker_write_failed";
}
void DeviceFeatures::updateRgb(bool connected) {
  if (!GW_ENABLE_RGB) return;
  const uint32_t color = !rgbOn_ ? 0 : rgbStatus_ ?
    pixels_.Color(connected ? 0 : 255, connected ? 180 : 90, connected ? 100 : 0) : pixels_.Color(red_, green_, blue_);
  static uint32_t previous = UINT32_MAX;
  static uint8_t oldBrightness = 0;
  if (color == previous && oldBrightness == brightness_) return;
  pixels_.setBrightness(brightness_); pixels_.fill(color); pixels_.show();
  previous = color; oldBrightness = brightness_;
}
const char* DeviceFeatures::setLight(JsonDocument& doc) {
  if (!GW_ENABLE_RGB) return "rgb_disabled";
  bool on = rgbOn_, status = rgbStatus_;
  uint8_t brightness = brightness_, red = red_, green = green_, blue = blue_;
  if (!doc["state"].isUnbound()) {
    const char* value = doc["state"] | "";
    if (strcmp(value, "ON") && strcmp(value, "OFF")) return "invalid_light_state";
    on = !strcmp(value, "ON");
  }
  if (!doc["brightness"].isUnbound()) {
    if (!doc["brightness"].is<uint8_t>()) return "invalid_brightness";
    brightness = doc["brightness"];
  }
  if (!doc["effect"].isUnbound()) {
    const char* value = doc["effect"] | "";
    if (strcmp(value, "Statisch") && strcmp(value, "Status")) return "invalid_light_effect";
    status = !strcmp(value, "Status");
  }
  if (!doc["color"].isUnbound()) {
    if (!doc["color"]["r"].is<uint8_t>() || !doc["color"]["g"].is<uint8_t>() || !doc["color"]["b"].is<uint8_t>())
      return "invalid_rgb";
    red = doc["color"]["r"]; green = doc["color"]["g"]; blue = doc["color"]["b"];
  }
  rgbOn_ = on; rgbStatus_ = status; brightness_ = brightness;
  red_ = red; green_ = green; blue_ = blue; changed_ = true;
  return nullptr;
}
void DeviceFeatures::lightState(JsonDocument& doc) const {
  doc["state"] = rgbOn_ ? "ON" : "OFF"; doc["brightness"] = brightness_;
  doc["color_mode"] = "rgb"; doc["effect"] = rgbStatus_ ? "Status" : "Statisch";
  doc["color"]["r"] = red_; doc["color"]["g"] = green_; doc["color"]["b"] = blue_;
}
const char* DeviceFeatures::configureNrf(JsonDocument& doc) {
  if (!nrfPresent_) return "nrf_unavailable";
  if (!doc["channel"].is<unsigned>() || !FeatureRules::nrfChannel(doc["channel"])) return "invalid_nrf_channel";
  uint8_t address[5]; size_t length;
  if (!FeatureRules::hexBytes(doc["address_hex"] | "", address, sizeof(address), length) || length != 5)
    return "invalid_nrf_address";
  nrf_.stopListening(); nrfListening_ = false;
  nrfChannel_ = doc["channel"]; memcpy(nrfAddress_, address, 5);
  nrf_.setChannel(nrfChannel_); nrf_.openReadingPipe(1, nrfAddress_); nrf_.flush_rx();
  nrf_.powerDown(); changed_ = true;
  return nullptr;
}
const char* DeviceFeatures::sendNrf(JsonDocument& doc) {
  if (!GW_TX_ENABLED) return "tx_disabled";
  if (!nrfPresent_ || !nrfEnabled_) return "nrf_disabled";
  uint8_t bytes[32]; size_t count;
  if (!FeatureRules::hexBytes(doc["payload_hex"] | "", bytes, sizeof(bytes), count)) return "invalid_nrf_payload";
  nrf_.powerUp(); nrf_.stopListening(); nrf_.openWritingPipe(nrfAddress_);
  // RF24::write can wait forever if the module stops responding.
  nrf_.startWrite(bytes, count, false);
  bool ok = false, failed = false, ready = false;
  const uint32_t started = millis();
  do { nrf_.whatHappened(ok, failed, ready); if (!ok && !failed) delay(1); }
  while (!ok && !failed && millis() - started < 100);
  nrf_.stopListening();
  if (!ok) nrf_.flush_tx();
  if (nrfListening_) nrf_.startListening(); else nrf_.powerDown();
  return ok ? nullptr : "nrf_send_failed";
}
const char* DeviceFeatures::scanNrf() {
  if (!nrfPresent_) return "nrf_unavailable";
  JsonDocument doc; doc["event"] = "nrf_energy_scan";
  JsonArray bins = doc["channels"].to<JsonArray>();
  for (unsigned channel = 0; channel <= 83; ++channel) {
    nrf_.stopListening(); nrf_.setChannel(channel); nrf_.startListening();
    delayMicroseconds(200);
    JsonObject bin = bins.add<JsonObject>(); bin["channel"] = channel; bin["energy"] = nrf_.testRPD();
  }
  nrf_.stopListening(); nrf_.setChannel(nrfChannel_); nrf_.flush_rx();
  if (nrfListening_) nrf_.startListening(); else nrf_.powerDown();
  if (event_) event_(doc, "nrf/scan");
  return nullptr;
}
const char* DeviceFeatures::backup() {
  if (!sdMounted_) return "sd_unavailable";
  if (!SD.exists("/lilygo") && !SD.mkdir("/lilygo")) return "sd_directory_failed";
  File file = SD.open("/lilygo/backup.tmp", FILE_WRITE);
  if (!file) return "sd_open_failed";
  bool ok = true;
  auto line = [&](JsonDocument& doc) {
    const size_t expected = measureJson(doc);
    if (serializeJson(doc, file) != expected || file.write(uint8_t('\n')) != 1) ok = false;
  };
  JsonDocument header; header["event"] = "backup"; header["version"] = Settings::Version; line(header);
  for (unsigned i = 0; i < Settings::Slots; ++i) if (rfStore_.at(i).kind != RemoteKind::Empty) {
    JsonDocument doc; RemoteStore::toJson(doc.to<JsonObject>(), rfStore_.at(i), i); doc["action"] = "store"; line(doc);
  }
  for (unsigned i = 0; i < Settings::IrSlots; ++i) if (ir.at(i).occupied) {
    JsonDocument doc; IrBridge::toJson(doc.to<JsonObject>(), ir.at(i), i); doc["action"] = "ir_store"; line(doc);
  }
  file.flush(); if (file.getWriteError()) ok = false; file.close();
  if (!ok) { SD.remove("/lilygo/backup.tmp"); return "sd_write_failed"; }
  if (SD.exists("/lilygo/backup.jsonl")) {
    if (SD.exists("/lilygo/backup.prev.jsonl") && !SD.remove("/lilygo/backup.prev.jsonl")) return "sd_rotate_failed";
    if (!SD.rename("/lilygo/backup.jsonl", "/lilygo/backup.prev.jsonl")) return "sd_rotate_failed";
  }
  if (!SD.rename("/lilygo/backup.tmp", "/lilygo/backup.jsonl")) {
    if (SD.exists("/lilygo/backup.prev.jsonl")) SD.rename("/lilygo/backup.prev.jsonl", "/lilygo/backup.jsonl");
    return "sd_commit_failed";
  }
  sdUsedMb_ = SD.usedBytes() / (1024 * 1024); changed_ = true;
  return nullptr;
}
bool DeviceFeatures::command(JsonDocument& doc, const char*& error, bool radioBusy) {
  const char* action = doc["action"] | "";
  const bool recognized = !strncmp(action, "ir_", 3) || !strncmp(action, "nrf_", 4) ||
    !strcmp(action, "feature_set") || !strcmp(action, "screen_set") || !strcmp(action, "light_set") ||
    !strcmp(action, "beep") || !strcmp(action, "backup") || !strcmp(action, "probe_features");
  if (!recognized) return false;
  error = nullptr;
  if (!strcmp(action, "ir_list")) {
    for (unsigned i = 0; i < Settings::IrSlots; ++i) {
      JsonDocument out; IrBridge::toJson(out.to<JsonObject>(), ir.at(i), i);
      out["event"] = "ir_remote"; if (event_) event_(out, "ir/remotes");
    }
    return true;
  }
  if (radioBusy || ir.learning()) { error = "device_busy"; return true; }
  unsigned selected = 0;
  if (!strcmp(action, "ir_learn") || !strcmp(action, "ir_send") || !strcmp(action, "ir_store") || !strcmp(action, "ir_delete")) {
    if (!slot(doc, selected)) error = "invalid_ir_slot";
    else if (!strcmp(action, "ir_learn")) {
      const char* name = doc["name"] | "";
      if (!doc["name"].isUnbound() && !doc["name"].is<const char*>()) error = "invalid_name";
      else error = ir.learn(selected, name);
    } else if (!strcmp(action, "ir_send")) {
      unsigned repeats;
      error = optionalUnsigned(doc, "repeats", 0, repeats) ? ir.send(selected, repeats) : "invalid_repeats";
    } else if (!strcmp(action, "ir_delete")) error = ir.erase(selected);
    else { IrRemote remote; error = IrBridge::fromJson(doc, remote) ? ir.save(selected, remote) : "invalid_ir_remote"; }
  } else if (!strcmp(action, "screen_set")) {
    if (!doc["enabled"].is<bool>()) error = "invalid_enabled"; else screenOn(doc["enabled"]);
  } else if (!strcmp(action, "light_set")) error = setLight(doc);
  else if (!strcmp(action, "feature_set")) {
    const char* name = doc["feature"] | "";
    if (!doc["enabled"].is<bool>()) error = "invalid_enabled";
    else {
      const bool enabled = doc["enabled"];
      if (!strcmp(name, "nfc")) {
        if (!GW_ENABLE_NFC) error = "nfc_disabled";
        else {
          nfcEnabled_ = enabled;
          if (enabled) nfcPresent_ = initNfc(); else { digitalWrite(Pins::NfcReset, LOW); nfcArmed_ = false; }
          nfcPaused_ = false;
          if (enabled && !nfcPresent_) error = "nfc_unavailable";
        }
      } else if (!strcmp(name, "ir")) {
        if (!ir.available()) error = "ir_unavailable"; else ir.enabled(enabled);
      } else if (!strcmp(name, "nrf")) {
        if (!nrfPresent_) error = "nrf_unavailable"; else nrfEnabled_ = enabled;
      } else if (!strcmp(name, "microphone")) {
        if (enabled && !micInstalled_ && !startMic()) error = "microphone_init_failed";
        else { micEnabled_ = enabled; if (!enabled) stopMic(); }
      } else error = "unknown_feature";
    }
    changed_ = true;
  } else if (!strcmp(action, "beep")) {
    unsigned hz, ms, volume;
    if (!optionalUnsigned(doc, "frequency_hz", 880, hz) || !optionalUnsigned(doc, "duration_ms", 150, ms) ||
        !optionalUnsigned(doc, "volume", 20, volume)) error = "invalid_tone";
    else error = beep(hz, ms, volume);
  } else if (!strcmp(action, "nrf_config")) error = configureNrf(doc);
  else if (!strcmp(action, "nrf_send")) error = sendNrf(doc);
  else if (!strcmp(action, "nrf_scan")) error = scanNrf();
  else if (!strcmp(action, "backup")) error = backup();
  else if (!strcmp(action, "probe_features")) probe();
  else error = "unknown_action";
  changed_ = true;
  return true;
}
void DeviceFeatures::tick(bool radioBusy) {
  ir.tick(!radioBusy);
  nfcTick(radioBusy || ir.learning());
  nrfTick(radioBusy || ir.learning());
  micTick(radioBusy || ir.learning());
  if (!radioBusy && !ir.learning()) updateRgb(WiFi.status() == WL_CONNECTED);
  if (ir.takeChanged()) changed_ = true;
}
bool DeviceFeatures::takeChanged() { const bool value = changed_; changed_ = false; return value; }
FeatureSnapshot DeviceFeatures::snapshot(unsigned selected) const {
  FeatureSnapshot out;
  out.nfcAvailable = nfcPresent_ && !nfcPaused_; out.nfcEnabled = nfcEnabled_ && GW_ENABLE_NFC;
  out.irAvailable = ir.available(); out.irEnabled = ir.enabled(); out.irLearning = ir.learning();
  out.irCount = ir.receivedCount(); out.irCode = ir.lastCode(); out.irLearnSlot = ir.learnSlot(); out.irSecondsLeft = ir.secondsLeft();
  if (selected < Settings::IrSlots) { out.irSlotName = ir.at(selected).name; out.irSlotOccupied = ir.at(selected).occupied; }
  out.nfcCount = nfcCount_; out.nfcUid = nfcUid_;
  out.nrfAvailable = nrfPresent_; out.nrfEnabled = nrfEnabled_; out.nrfCount = nrfCount_; out.nrfPayload = nrfPayload_;
  out.micEnabled = micEnabled_; out.noiseAvailable = micInstalled_ && !micPaused_ && noiseAt_ && millis() - noiseAt_ <= 3000;
  out.noiseDbfs = noiseDbfs_; out.sdMounted = sdMounted_; out.screenOn = screenOn_; out.rgbOn = rgbOn_;
  return out;
}
void DeviceFeatures::appendState(JsonDocument& doc) const {
  const FeatureSnapshot s = snapshot(0);
  doc["nfc_available"] = s.nfcAvailable; doc["nfc_enabled"] = s.nfcEnabled; doc["nfc_uid"] = nfcUid_;
  doc["nfc_count"] = nfcCount_; doc["ir_available"] = ir.available(); doc["ir_enabled"] = ir.enabled();
  doc["ir_last_code"] = ir.lastCode(); doc["ir_count"] = ir.receivedCount();
  doc["nrf_available"] = nrfPresent_; doc["nrf_enabled"] = nrfEnabled_; doc["nrf_count"] = nrfCount_;
  doc["nrf_event_dropped"] = nrfDropped_; doc["nrf_last_payload"] = nrfPayload_; doc["nrf_channel"] = nrfChannel_;
  doc["nrf_address_hex"] = hexString(nrfAddress_, 5);
  doc["microphone_enabled"] = micEnabled_; doc["noise_available"] = s.noiseAvailable;
  if (s.noiseAvailable) doc["noise_dbfs"] = noiseDbfs_; else doc["noise_dbfs"] = nullptr;
  doc["screen_on"] = screenOn_; doc["rgb_on"] = rgbOn_; doc["sd_mounted"] = sdMounted_;
  if (sdMounted_) { doc["sd_total_mb"] = sdTotalMb_; doc["sd_used_mb"] = sdUsedMb_; }
  else { doc["sd_total_mb"] = nullptr; doc["sd_used_mb"] = nullptr; }
}
