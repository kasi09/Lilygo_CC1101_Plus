#pragma once
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_PN532.h>
#include <RF24.h>
#include <SPI.h>
#include "ir_bridge.h"
#include "remote_store.h"

struct FeatureSnapshot {
  bool nfcAvailable = false, nfcEnabled = false, irAvailable = false, irEnabled = false;
  bool nrfAvailable = false, nrfEnabled = false, micEnabled = false, noiseAvailable = false;
  bool sdMounted = false, screenOn = true, rgbOn = false;
  bool irLearning = false, irSlotOccupied = false;
  unsigned irSecondsLeft = 0, irLearnSlot = 0;
  uint32_t nfcCount = 0, irCount = 0, nrfCount = 0;
  float noiseDbfs = -96;
  String nfcUid, irCode, nrfPayload, irSlotName;
};

class DeviceFeatures {
public:
  explicit DeviceFeatures(RemoteStore& rfStore);
  void begin(FeatureEvent event);
  void tick(bool radioBusy);
  void appendState(JsonDocument& doc) const;
  void lightState(JsonDocument& doc) const;
  const char* setLight(JsonDocument& doc);
  bool command(JsonDocument& doc, const char*& error, bool radioBusy);
  FeatureSnapshot snapshot(unsigned irSlot) const;
  bool takeChanged();
  bool screenOn() const { return screenOn_; }
  void screenOn(bool on);
  IrBridge ir;
private:
  bool initNfc();
  void probe();
  void nfcTick(bool busy);
  void nrfTick(bool busy);
  void micTick(bool busy);
  bool startMic();
  void stopMic();
  const char* beep(unsigned hz, unsigned ms, unsigned volume);
  const char* backup();
  const char* configureNrf(JsonDocument& doc);
  const char* sendNrf(JsonDocument& doc);
  const char* scanNrf();
  void updateRgb(bool connected);
  RemoteStore& rfStore_;
  FeatureEvent event_ = nullptr;
  Adafruit_PN532 nfc_;
  Adafruit_NeoPixel pixels_;
  SPIClass spi_;
  RF24 nrf_;
  bool nfcPresent_ = false, nfcEnabled_ = true, nfcArmed_ = false, nfcPaused_ = false;
  bool nfcAckPending_ = false;
  uint32_t nfcCommandAt_ = 0;
  bool nrfPresent_ = false, nrfEnabled_ = false, nrfListening_ = false;
  bool micEnabled_ = false, micInstalled_ = false, micPaused_ = false;
  bool sdMounted_ = false, screenOn_ = true, changed_ = false;
  bool rgbOn_ = false, rgbStatus_ = false;
  uint8_t brightness_ = 32, red_ = 0, green_ = 180, blue_ = 255;
  uint8_t nrfAddress_[5] = {'H','A','0','0','1'}, nrfChannel_ = 76;
  uint32_t nfcCount_ = 0, nrfCount_ = 0, nrfDropped_ = 0;
  uint32_t nfcLastSeen_ = 0, nfcLastArm_ = 0, nrfLastEvent_ = 0, micLastRead_ = 0, noiseAt_ = 0;
  uint64_t sdTotalMb_ = 0, sdUsedMb_ = 0;
  float noiseDbfs_ = -96;
  String nfcUid_, nrfPayload_;
};
