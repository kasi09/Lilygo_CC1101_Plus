#include "radio_gateway.h"
#include "rf_rules.h"
#include <cstring>

namespace {
constexpr uint32_t GapUs = 8000;
portMUX_TYPE rawMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t edgeAt = 0;
volatile unsigned rawCount = 0;
volatile bool rawReady = false, rawCollecting = false, rawFirstHigh = true;
volatile uint16_t rawGap = 10000;
volatile uint16_t rawPulses[Settings::RawPulses];

void IRAM_ATTR rawEdge() {
  const uint32_t now = micros();
  const bool high = digitalRead(Pins::Gdo0);
  portENTER_CRITICAL_ISR(&rawMux);
  const uint32_t duration = now - edgeAt;
  edgeAt = now;
  if (!rawReady) {
    if (duration >= GapUs) {
      if (rawCollecting && rawCount >= 16 && duration <= 40000) {
        rawGap = duration;
        rawReady = true;
      } else {
        rawCount = 0;
        rawFirstHigh = high;
        rawCollecting = true;
      }
    } else if (rawCollecting) {
      if (duration < 80 || rawCount >= Settings::RawPulses) {
        rawCount = 0;
        rawCollecting = false;
      } else {
        rawPulses[rawCount++] = duration;
      }
    }
  }
  portEXIT_CRITICAL_ISR(&rawMux);
}
}

RadioGateway::RadioGateway() : spi_(HSPI),
  module_(Pins::RadioCs, Pins::Gdo0, RADIOLIB_NC, Pins::Gdo2, spi_), radio_(&module_) {}

