#include "network_manager.h"
#include "network_rules.h"
#include "settings.h"
#include "setup_page_script.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <algorithm>

namespace {
template <size_t N> bool field(const String& value, char (&target)[N]) {
  if (!NetworkRules::text(value.c_str(), value.length(), N - 1)) return false;
  strlcpy(target, value.c_str(), N);
  return true;
}
template <size_t N> bool terminated(const char (&value)[N]) {
  return memchr(value, '\0', N) != nullptr;
}
bool valid(const NetworkProfile& p) {
  return p.magic == 0x4c474e31 && p.version == 1 && p.discovery <= 1 &&
    terminated(p.ssid) && terminated(p.wifiPassword) && terminated(p.mqttHost) &&
    terminated(p.mqttUser) && terminated(p.mqttPassword) && terminated(p.deviceId) &&
    NetworkRules::text(p.ssid, strlen(p.ssid), 32, false) &&
    NetworkRules::wifiPassword(p.wifiPassword, strlen(p.wifiPassword)) &&
    NetworkRules::host(p.mqttHost, strlen(p.mqttHost)) && NetworkRules::port(p.mqttPort) &&
    NetworkRules::text(p.mqttUser, strlen(p.mqttUser), 128) &&
    NetworkRules::text(p.mqttPassword, strlen(p.mqttPassword), 256) &&
    NetworkRules::deviceId(p.deviceId, strlen(p.deviceId));
}
String escape(const char* input) {
  String out;
  for (const char* p = input; *p; ++p) {
    switch (*p) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += *p;
    }
  }
  return out;
}
String randomText(size_t length) {
  constexpr const char alphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz";
  String result; result.reserve(length);
  for (size_t i = 0; i < length; ++i) result += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  return result;
}
}

void NetworkManager::begin(std::function<void()> beforeChange) {
  beforeChange_ = beforeChange;
  strlcpy(profile_.ssid, GW_WIFI_SSID, sizeof(profile_.ssid));
  strlcpy(profile_.wifiPassword, GW_WIFI_PASSWORD, sizeof(profile_.wifiPassword));
  strlcpy(profile_.mqttHost, GW_MQTT_HOST, sizeof(profile_.mqttHost));
  strlcpy(profile_.mqttUser, GW_MQTT_USER, sizeof(profile_.mqttUser));
  strlcpy(profile_.mqttPassword, GW_MQTT_PASSWORD, sizeof(profile_.mqttPassword));
  strlcpy(profile_.deviceId, GW_DEVICE_ID, sizeof(profile_.deviceId));
  profile_.mqttPort = GW_MQTT_PORT; profile_.discovery = GW_MQTT_DISCOVERY;
  storageReady_ = preferences_.begin("gateway-net", false);
  if (storageReady_ && preferences_.getBytesLength("profile") == sizeof(NetworkProfile)) {
    NetworkProfile stored;
    if (preferences_.getBytes("profile", &stored, sizeof(stored)) == sizeof(stored) && valid(stored)) {
      profile_ = stored; saved_ = true;
    }
  }
  WiFi.persistent(false);
  WiFi.setHostname(profile_.deviceId);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  offlineSince_ = millis();
  const char* headers[] = {"Origin", "Content-Length"};
  server_.collectHeaders(headers, 2);
  server_.on("/", HTTP_GET, [this]() { page(); });
  server_.on("/status", HTTP_GET, [this]() { status(); });
  server_.on("/save", HTTP_POST, [this]() { save(); });
  server_.on("/scan", HTTP_POST, [this]() { startScan(); });
  server_.on("/networks", HTTP_GET, [this]() { networks(); });
  server_.onNotFound([this]() { redirect(); });
  if (profile_.ssid[0]) connectStation();
  else openPortal(true);
}

void NetworkManager::connectStation() {
  WiFi.begin(profile_.ssid, profile_.wifiPassword[0] ? profile_.wifiPassword : nullptr);
  lastWifiTry_ = millis();
}

