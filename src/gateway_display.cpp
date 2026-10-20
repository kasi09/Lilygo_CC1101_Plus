#include "gateway_display.h"
#include <cmath>
#include <cstring>

ScanDisplayData::ScanDisplayData() { reset(lower, upper, 0); }
void ScanDisplayData::reset(float start, float end, unsigned expected) {
  lower = start; upper = end; expectedPoints = expected;
  latestFrequency = peakFrequency = 0; latestRssi = peakRssi = -127;
  points = 0; finished = aborted = failed = false;
  for (float& bin : bins) bin = -127;
}
void ScanDisplayData::add(float frequency, float rssi) {
  latestFrequency = frequency; latestRssi = rssi;
  ++points;
  if (points == 1 || rssi > peakRssi) { peakFrequency = frequency; peakRssi = rssi; }
  if (!std::isfinite(frequency) || !std::isfinite(rssi)) return;
  const float position = upper > lower ? (frequency - lower) / (upper - lower) : 0;
  const int bin = constrain(int(position * DisplayScanBins), 0, int(DisplayScanBins) - 1);
  if (rssi > bins[bin]) bins[bin] = rssi;
}

namespace {
constexpr uint16_t Background = 0x0843, Panel = 0x10C6, Border = 0x294A;
constexpr uint16_t Ink = 0xEF7D, Muted = 0x94B4, Accent = 0x4DDF;
constexpr uint16_t Green = 0x4E91, Amber = 0xFD68, Red = 0xF289;
constexpr const char* PageTitles[] = {"EMPFANG", "GESPEICHERTE CODES", "FERNBEDIENUNG LERNEN", "FUNK-SCANNER", "EINGAENGE", "INFRAROT", "BEDIENUNG", "GERAETEWERTE", "WLAN / MQTT EINRICHTEN"};
constexpr const char* Presets[] = {"433 MHz", "868 MHz", "Alle Bereiche"};
constexpr const char* Controls[] = {"Signalton abspielen", "RGB Ring umschalten", "Mikrofonpegel umschalten",
  "NFC Leser umschalten", "nRF24 Empfang umschalten", "Codes auf SD sichern", "Hardware erneut pruefen"};
static_assert(sizeof(PageTitles) / sizeof(PageTitles[0]) == unsigned(DisplayPage::Count), "every page needs a title");
static_assert(sizeof(Controls) / sizeof(Controls[0]) == DisplayControlCount, "control list matches navigation");

template <class Surface>
void text(Surface& c, String value, int x, int y, uint16_t color = Ink,
          unsigned font = 2, int width = 304, unsigned size = 1) {
  value.replace("\xC3\xA4", "ae"); value.replace("\xC3\xB6", "oe");
  value.replace("\xC3\xBC", "ue"); value.replace("\xC3\x9F", "ss");
  value.replace("\xC3\x84", "Ae"); value.replace("\xC3\x96", "Oe");
  value.replace("\xC3\x9C", "Ue");
  for (unsigned i = 0; i < value.length(); ++i)
    if (uint8_t(value[i]) < 32 || uint8_t(value[i]) == 127) value.setCharAt(i, ' ');
  c.setTextFont(font); c.setTextSize(size); c.setTextColor(color);
  c.setTextDatum(TL_DATUM); c.setTextPadding(0);
  if (c.textWidth(value) > width) {
    while (value.length() && c.textWidth(value + "...") > width) {
      unsigned end = value.length() - 1;
      while (end && (uint8_t(value[end]) & 0xC0) == 0x80) --end;
      value.remove(end);
    }
    value += "...";
  }
  c.drawString(value, x, y);
  c.setTextSize(1);
}
template <class Surface>
void card(Surface& c, int x, int y, int w, int h) {
  c.fillRoundRect(x, y, w, h, 6, Panel);
  c.drawRoundRect(x, y, w, h, 6, Border);
}
template <class Surface>
void button(Surface& c, const String& label, bool enabled = true) {
  c.fillRoundRect(8, 117, 304, 19, 4, enabled ? Accent : Border);
  c.setTextFont(1); c.setTextSize(1); c.setTextColor(enabled ? Background : Muted);
  c.setTextDatum(MC_DATUM); c.drawString(label, 160, 126); c.setTextDatum(TL_DATUM);
}
template <class Surface>
void render(Surface& c, const DisplaySnapshot& s) {
  const auto page = s.learning ? DisplayPage::Learn : s.scanning ? DisplayPage::Scan : s.features.irLearning ? DisplayPage::Infrared : s.page;
  c.fillScreen(Background);
  c.fillRect(0, 0, 320, 19, Panel);
  text(c, "T-EMBED CC1101 PLUS", 8, 5, Accent, 1, 182);
  text(c, s.setupActive ? "AP" : "WLAN", 219, 5, s.setupActive ? Amber : s.wifi ? Green : Muted, 1, 31);
  text(c, "MQTT", 274, 5, s.mqtt ? Green : Muted, 1, 31);
  c.fillCircle(212, 8, 2, s.setupActive ? Amber : s.wifi ? Green : Red);
  c.fillCircle(267, 8, 2, s.mqtt ? Green : Red);
  text(c, PageTitles[unsigned(page)], 8, 23, Ink, 2, 266);
  text(c, String(unsigned(page) + 1) + "/" + unsigned(DisplayPage::Count), 288, 28, Muted, 1, 25);

  const Remote empty;
  const Remote& remote = s.remote ? *s.remote : empty;
  const String slot = String("Platz ") + String(s.slot);
  String footer;
  switch (page) {
    case DisplayPage::Overview: {
      text(c, String(s.frequency, 2), 8, 46, Accent, 2, 142, 2);
      text(c, "MHz", 145, 61, Muted, 1, 27);
      text(c, s.radioOk ? "FUNK BEREIT" : "FUNK FEHLER", 202, 47, s.radioOk ? Green : Red, 1, 110);
      text(c, String("RX ") + s.rxCount + "   TX " + s.txCount, 202, 64, Ink, 1, 110);
      text(c, String("Codes ") + s.saved + "/16", 202, 80, Muted, 1, 110);
      text(c, String("Funkpegel ") + String(s.rssi, 0) + " dBm", 8, 83, Muted, 1, 177);
      c.fillRoundRect(8, 97, 176, 5, 2, Border);
      const int strength = constrain(int((s.rssi + 110) * 176 / 80), 0, 176);
      if (strength) c.fillRoundRect(8, 97, strength, 5, 2, Accent);
      if (s.lastReceived) {
        const Remote& last = *s.lastReceived;
        text(c, "LETZTER EMPFANG", 8, 109, Muted, 1);
        text(c, last.kind == RemoteKind::Raw ? String("RAW / ") + last.count + " Pulse" :
                  String(last.code) + " / " + last.bits + " Bit / RC" + last.protocol,
             8, 120, Ink, 1);
      } else text(c, "Warte auf ein passendes Funksignal", 8, 115, Muted, 1);
      footer = "Seite: weiter | Klick: Codes";
      break;
    }
    case DisplayPage::Codes: {
      card(c, 8, 45, 304, 66);
      text(c, slot, 16, 50, Accent, 1, 130);
      text(c, remote.kind == RemoteKind::Empty ? "LEER" : remote.kind == RemoteKind::Rc ? "FESTCODE" : "RAW",
           246, 50, Muted, 1, 59);
      text(c, remote.kind == RemoteKind::Empty ? "Noch kein Code gespeichert" : remote.name,
           16, 64, Ink, 2, 288);
      if (remote.kind != RemoteKind::Empty) {
        const String detail = remote.kind == RemoteKind::Rc ?
          String(remote.code) + " / " + remote.bits + " Bit / RC" + remote.protocol :
          String(remote.count) + " Pulse / " + remote.gapUs + " us Pause";
        text(c, String(remote.frequency, 2) + " MHz   " + detail, 16, 92, Muted, 1, 288);
      }
      button(c, remote.kind == RemoteKind::Empty ? "Diesen Platz auf der Lernseite belegen" : "DRUECKEN: CODE SENDEN",
             remote.kind != RemoteKind::Empty && s.radioOk);
      footer = "Drehen: Platz | Seite: weiter";
      break;
    }
    case DisplayPage::Learn: {
      if (s.learning) {
        text(c, String(s.secondsLeft) + " s", 12, 47, Accent, 2, 120, 2);
        text(c, s.rawLearning ? "RAW-AUFNAHME" : "RC-FESTCODE", 165, 50, Amber, 1, 145);
        text(c, slot, 165, 67, Ink, 2, 145);
        c.fillRoundRect(12, 89, 296, 6, 3, Border);
        const unsigned remaining = min(s.secondsLeft, 15u) * 296 / 15;
        if (remaining) c.fillRoundRect(12, 89, remaining, 6, 3, Accent);
        text(c, "Jetzt Fernbedienung betaetigen", 12, 103, Ink, 2, 296);
        text(c, s.rawLearning ? "RAW: kurze Taste, wiederholte Rahmen" : "Der naechste passende Code wird gespeichert",
             12, 124, Muted, 1, 296);
        footer = "Klick oder Seite: Abbrechen";
      } else {
        card(c, 8, 45, 304, 66);
        text(c, slot + " als Ziel", 16, 50, Accent, 2, 288);
        text(c, remote.kind == RemoteKind::Empty ? "Freier Speicherplatz" : String("Ersetzt: ") + remote.name,
             16, 73, remote.kind == RemoteKind::Empty ? Muted : Amber, 1, 288);
        text(c, "15 s Lernzeit / aktuelle Empfangsfrequenz", 16, 92, Muted, 1, 288);
        button(c, "KLICK: RC LERNEN | HALTEN: RAW LERNEN", s.radioOk && s.storage);
        footer = "Drehen: Zielplatz | Seite: weiter";
      }
      break;
    }
    case DisplayPage::Scan: {
      const ScanDisplayData blank;
      const ScanDisplayData& scan = s.scan ? *s.scan : blank;
      text(c, s.scanning ? String("LAEUFT / ") + String(scan.latestFrequency, 2) + " MHz" :
                String(Presets[s.scanPreset % 3]) + (scan.aborted ? " / gestoppt" : scan.failed ? " / Fehler" : scan.finished ? " / fertig" : ""),
           8, 45, s.scanning ? Green : Accent, 1, 209);
      text(c, String(scan.points) + "/" + scan.expectedPoints, 248, 45, Muted, 1, 64);
      const int x = 36, y = 60, w = 272, h = 42;
      for (int line = 0; line < 3; ++line) c.drawFastHLine(x, y + line * h / 2, w, Border);
      text(c, "-30", 8, y, Muted, 1, 25); text(c, "-110", 8, y + h - 7, Muted, 1, 25);
      for (unsigned i = 0; i < DisplayScanBins; ++i) {
        if (scan.bins[i] <= -127) continue;
        const int height = constrain(int((scan.bins[i] + 110) * h / 80), 1, h);
        const int left = x + i * w / DisplayScanBins;
        const int right = x + (i + 1) * w / DisplayScanBins;
        c.fillRect(left, y + h - height, max(1, right - left - 1), height, Accent);
      }
      text(c, String(scan.lower, 2), x, 106, Muted, 1, 104);
      text(c, String(scan.upper, 2) + " MHz", 231, 106, Muted, 1, 79);
      if (scan.points) text(c, String("Peak ") + String(scan.peakFrequency, 2) + " MHz / " +
                               String(scan.peakRssi, 0) + " dBm", 8, 120, Ink, 1);
      else text(c, "Drehen: Band waehlen, dann Scan starten", 8, 120, Muted, 1);
      if (s.scanning && scan.expectedPoints) {
        c.fillRect(8, 133, 304, 2, Border);
        c.fillRect(8, 133, min(304u, scan.points * 304 / scan.expectedPoints), 2, Green);
      }
      footer = s.scanning ? "Klick oder Seite: Scan stoppen" :
        String("Klick: ") + Presets[s.scanPreset % 3] + " | Drehen: Band | Seite: weiter";
      break;
    }
    case DisplayPage::Inputs: {
      const FeatureSnapshot& f = s.features;
      text(c, String("NFC ") + (f.nfcEnabled ? f.nfcAvailable ? f.nfcUid.length() ? f.nfcUid : "bereit" : "fehlt / pausiert" : "aus"), 8, 47, Accent, 2);
      text(c, String("IR  ") + (f.irEnabled ? f.irCode.length() ? f.irCode : "bereit" : "aus"), 8, 70, Ink, 1);
      text(c, String("nRF ") + (f.nrfAvailable ? f.nrfEnabled ? f.nrfPayload.length() ? f.nrfPayload : "warte auf Paket" : "aus" : "nicht erkannt"), 8, 87, Ink, 1);
      text(c, f.noiseAvailable ? String("Mikrofon ") + String(f.noiseDbfs, 1) + " dBFS (relativ)" :
              f.micEnabled ? "Mikrofon: keine aktuellen Werte" : "Mikrofon aus", 8, 104, Muted, 1);
      text(c, String("Ereignisse NFC ") + f.nfcCount + " / IR " + f.irCount + " / nRF " + f.nrfCount, 8, 121, Muted, 1);
      footer = "Seite: weiter | Steuern auf Bedienseite";
      break;
    }
    case DisplayPage::Infrared: {
      const FeatureSnapshot& f = s.features;
      card(c, 8, 45, 304, 66);
      text(c, String("IR Platz ") + (f.irLearning ? f.irLearnSlot : s.irSlot) + "/7", 16, 51, Accent, 2, 190);
      text(c, f.irLearning ? String(f.irSecondsLeft) + " s" : f.irSlotOccupied ? "BELEGT" : "LEER", 249, 54, f.irLearning ? Amber : Muted, 1, 55);
      text(c, f.irLearning ? "Jetzt IR-Fernbedienung druecken" : f.irSlotOccupied ? f.irSlotName : "Halten zum Anlernen", 16, 77, Ink, 2, 288);
      text(c, "8 Speicherplaetze / RAW: 38 kHz als Vorgabe", 16, 98, Muted, 1, 288);
      button(c, f.irLearning ? "KLICK: LERNEN ABBRECHEN" : "KLICK: SENDEN | HALTEN: LERNEN", f.irAvailable);
      footer = f.irLearning ? "Klick oder Seite: Abbrechen" : "Drehen: IR Platz | Seite: weiter";
      break;
    }
    case DisplayPage::Controls: {
      const FeatureSnapshot& f = s.features;
      card(c, 8, 45, 304, 66);
      text(c, String("Aktion ") + (s.control + 1) + "/" + DisplayControlCount, 16, 51, Accent, 1, 288);
      text(c, Controls[s.control % DisplayControlCount], 16, 69, Ink, 2, 288);
      String status = s.control == 1 ? f.rgbOn ? "Ring AN" : "Ring AUS" :
        s.control == 2 ? f.micEnabled ? "Mikrofon AN / keine Aufzeichnung" : "Mikrofon AUS" :
        s.control == 3 ? f.nfcEnabled ? "NFC AN" : "NFC AUS" :
        s.control == 4 ? f.nrfEnabled ? "nRF24 AN" : "nRF24 AUS / passend konfigurieren" :
        s.control == 5 ? f.sdMounted ? "SD eingebunden / JSONL Sicherung" : "SD fehlt / keine Formatierung" : "";
      text(c, status, 16, 96, Muted, 1, 288);
      button(c, "DRUECKEN: AKTION AUSFUEHREN");
      footer = "Drehen: Aktion | Seite: weiter";
      break;
    }
    case DisplayPage::Device: {
      card(c, 8, 45, 150, 88); card(c, 164, 45, 148, 88);
      text(c, "AKKU / BQ27220", 16, 52, Muted, 1, 133);
      text(c, s.board.batteryAvailable ? String(s.board.percent) + " %" : "-- %",
           16, 65, s.board.batteryAvailable ? Accent : Muted, 2, 133, 2);
      if (s.board.batteryAvailable) {
        const String amps = s.board.currentAvailable ? String(s.board.milliamps) + " mA" : "-- mA";
        text(c, String(s.board.millivolts / 1000.0f, 2) + " V  " + amps, 16, 103, Ink, 1, 133);
      } else text(c, "Werte nicht verfuegbar", 16, 103, Muted, 1, 133);
      text(c, s.board.temperatureAvailable ? String("Chip ") + String(s.board.chipCelsius, 1) + " C" : "Chiptemperatur --",
           16, 118, Muted, 1, 133);
      text(c, "VERBINDUNG", 172, 52, Muted, 1, 132);
      text(c, s.wifi ? String(s.wifiRssi) + " dBm" : "WLAN offline", 172, 68, s.wifi ? Green : Amber, 2, 132);
      text(c, s.mqtt ? "MQTT verbunden" : "MQTT offline", 172, 94, s.mqtt ? Green : Amber, 1, 132);
      text(c, s.storage ? String("Codes ") + s.saved + "/16" : "Speicherfehler", 172, 109,
           s.storage ? Ink : Red, 1, 132);
      text(c, s.board.chargerAvailable ? s.board.usbPower ? s.board.chargeState == 3 ? "USB / Akku voll" :
          s.board.chargeState ? "USB / laedt" : "USB / Ladepause" : "Akkubetrieb" : "Ladestatus --", 172, 123, Muted, 1, 132);
      footer = "Klick: Werte lesen | Seite: weiter";
      break;
    }
    case DisplayPage::Network: {
      if (s.setupActive) {
        text(c, "MIT DIESEM HOTSPOT VERBINDEN", 8, 46, Muted, 1);
        text(c, s.setupSsid, 8, 60, Accent, 2);
        text(c, "Passwort:", 8, 86, Muted, 1, 77);
        text(c, s.setupPassword, 91, 81, Ink, 2, 221);
        text(c, "Browser: http://192.168.4.1", 8, 106, Ink, 1);
        button(c, "KLICK: HOTSPOT SCHLIESSEN");
        footer = "Seite: weiter | Hotspot ohne Internet";
      } else {
        text(c, s.wifi ? "WLAN verbunden" : "WLAN nicht verbunden", 8, 49, s.wifi ? Green : Amber, 2);
        text(c, s.networkIp.length() ? String("IP: ") + s.networkIp : "USB und Funk bleiben nutzbar", 8, 74, Ink, 1);
        text(c, s.mqtt ? "MQTT verbunden" : "WLAN/MQTT hier neu einrichten", 8, 94, Muted, 1);
        button(c, "DRUECKEN: EINRICHTUNGS-HOTSPOT STARTEN");
        footer = "Seite: weiter | Zugangsdaten im Browser";
      }
      break;
    }
    default: break;
  }
  c.fillRect(0, 137, 320, 17, Background);
  text(c, s.notice, 8, 142, s.radioOk ? Muted : Red, 1, 304);
  c.fillRect(0, 156, 320, 14, Panel);
  text(c, footer, 8, 159, Muted, 1, 304);
}
}