void RadioGateway::antenna(float mhz) {
  // Explicit ranges: do not copy the upstream signed-difference comparisons.
  digitalWrite(Pins::AntennaSw1, mhz < 779 ? HIGH : LOW);
  digitalWrite(Pins::AntennaSw0, mhz <= 348 ? LOW : HIGH);
}
bool RadioGateway::check(int16_t state) {
  error_ = state;
  if (state == RADIOLIB_ERR_NONE) return true;
  detach();
  mode_ = Mode::Error;
  return false;
}
void RadioGateway::detach() {
  if (decoderActive_) decoder_.disableReceive();
  decoderActive_ = false;
  detachInterrupt(digitalPinToInterrupt(Pins::Gdo0));
}
bool RadioGateway::begin() {
  pinMode(Pins::Power, OUTPUT); digitalWrite(Pins::Power, HIGH);
  for (int cs : {Pins::DisplayCs, Pins::SdCs, Pins::RadioCs, Pins::NrfCs}) {
    pinMode(cs, OUTPUT); digitalWrite(cs, HIGH);
  }
  pinMode(Pins::NrfCe, OUTPUT); digitalWrite(Pins::NrfCe, LOW);
  pinMode(Pins::AntennaSw1, OUTPUT); pinMode(Pins::AntennaSw0, OUTPUT);
  antenna(frequency_);
  delay(20);
  spi_.begin(Pins::Sck, Pins::Miso, Pins::Mosi);
  if (!check(radio_.begin(frequency_, 4.8f, 5.0f, 270.0f, GW_TX_POWER_DBM))) return false;
  initialized_ = true;
  if (!check(radio_.setOOK(true))) return false;
  return receive();
}
bool RadioGateway::receive() {
  detach();
  pinMode(Pins::Gdo0, INPUT);
  if (!check(radio_.receiveDirectAsync())) return false;
  decoder_.resetAvailable();
  decoder_.enableReceive(digitalPinToInterrupt(Pins::Gdo0));
  decoderActive_ = true;
  mode_ = Mode::Receive;
  return true;
}
bool RadioGateway::tune(float mhz) {
  if (!initialized_ || !RfRules::receiveFrequency(mhz)) return false;
  detach();
  antenna(mhz);
  if (!check(radio_.setFrequency(mhz))) return false;
  frequency_ = mhz;
  return receive();
}
bool RadioGateway::received(Remote& r) {
  if (mode_ != Mode::Receive || !decoder_.available()) return false;
  // Stop the ISR while copying its multi-field result.
  decoder_.disableReceive();
  decoderActive_ = false;
  r = Remote{};
  r.kind = RemoteKind::Rc;
  r.frequency = frequency_;
  r.code = decoder_.getReceivedValue();
  r.bits = decoder_.getReceivedBitlength();
  r.protocol = decoder_.getReceivedProtocol();
  r.pulseUs = decoder_.getReceivedDelay();
  decoder_.resetAvailable();
  decoder_.enableReceive(digitalPinToInterrupt(Pins::Gdo0));
  decoderActive_ = true;
  return RemoteStore::valid(r);
}
bool RadioGateway::startRaw() {
  if (!stop()) return false;
  detach();
  portENTER_CRITICAL(&rawMux);
  edgeAt = micros(); rawCount = 0; rawReady = false; rawCollecting = false;
  portEXIT_CRITICAL(&rawMux);
  attachInterrupt(digitalPinToInterrupt(Pins::Gdo0), rawEdge, CHANGE);
  mode_ = Mode::RawLearn;
  return true;
}
bool RadioGateway::rawReceived(Remote& r) {
  if (mode_ != Mode::RawLearn) return false;
  // A one-shot transmitter may never produce another edge after its last
  // pulse. Finish that frame on silence instead of waiting for a second press.
  portENTER_CRITICAL(&rawMux);
  if (!rawReady && rawCollecting && rawCount >= 16 && micros() - edgeAt >= GapUs) {
    rawGap = GapUs;
    rawReady = true;
  }
  const bool ready = rawReady;
  portEXIT_CRITICAL(&rawMux);
  if (!ready) return false;
  detach();
  r = Remote{};
  r.kind = RemoteKind::Raw;
  r.frequency = frequency_;
  portENTER_CRITICAL(&rawMux);
  r.count = rawCount; r.firstHigh = rawFirstHigh; r.gapUs = rawGap;
  for (unsigned i = 0; i < r.count; ++i) r.pulses[i] = rawPulses[i];
  rawReady = false;
  portEXIT_CRITICAL(&rawMux);
  if (!receive()) return false;
  if (RemoteStore::valid(r)) return true;
  startRaw();
  return false;
}
bool RadioGateway::scan(float start, float end, float step) {
  if (!initialized_ || !RfRules::scanRange(start, end, step)) return false;
  detach();
  scanStart_ = start; scanEnd_ = end; scanStep_ = step; scanIndex_ = 0;
  allBands_ = false; scanBand_ = 0;
  antenna(start);
  if (!check(radio_.setFrequency(start)) || !check(radio_.receiveDirectAsync())) return false;
  mode_ = Mode::Scan;
  sampleDue_ = millis() + Settings::ScanDwellMs;
  return true;
}
bool RadioGateway::scan(const char* band) {
  if (!strcmp(band, "433")) return scan(433.05f, 434.79f, 0.05f);
  if (!strcmp(band, "868")) return scan(863.0f, 870.0f, 0.1f);
  if (strcmp(band, "all")) return false;
  if (!scan(300, 348, 0.5f)) return false;
  allBands_ = true;
  return true;
}
bool RadioGateway::scanTick(Sample& sample) {
  if (mode_ != Mode::Scan || int32_t(millis() - sampleDue_) < 0) return false;
  sample = {scanStart_ + scanIndex_ * scanStep_, radio_.getRSSI(), false};
  ++scanIndex_;
  float next = scanStart_ + scanIndex_ * scanStep_;
  if (next > scanEnd_ + 0.0001f) {
    if (allBands_ && scanBand_ < 2) {
      ++scanBand_;
      scanStart_ = scanBand_ == 1 ? 387 : 779;
      scanEnd_ = scanBand_ == 1 ? 464 : 928;
      scanIndex_ = 0;
      next = scanStart_;
    } else {
      sample.finished = true;
      tune(frequency_);
      return true;
    }
  }
  antenna(next);
  if (!check(radio_.setFrequency(next)) || !check(radio_.receiveDirectAsync())) {
    sample.finished = true;
    return true;
  }
  sampleDue_ = millis() + Settings::ScanDwellMs;
  return true;
}
bool RadioGateway::stop() { return tune(frequency_); }
float RadioGateway::rssi() { return initialized_ && mode_ != Mode::Error ? radio_.getRSSI() : -127; }
const char* RadioGateway::modeName() const {
  switch (mode_) {
    case Mode::Receive: return "receive";
    case Mode::Scan: return "scan";
    case Mode::RawLearn: return "learn_raw";
    default: return "error";
  }
}
const char* RadioGateway::send(const Remote& r, unsigned repeats) {
  if (!GW_TX_ENABLED) return "tx_disabled";
  if (!initialized_ || mode_ == Mode::Error) return "radio_error";
  if (!RemoteStore::valid(r)) return "invalid_remote";
  if (!RfRules::transmitFrequency(r.frequency)) return "tx_frequency_not_allowed";
  if (repeats < 1 || repeats > 10) return "invalid_repeats";
  if (hasTransmitted_ && millis() - lastTransmit_ < Settings::TransmitCooldownMs)
    return "tx_cooldown";
  uint64_t airtime = r.gapUs;
  for (unsigned i = 0; i < r.count && r.kind == RemoteKind::Raw; ++i) airtime += r.pulses[i];
  airtime = r.kind == RemoteKind::Rc ?
    RfRules::rcAirtimeUs(r.bits, r.protocol, r.pulseUs, repeats) : airtime * repeats;
  if (airtime > Settings::MaxTransmitUs) return "tx_airtime_exceeded";
  detach();
  antenna(r.frequency);
  if (!check(radio_.setFrequency(r.frequency))) return "radio_error";
  // Set data LOW before STX, avoiding an unmodulated carrier during GPIO setup.
  pinMode(Pins::Gdo0, OUTPUT); digitalWrite(Pins::Gdo0, LOW);
  if (!check(radio_.transmitDirectAsync())) {
    radio_.standby(); pinMode(Pins::Gdo0, INPUT); return "radio_error";
  }
  if (r.kind == RemoteKind::Rc) {
    decoder_.enableTransmit(Pins::Gdo0);
    decoder_.setProtocol(r.protocol, r.pulseUs);
    decoder_.setRepeatTransmit(repeats);
    decoder_.send(r.code, r.bits);
    decoder_.disableTransmit();
  } else {
    for (unsigned repeat = 0; repeat < repeats; ++repeat) {
      bool high = r.firstHigh;
      for (unsigned i = 0; i < r.count; ++i) {
        digitalWrite(Pins::Gdo0, high ? HIGH : LOW);
        delayMicroseconds(r.pulses[i]);
        high = !high;
      }
      // The recorded separator is the level following the last stored pulse.
      digitalWrite(Pins::Gdo0, high ? HIGH : LOW);
      delayMicroseconds(r.gapUs);
    }
  }
  digitalWrite(Pins::Gdo0, LOW);
  radio_.standby();
  lastTransmit_ = millis(); hasTransmitted_ = true;
  return tune(frequency_) ? nullptr : "radio_restore_failed";
}
