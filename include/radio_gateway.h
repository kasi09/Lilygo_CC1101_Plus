#pragma once
#include <Arduino.h>
#include <RadioLib.h>
#include <RCSwitch.h>
#include "board_pins.h"
#include "remote_store.h"

class RadioGateway {
public:
  enum class Mode { Receive, Scan, RawLearn, Error };
  struct Sample { float frequency; float rssi; bool finished; };
  RadioGateway();
  bool begin();
  bool tune(float mhz);
  bool received(Remote& remote);
  bool startRaw();
  bool rawReceived(Remote& remote);
  bool scan(const char* band);
  bool scan(float start, float end, float step);
  bool scanTick(Sample& sample);
  bool stop();
  const char* send(const Remote& remote, unsigned repeats);
  float frequency() const { return frequency_; }
  float rssi();
  Mode mode() const { return mode_; }
  const char* modeName() const;
  int16_t error() const { return error_; }
private:
  bool check(int16_t state);
  bool receive();
  void detach();
  void antenna(float mhz);
  SPIClass spi_;
  Module module_;
  CC1101 radio_;
  RCSwitch decoder_;
  Mode mode_ = Mode::Error;
  bool initialized_ = false;
  bool decoderActive_ = false;
  int16_t error_ = 0;
  float frequency_ = Settings::DefaultFrequency;
  float scanStart_ = 0, scanEnd_ = 0, scanStep_ = 0;
  unsigned scanIndex_ = 0, scanBand_ = 0;
  bool allBands_ = false;
  uint32_t sampleDue_ = 0, lastTransmit_ = 0;
  bool hasTransmitted_ = false;
};