bool GatewayDisplay::begin() {
  frame_.setColorDepth(16);
  buffered_ = frame_.createSprite(320, 170) != nullptr;
  if (!buffered_) {
    frame_.setColorDepth(8);
    buffered_ = frame_.createSprite(320, 170) != nullptr;
  }
  return buffered_;
}
void GatewayDisplay::draw(const DisplaySnapshot& snapshot) {
  if (buffered_) { render(frame_, snapshot); frame_.pushSprite(0, 0); }
  else render(screen_, snapshot);
}
const char* GatewayDisplay::resultText(const char* action, const char* error) {
  if (error) {
    struct Error { const char* code; const char* label; };
    static const Error errors[] = {
      {"learn_timeout", "Kein passender Code erkannt / Lernzeit vorbei"},
      {"tx_cooldown", "Bitte kurz warten, dann erneut senden"},
      {"tx_airtime_exceeded", "Code zu lang / Wiederholungen per USB reduzieren"},
      {"invalid_remote", "Speicherplatz leer oder Code ungueltig"},
      {"radio_error", "Funkmodul nicht bereit"},
      {"radio_restore_failed", "Empfang nach Senden fehlgeschlagen"},
      {"storage_unavailable", "Codespeicher nicht verfuegbar"},
      {"storage_write_failed", "Code konnte nicht gespeichert werden"},
      {"tx_disabled", "Senden in der Konfiguration deaktiviert"},
      {"tx_frequency_not_allowed", "Senden nur im freigegebenen 433-MHz-Bereich"},
      {"scan_rejected", "Scan konnte nicht gestartet werden"},
      {"stale_or_missing_session", "MQTT-Befehl veraltet / neu verbinden"}
      ,{"device_busy", "Erst Lernen oder Scan beenden"}
      ,{"sd_unavailable", "Keine SD Karte eingebunden"}
      ,{"nrf_unavailable", "nRF24 nicht erkannt"}
      ,{"ir_frame_too_long_or_invalid", "IR Rahmen zu lang / andere Taste versuchen"}
    };
    for (const auto& entry : errors) if (!strcmp(error, entry.code)) return entry.label;
    return "Befehl ungueltig / Details ueber USB";
  }
  if (!strcmp(action, "send")) return "Code gesendet";
  if (!strcmp(action, "learn")) return "Taste der Fernbedienung druecken";
  if (!strcmp(action, "scan")) return "Scan laeuft / Empfang pausiert";
  if (!strcmp(action, "stop")) return "Empfang fortgesetzt";
  if (!strcmp(action, "store")) return "Code gespeichert";
  if (!strcmp(action, "delete")) return "Speicherplatz geloescht";
  if (!strcmp(action, "tune")) return "Empfangsfrequenz umgestellt";
  if (!strcmp(action, "ir_learn")) return "Jetzt IR-Fernbedienung druecken";
  if (!strcmp(action, "ir_send")) return "IR Code gesendet";
  if (!strcmp(action, "backup")) return "Codes auf SD gesichert";
  return "Befehl ausgefuehrt";
}