bool NetworkManager::openPortal(bool automatic) {
  if (portalActive_) return true;
  if (!GW_SETUP_AP_ENABLED) return false;
  char suffix[7]; snprintf(suffix, sizeof(suffix), "%06lX", (unsigned long)((ESP.getEfuseMac() >> 24) & 0xffffff));
  portalSsid_ = String("LilyGO-Setup-") + suffix;
  portalPassword_ = randomText(12);
  csrf_ = randomText(32);
  WiFi.mode(WIFI_AP_STA);
  const IPAddress ip(192, 168, 4, 1);
  if (!WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0)) ||
      !WiFi.softAP(portalSsid_.c_str(), portalPassword_.c_str(), 1, false, 2)) {
    WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
    portalPassword_ = ""; return false;
  }
  dns_.start(53, "*", ip);
  server_.begin();
  portalActive_ = true; automatic_ = automatic; appliedInPortal_ = false;
  networksJson_ = "";
  dismissed_ = false; ready_ = false; changed_ = true;
  requestScan();
  return true;
}

void NetworkManager::closePortal(bool dismissed) {
  if (!portalActive_) return;
  server_.stop(); dns_.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_STA);
  portalActive_ = false; portalPassword_ = ""; csrf_ = "";
  networksJson_ = "";
  scanPending_ = false; scanVisible_ = 0;
  dismissed_ = dismissed; changed_ = true; ready_ = false;
}

bool NetworkManager::takeChanged() {
  const bool result = changed_; changed_ = false; return result;
}

void NetworkManager::tick(bool serviceAllowed, bool mqttConnected, int mqttState) {
  mqttConnected_ = mqttConnected; mqttState_ = mqttState;
  const uint32_t now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;
  scanTick(serviceAllowed);
  if (connected != wasConnected_) {
    wasConnected_ = connected; changed_ = true;
    if (!connected) offlineSince_ = now;
    else dismissed_ = false;
  }
  if (serviceAllowed && !scanning_ && pendingApply_ && int32_t(now - applyAt_) >= 0) {
    if (beforeChange_) beforeChange_();
    WiFi.disconnect(false, false);
    profile_ = pending_;
    pendingApply_ = false; appliedInPortal_ = true;
    offlineSince_ = now; wasConnected_ = false; ready_ = false;
    connectStation(); changed_ = true;
    return;
  }
  if (serviceAllowed && !portalActive_ &&
      NetworkRules::fallback(profile_.ssid[0], connected, dismissed_, now, offlineSince_)) openPortal(true);
  if (serviceAllowed && !scanning_ && !scanPending_ && !connected && profile_.ssid[0] && now - lastWifiTry_ >= 15000) connectStation();
  if (portalActive_) {
    // The setup server listens only on the AP address. RF learning/scan has priority.
    if (serviceAllowed) { dns_.processNextRequest(); server_.handleClient(); }
    const bool networkReady = connected && (!profile_.mqttHost[0] || mqttConnected);
    if (!networkReady || pendingApply_) ready_ = false;
    else if ((automatic_ || appliedInPortal_) && !ready_) { ready_ = true; readyAt_ = now; }
    if (serviceAllowed && ready_ && now - readyAt_ >= NetworkRules::ReadyCloseMs) closePortal();
  }
}

void NetworkManager::redirect() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("Location", "http://192.168.4.1/");
  server_.send(302, "text/plain", "Einrichtung: http://192.168.4.1/");
}

bool NetworkManager::validRequest() {
  if (!portalActive_ || pendingApply_ || !csrf_.length() || server_.arg("token") != csrf_) return false;
  const String origin = server_.header("Origin");
  return (!origin.length() || origin == "http://192.168.4.1") &&
    server_.header("Content-Length").toInt() > 0 && server_.header("Content-Length").toInt() <= 3072;
}

void NetworkManager::startScan() {
  server_.sendHeader("Cache-Control", "no-store");
  if (!validRequest()) { server_.send(403, "application/json", "{\"error\":\"invalid_session\"}"); return; }
  if (requestScan()) {
    server_.send(503, "application/json", "{\"error\":\"scan_unavailable\"}"); return;
  }
  server_.send(202, "application/json", "{\"scanning\":true}");
}

