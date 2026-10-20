#pragma once
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "setup_qr.h"
#include "board_telemetry.h"
#include "remote_store.h"
#include "device_features.h"

enum class DisplayPage : unsigned { Overview, Codes, Learn, Scan, Inputs, Infrared, Controls, Device, Network, Count };
constexpr unsigned DisplayControlCount = 7;
constexpr unsigned DisplayScanBins = 72;
struct ScanDisplayData {
  float lower = 433.05f, upper = 434.79f;
  float bins[DisplayScanBins];
  float latestFrequency = 0, latestRssi = -127;
  float peakFrequency = 0, peakRssi = -127;
  unsigned points = 0, expectedPoints = 0;
  bool finished = false, aborted = false, failed = false;
  ScanDisplayData();
  void reset(float start, float end, unsigned expected);
  void add(float frequency, float rssi);
};
struct DisplaySnapshot {
  DisplayPage page = DisplayPage::Overview;
  bool wifi = false, mqtt = false, storage = false, radioOk = false;
  bool learning = false, rawLearning = false, scanning = false;
  unsigned slot = 0, saved = 0, scanPreset = 0, secondsLeft = 0;
  uint32_t rxCount = 0, txCount = 0;
  float frequency = 433.92f, rssi = -127;
  int wifiRssi = -127;
  const Remote* remote = nullptr;
  const Remote* lastReceived = nullptr;
  const ScanDisplayData* scan = nullptr;
  BoardReadings board;
  FeatureSnapshot features;
  unsigned irSlot = 0, control = 0;
  String notice;
  bool setupActive = false;
  String setupSsid, setupPassword, networkIp;
};

class GatewayDisplay {
public:
  explicit GatewayDisplay(TFT_eSPI& screen) : screen_(screen), frame_(&screen) {}
  bool begin();
  void draw(const DisplaySnapshot& snapshot);
  void diagnostics(JsonObject output);
  void setupQrInfo(JsonObject output);
  static const char* resultText(const char* action, const char* error);
private:
  TFT_eSPI& screen_;
  TFT_eSprite frame_;
  SetupQr setupQr_;
  bool buffered_ = false;
};
