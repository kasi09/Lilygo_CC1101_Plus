#pragma once
#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <functional>

struct NetworkProfile {
  uint32_t magic = 0x4c474e31;
  uint16_t version = 1;
  uint16_t mqttPort = 1883;
  uint8_t discovery = 0;
  char ssid[33] = {};
  char wifiPassword[65] = {};
  char mqttHost[254] = {};
  char mqttUser[129] = {};
  char mqttPassword[257] = {};
  char deviceId[33] = {};
};

class NetworkManager {
public:
  void begin(std::function<void()> beforeChange);
  void tick(bool serviceAllowed, bool mqttConnected, int mqttState);
  bool openPortal(bool automatic = false);
  void closePortal(bool dismissed = false);
  const NetworkProfile& profile() const { return profile_; }
  bool portalActive() const { return portalActive_; }
  bool saved() const { return saved_; }
  bool storageReady() const { return storageReady_; }
  const String& portalSsid() const { return portalSsid_; }
  const String& portalPassword() const { return portalPassword_; }
  bool takeChanged();
private:
  NetworkProfile profile_, pending_;
  Preferences preferences_;
  DNSServer dns_;
  WebServer server_{IPAddress(192, 168, 4, 1), 80};
  std::function<void()> beforeChange_;
  String portalSsid_, portalPassword_, csrf_;
  String networksJson_;
  bool scanning_ = false;
  bool storageReady_ = false, saved_ = false, portalActive_ = false;
  bool automatic_ = false, dismissed_ = false, changed_ = false, wasConnected_ = false;
  bool pendingApply_ = false, appliedInPortal_ = false, ready_ = false;
  bool mqttConnected_ = false;
  int mqttState_ = -1;
  uint32_t offlineSince_ = 0, lastWifiTry_ = 0, applyAt_ = 0, readyAt_ = 0;
  void connectStation();
  void page(const String& message = "", bool error = false);
  void status();
  void save();
  void startScan();
  void networks();
  void scanTick();
  void redirect();
  bool validRequest();
};