const char* NetworkManager::requestScan() {
  if (!portalActive_ || pendingApply_) return "setup_unavailable";
  if (scanning_ || scanPending_) return nullptr;
  scanPending_ = true; scanAttempts_ = 0;
  scanRetryAt_ = millis(); scanStarted_ = millis(); scanFinished_ = 0;
  return nullptr;
}

void NetworkManager::diagnostics(JsonObject output, bool includePage, bool includeNetworks) {
  output["portal_active"] = portalActive_;
  output["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  output["mqtt_connected"] = mqttConnected_;
  output["config_saved"] = saved_;
  output["wifi_mode"] = unsigned(WiFi.getMode());
  output["wifi_status"] = unsigned(WiFi.status());
  output["scan_running"] = scanning_ || scanPending_;
  output["scan_attempts"] = scanAttempts_;
  const bool complete = WiFiGenericClass::getStatusBits() & WIFI_SCAN_DONE_BIT;
  output["scan_driver_state"] = scanning_ && !complete ? WIFI_SCAN_RUNNING : WiFi.scanComplete();
  output["scan_reported"] = scanReported_;
  output["scan_visible"] = scanVisible_;
  output["scan_elapsed_ms"] = scanStarted_ ? (scanFinished_ ? scanFinished_ : millis()) - scanStarted_ : 0;
  // Explicit owner USB request only. HTML includes the temporary form token.
  if (includePage && portalActive_) output["html"] = renderPage();
  if (includeNetworks && portalActive_) {
    JsonDocument data;
    if (!deserializeJson(data, scanPayload())) output["scan_result"] = data.as<JsonObject>();
  }
}

void NetworkManager::scanTick(bool serviceAllowed) {
  if (scanPending_ && serviceAllowed && !pendingApply_ && int32_t(millis() - scanRetryAt_) >= 0) {
    ++scanAttempts_;
    scanReported_ = WiFi.scanNetworks(true, false, false, 150);
    if (scanReported_ == WIFI_SCAN_RUNNING) {
      scanning_ = true; scanPending_ = false; scanDriverStarted_ = millis();
    }
    else if (scanAttempts_ < 4) {
      // A connection attempt has priority in the ESP32 driver. Try again without disconnecting it.
      scanRetryAt_ = millis() + 1000;
      return;
    } else {
      scanPending_ = false;
      scanFinished_ = millis();
      JsonDocument failure;
      if (networksJson_.length()) deserializeJson(failure, networksJson_);
      failure["scanning"] = false; failure["error"] = "scan_unavailable";
      if (!failure["networks"].is<JsonArray>()) failure["networks"].to<JsonArray>();
      networksJson_ = ""; serializeJson(failure, networksJson_);
      return;
    }
  }
  if (!scanning_) {
    if (!portalActive_ && (WiFiGenericClass::getStatusBits() & WIFI_SCAN_DONE_BIT)) WiFi.scanDelete();
    return;
  }
  // Arduino 2.0.17's scanComplete() times out after dwell*20 (3 s here),
  // although a connected station can still be scanning. Wait for the real done bit.
  const bool complete = WiFiGenericClass::getStatusBits() & WIFI_SCAN_DONE_BIT;
  if (!complete && millis() - scanDriverStarted_ < 15000) return;
  const int16_t count = complete ? WiFi.scanComplete() : WIFI_SCAN_FAILED;
  if (!complete) esp_wifi_scan_stop();
  scanning_ = false;
  scanFinished_ = millis();
  scanReported_ = count;
  JsonDocument doc;
  if (count < 0 && networksJson_.length()) deserializeJson(doc, networksJson_);
  doc["scanning"] = false;
  auto list = count < 0 && doc["networks"].is<JsonArray>() ? doc["networks"].as<JsonArray>() : doc["networks"].to<JsonArray>();
  if (count < 0) doc["error"] = "scan_failed";
  if (portalActive_ && count > 0) {
    struct Network { String ssid; int32_t rssi; bool secure; } found[24];
    unsigned used = 0;
    for (int i = 0; i < count && i < 256; ++i) {
      const String ssid = WiFi.SSID(i);
      if (!NetworkRules::text(ssid.c_str(), ssid.length(), 32, false) || ssid == portalSsid_) continue;
      const int32_t rssi = WiFi.RSSI(i);
      const bool secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      unsigned slot = 0;
      while (slot < used && found[slot].ssid != ssid) ++slot;
      if (slot < used) {
        if (rssi > found[slot].rssi) found[slot] = {ssid, rssi, secure};
      } else if (used < 24) found[used++] = {ssid, rssi, secure};
      else {
        auto weakest = std::min_element(found, found + used,
          [](const Network& a, const Network& b) { return a.rssi < b.rssi; });
        if (rssi > weakest->rssi) *weakest = {ssid, rssi, secure};
      }
    }
    std::sort(found, found + used, [](const Network& a, const Network& b) { return a.rssi > b.rssi; });
    scanVisible_ = used;
    for (unsigned i = 0; i < used; ++i) {
      auto item = list.add<JsonObject>();
      item["ssid"] = found[i].ssid; item["rssi"] = found[i].rssi; item["secure"] = found[i].secure;
    }
  }
  networksJson_ = "";
  if (portalActive_) serializeJson(doc, networksJson_);
  WiFi.scanDelete();
}

String NetworkManager::scanPayload() const {
  JsonDocument doc;
  if (networksJson_.length()) deserializeJson(doc, networksJson_);
  doc["scanning"] = scanning_ || scanPending_;
  if (!doc["networks"].is<JsonArray>()) doc["networks"].to<JsonArray>();
  if (scanPending_ || scanning_) {
    doc.remove("error"); doc["phase"] = scanPending_ && scanAttempts_ ? "retrying" : "scanning";
  }
  String payload; serializeJson(doc, payload); return payload;
}

void NetworkManager::networks() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", scanPayload());
}

void NetworkManager::status() {
  JsonDocument doc;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["mqtt_connected"] = mqttConnected_;
  doc["mqtt_configured"] = bool(profile_.mqttHost[0]);
  doc["mqtt_state"] = mqttState_;
  doc["saving"] = pendingApply_;
  doc["closing"] = ready_;
  String payload; serializeJson(doc, payload);
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", payload);
}

void NetworkManager::save() {
  if (!validRequest()) { server_.send(403, "text/plain; charset=utf-8", "Sitzung ungültig. Einrichtungsseite neu öffnen."); return; }
  if (!storageReady_) { page("Einstellungen können nicht gespeichert werden: Gerätespeicher nicht verfügbar.", true); return; }
  NetworkProfile candidate;
  String ssid = server_.arg("network_ssid");
  if (!ssid.length()) ssid = server_.arg("ssid");
  String wifiPassword = server_.arg("wifi_password");
  if (server_.hasArg("open_wifi")) wifiPassword = "";
  else if (!wifiPassword.length() && ssid == profile_.ssid) wifiPassword = profile_.wifiPassword;
  else if (!wifiPassword.length()) { page("WLAN-Passwort eingeben oder ausdrücklich WLAN ohne Passwort auswählen.", true); return; }
  const String mqttHost = server_.arg("mqtt_host"), mqttUser = server_.arg("mqtt_user");
  String mqttPassword = server_.arg("mqtt_password");
  if (server_.hasArg("clear_mqtt")) mqttPassword = "";
  else if (!mqttPassword.length() && mqttHost == profile_.mqttHost && mqttUser == profile_.mqttUser)
    mqttPassword = profile_.mqttPassword;
  const String portText = server_.arg("mqtt_port");
  bool decimal = portText.length() > 0 && portText.length() <= 5;
  for (size_t i = 0; i < portText.length(); ++i) if (portText[i] < '0' || portText[i] > '9') decimal = false;
  const unsigned long port = portText.toInt();
  if (!decimal || !NetworkRules::port(port)) { page("MQTT-Port muss zwischen 1 und 65535 liegen.", true); return; }
  candidate.mqttPort = port;
  candidate.discovery = server_.arg("discovery") == "1" ? 1 : 0;
  if (!field(ssid, candidate.ssid) || !field(wifiPassword, candidate.wifiPassword) ||
      !field(mqttHost, candidate.mqttHost) || !field(mqttUser, candidate.mqttUser) ||
      !field(mqttPassword, candidate.mqttPassword) || !field(server_.arg("device_id"), candidate.deviceId) ||
      !valid(candidate)) {
    page("Bitte Eingaben prüfen: WLAN 1–32 Bytes, WPA-Passwort 8–63 Bytes oder 64 Hex-Zeichen; Broker ohne http://, Geräte-ID nur a–z, 0–9, _ und -.", true); return;
  }
  if (preferences_.putBytes("profile", &candidate, sizeof(candidate)) != sizeof(candidate)) {
    page("Speichern fehlgeschlagen. Die bisherigen Einstellungen bleiben aktiv.", true); return;
  }
  pending_ = candidate; saved_ = true; pendingApply_ = true; applyAt_ = millis() + 1000;
  scanPending_ = false;
  page("Gespeichert. Die Verbindung wird aufgebaut. Bei Fehlern bleibt dieser Hotspot erreichbar; sonst endet er nach 10 Sekunden.");
}

String NetworkManager::networkOptions() const {
  String html = F("<option value=''>WLAN waehlen / manuell eingeben</option>");
  JsonDocument cached;
  if (networksJson_.length()) deserializeJson(cached, networksJson_);
  for (JsonObjectConst item : cached["networks"].as<JsonArrayConst>()) {
    const String ssid = item["ssid"].as<String>();
    html += F("<option value='"); html += escape(ssid.c_str()); html += F("' data-secure='");
    html += item["secure"].as<bool>() ? '1' : '0'; html += F("'>");
    html += escape(ssid.c_str()); html += F(" / "); html += item["rssi"].as<int>();
    html += item["secure"].as<bool>() ? F(" dBm / gesichert</option>") : F(" dBm / offen</option>");
  }
  return html;
}

String NetworkManager::renderPage(const String& message, bool error) {
  const NetworkProfile& view = pendingApply_ ? pending_ : profile_;
  String html; html.reserve(13000);
  html = F("<!doctype html><html lang='de'><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>LilyGO einrichten</title><style>"
    "body{font:16px system-ui;background:#101a24;color:#eaf3fa;margin:0;padding:24px}main{max-width:560px;margin:auto}h1{font-size:1.7rem;color:#64dfce}label{display:block;margin:16px 0 6px}input,select,button{box-sizing:border-box;width:100%;padding:12px;border-radius:8px;border:1px solid #526778;background:#1c2c3a;color:inherit;font:inherit}input[type=checkbox]{width:auto;margin-right:8px}button{margin-top:24px;background:#64dfce;color:#102029;font-weight:700}small,p{line-height:1.5;color:#bed0dc}.message{padding:12px;border:1px solid #64dfce;border-radius:8px}.error{border-color:#ff9b8f}details{margin-top:22px}summary{cursor:pointer}a{color:#64dfce}#status{padding:12px 0}</style><main><h1>LilyGO einrichten</h1><p>WLAN und MQTT direkt auf dem Gerät speichern. Ein erneutes Flashen ist dafür nicht nötig.</p>");
  if (message.length()) html += String("<p class='message") + (error ? " error" : "") + "'>" + escape(message.c_str()) + "</p>";
  html += F("<p id='status' aria-live='polite'>Verbindungsstatus wird geladen …</p><form method='post' action='http://192.168.4.1/save' data-saving='");
  html += pendingApply_ ? '1' : '0';
  html += F("'><input type='hidden' name='token' value='");
  html += csrf_; html += F("'><label for='networks'>Gefundene WLANs (2,4 GHz)</label><select id='networks' name='network_ssid'>");
  html += networkOptions();
  html += F("</select><button id='scan' type='button'>Erneut nach WLAN suchen</button><p id='scan-status' aria-live='polite'>Die WLAN-Liste wird auf dem Geraet vorbereitet. Auswahl und manuelle Eingabe sind sofort nutzbar.</p><p><a href='/'>WLAN-Liste neu laden</a></p><noscript><p>Die Auswahl funktioniert auch ohne JavaScript. Falls die Liste noch leer ist, nach wenigen Sekunden neu laden oder den Namen direkt eingeben.</p></noscript><label for='ssid'>WLAN-Name bei manueller Eingabe</label><input id='ssid' name='ssid' maxlength='32' autocomplete='off' value='");
  html += escape(view.ssid);
  html += F("'><label for='wifi_password'>WLAN-Passwort</label><input id='wifi_password' name='wifi_password' type='password' maxlength='64' autocomplete='new-password'><small>Beim gleichen WLAN leer lassen, um das gespeicherte Passwort beizubehalten.</small><label><input type='checkbox' name='open_wifi'>WLAN ausdrücklich ohne Passwort verwenden</label><label for='mqtt_host'>MQTT-Broker: IP-Adresse oder Hostname</label><input id='mqtt_host' name='mqtt_host' maxlength='253' autocomplete='off' value='");
  html += escape(view.mqttHost);
  html += F("'><small>Meist die LAN-IP des Home-Assistant-Rechners. Leer lassen, um MQTT auszuschalten.</small><label for='mqtt_port'>MQTT-Port</label><input id='mqtt_port' name='mqtt_port' type='number' min='1' max='65535' required value='");
  html += view.mqttPort;
  html += F("'><label for='mqtt_user'>MQTT-Benutzer</label><input id='mqtt_user' name='mqtt_user' maxlength='128' autocomplete='off' value='");
  html += escape(view.mqttUser);
  html += F("'><label for='mqtt_password'>MQTT-Passwort</label><input id='mqtt_password' name='mqtt_password' type='password' maxlength='256' autocomplete='new-password'><small>Leer bedeutet beibehalten, wenn Broker und Benutzer gleich bleiben.</small><details><summary>Weitere Einstellungen</summary><label><input type='checkbox' name='clear_mqtt'>Gespeichertes MQTT-Passwort löschen</label><label for='device_id'>Eindeutige Geräte-ID</label><input id='device_id' name='device_id' maxlength='32' pattern='[a-z0-9_-]+' required value='");
  html += escape(view.deviceId);
  html += F("'><small>Home-Assistant-Basistopic: lilygo/GERÄTE-ID. Bei mehreren Geräten unterschiedliche IDs verwenden.</small><label for='discovery'>Home-Assistant-Anbindung</label><select id='discovery' name='discovery'><option value='0'");
  if (!view.discovery) html += F(" selected");
  html += F(">Eigene Integration (HACS)</option><option value='1'");
  if (view.discovery) html += F(" selected");
  html += F(">MQTT Discovery</option></select></details><button type='submit'>Speichern und verbinden</button></form><p>Der Hotspot hat keinen Internetzugang. Beim Verbindungsaufbau kann eine erneute Verbindung mit dem Hotspot nötig sein. Passwort und Adresse stehen auf dem LilyGO-Display.</p><script>");
  html += FPSTR(SetupPageScript);
  html += F("</script></main></html>");
  return html;
}

void NetworkManager::page(const String& message, bool error) {
  if (server_.hostHeader().length() && server_.hostHeader() != "192.168.4.1" &&
      server_.hostHeader() != "192.168.4.1:80") { redirect(); return; }
  const String html = renderPage(message, error);
  server_.sendHeader("Cache-Control", "no-store");
  server_.sendHeader("X-Frame-Options", "DENY");
  // no-referrer makes normal HTML form POSTs send Origin: null and fail validRequest().
  server_.sendHeader("Referrer-Policy", "same-origin");
  server_.sendHeader("Content-Security-Policy", "default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; connect-src 'self'; form-action http://192.168.4.1; frame-ancestors 'none'");
  server_.send(error ? 400 : 200, "text/html; charset=utf-8", html);
}
