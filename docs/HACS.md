# LILYGO T-Embed CC1101 Plus – eigene Home-Assistant-Integration

Lokale Custom Component **0.2.0**, Domain `lilygo_t_embed`, für Home Assistant
**ab 2026.10.0** und Gateway-Firmware **ab 0.4.0**. Die Kommunikation läuft lokal
über die bereits eingerichtete MQTT-Integration. In Home Assistant werden keine
zweiten Broker-Zugangsdaten gespeichert.

## Lokal installieren

1. Das Paket `dist/lilygo_t_embed-0.2.0.zip` entpacken. Den enthaltenen Ordner
   `custom_components/lilygo_t_embed` nach
   `/config/custom_components/lilygo_t_embed` auf HAOS kopieren. Das Verzeichnis
   darf nicht doppelt verschachtelt sein; dort muss direkt `manifest.json` liegen.
2. Home Assistant neu starten. Unter **Einstellungen → Geräte & Dienste →
   Integration hinzufügen** nach **LILYGO T-Embed CC1101 Plus** suchen.
3. Gerätenamen und MQTT-Basistopic eintragen, standardmäßig `lilygo/lilygo_rf`.
   Für jedes Board einen eigenen `GW_DEVICE_ID` und ein eigenes Basistopic nutzen.
4. Der Verbindungstest wartet bis zu 25 Sekunden auf einen aktuellen Status.
   Mit **Ohne Verbindungstest vorbereiten** lässt sich das Gerät vorher anlegen;
   seine Entitäten bleiben bis zum Empfang passender Live-Daten nicht verfügbar.

## Eigenes Anlernfenster

Nach dem Update auf **0.2.0** Home Assistant neu starten und das Frontend neu
laden. Als Administrator erscheint **LILYGO** in der Seitenleiste. Die Seite
ist außerdem als Konfigurationsoberfläche der Integration registriert. Sie wird
automatisch eingerichtet; zusätzliche YAML- oder Lovelace-Ressourcen entfallen.

1. **LILYGO** öffnen und das gewünschte Gerät auswählen.
2. **Fernbedienung anlernen** oder **Anlernen** auf einem freien Platz drücken.
3. Einen Namen wie **Wohnzimmer EIN** eingeben. Für SEC24 HAF760R2 **Funk RAW**
   und **433,92 MHz** verwenden. RC und Infrarot stehen ebenfalls zur Auswahl.
4. **Anlernen starten** drücken und erst bei der Aufforderung die passende Taste
   der Fernbedienung drücken, bei Bedarf ungefähr zwei Sekunden halten.
5. Der Countdown zeigt das 15-Sekunden-Fenster. **Gespeichert** erscheint erst
   nach einem passenden Live-Ereignis vom Lilygo. Bei fehlender Bestätigung
   wird kein Erfolg angenommen und kein Versuch automatisch wiederholt.
6. AUS auf einem weiteren Platz separat aufnehmen. Gespeicherte Codes können
   über **Senden** auf der jeweiligen Karte einmalig übertragen werden.

Neue Aufnahmen wählen zunächst einen freien Platz. Beim erneuten Anlernen
eines belegten Platzes muss das Ersetzen ausdrücklich bestätigt werden.
**Abbrechen** beendet eine laufende Aufnahme am Gerät. Funk besitzt 16 Plätze
von 0 bis 15, Infrarot acht Plätze von 0 bis 7. Offline-Geräte und noch nicht
geladene Speicherlisten lassen sich nicht versehentlich überschreiben.
Die Oberfläche passt sich an Smartphone, Desktop und das HA-Farbschema an.

Die Seite verwendet ausschließlich lokale Dateien aus der Komponente und die
bestehende HA-Verbindung. Ihre Geräteübersicht enthält Namen und technische
Zustände, keine Funkcodes, Pulsfolgen, MQTT-Sessions oder Zugangsdaten.
Die bestehenden Aktionen bleiben für Automationen verfügbar.

Der Quellcode liegt unter
[kasi09/Lilygo_CC1101_Plus](https://github.com/kasi09/Lilygo_CC1101_Plus).
Die lokale Installation funktioniert ohne Repository-Zugriff. Für die
Installation über HACS muss das Repository öffentlich zugänglich sein;
private Repositories werden von HACS nicht unterstützt. Ein lokales ZIP kann
dort nicht als Repository hinzugefügt werden.

## WLAN und MQTT über den Hotspot einrichten

Ab Firmware **0.5.0** werden Zugangsdaten bequem im Browser eingegeben:

1. Ohne gespeichertes WLAN startet **LilyGO-Setup-…** sofort, bei einem
   WLAN-Ausfall nach 30 Sekunden. Das zufällige Passwort steht auf dem Display.
2. Mit diesem Hotspot verbinden und **http://192.168.4.1/** öffnen.
3. Ein 2,4-GHz-WLAN und den MQTT-Broker konfigurieren. Für diese Komponente unter
   **Weitere Einstellungen** die Anbindung **Eigene Integration (HACS)** wählen.
4. Speichern. Bei erfolgreicher Verbindung endet der Hotspot nach zehn Sekunden;
   bei Fehlern bleibt die Seite für Korrekturen erreichbar.

Die Geräte-ID ist standardmäßig `lilygo_rf`; damit lautet das Basistopic
`lilygo/lilygo_rf`. Eine im Portal geänderte ID auch in Home Assistant verwenden.
Gespeicherte Portal-Einstellungen haben Vorrang vor lokalen Build-Vorgaben.
Das Portal lässt sich über die neue Displayseite auch bei funktionierendem
WLAN manuell öffnen, beispielsweise zum Korrigieren der MQTT-Anmeldung.

## Alternative: Zugangsdaten beim Bauen vorgeben

Die Komponente unterstützt das Protokoll ab **0.4.0**. Die aktuelle Firmware
**0.5.0** bietet zusätzlich die Einrichtung im Browser. Bei lokalen Build-Vorgaben:

```cpp
// In include/config.local.h zusammen mit den eigenen WLAN-/MQTT-Zugangsdaten:
#define GW_DEVICE_ID "lilygo_rf"
#define GW_MQTT_DISCOVERY false
```

`config.example.h` setzt Discovery bereits auf `false`. Bestehende lokale
Konfigurationen ohne diesen Schalter behalten zur Kompatibilität Discovery bei;
deshalb den Schalter dort ausdrücklich ergänzen. Bei bereits gespeicherten
Portal-Einstellungen dort die gewünschte Anbindung wählen. Bauen und flashen wie in der
Projekt-README beschrieben. Die Komponente meldet ältere Firmware oder aktive
Discovery als inkompatibel. Bei Offline-Einrichtung bleiben dann alle Geräte-
Entitäten nicht verfügbar.

Beim Wechsel von bereits benutzter MQTT-Discovery zuerst deren altes Gerät und
die zugehörigen retained Discovery-Konfigurationen im Broker entfernen. Das
Abschalten verhindert neue Veröffentlichungen, löscht vorhandene Broker-Einträge
aber nicht. Keine fremden `homeassistant/#`-Topics pauschal löschen. Wenn das
Gerät bisher ohne MQTT betrieben wurde, entfällt dieser Migrationsschritt.

## Entitäten und Bedienung

| Bereich | Funktionen |
| --- | --- |
| Messwerte | Akku, Spannung, Strom, interne Akku-IC-Temperatur, Funk-/WLAN-Pegel, Frequenz, Zähler, Speicher, Betriebszeit, SD-Belegung, relativer Mikrofonpegel |
| Zustand | USB-Versorgung, Laden, Codespeicher, Akku-IC, NFC, IR, nRF24, SD |
| Schalter | Display, NFC-Leser, IR-Empfang, nRF24-Empfang, Mikrofon-Pegelmessung |
| Licht | RGB-Ring, Helligkeit, Farbe, Effekte Statisch und Status |
| Tasten | RSSI-Scans, Stoppen, Signalton, SD-Sicherung, Hardwareprüfung, Aktualisieren, 2,4-GHz-Aktivität |
| Codeplätze | 16 Funk- und acht IR-Sendetasten; leere oder noch nicht geladene Plätze sind nicht verfügbar. Der Name steht im Attribut `code_name`. |
| Ereignisse | Drehgeber/Tasten, Funk/IR/nRF24, NFC-Tags, Befehls- und Scanergebnisse |

Die Chiptemperatur misst keinen Raumwert. Der Mikrofonpegel ist dBFS und kein
kalibrierter Schalldruckpegel. Rohdaten des Mikrofons werden nicht übertragen.
Fehlende oder laut Firmware veraltete Messwerte erscheinen nicht als Null.
Nach 45 Sekunden ohne Status und sofort bei erkanntem MQTT-/Geräte-Ausfall wird
das Gateway nicht verfügbar. Schalter und Licht verwenden gemeldete Zustände.
Hardwarezustände und Funktionsschalter werden auf dem Board nicht automatisch
über Neustarts hinweg wiederhergestellt.

## Aktionen in Home Assistant

Unter **Entwicklerwerkzeuge → Aktionen** stehen sieben eigene Aktionen mit
deutschen Feldern zur Verfügung. `device_id` ist die Home-Assistant-Geräte-ID;
in der grafischen Ansicht wird das LilyGO-Gerät ausgewählt.

### Fernbedienung anlernen

```yaml
action: lilygo_t_embed.learn_code
data:
  device_id: DEINE_HA_GERAETE_ID
  slot: 0
  name: Steckdose AN
  code_type: rc
```

Dann innerhalb von 15 Sekunden die Taste drücken. `code_type` ist `rc`, `raw`
oder `ir`; IR verwendet Plätze 0–7, Funk Plätze 0–15. Erfolgreiches Lernen
ersetzt den gewählten Platz. Eine Startbestätigung bedeutet nur, dass das
Lernfenster geöffnet wurde. Erfolg bzw. Zeitüberschreitung erscheinen als
Ereignis. Für Steckdosen AN und AUS getrennte Plätze anlernen.

### Gespeicherten Code senden

```yaml
action: lilygo_t_embed.send_code
data:
  device_id: DEINE_HA_GERAETE_ID
  kind: rf
  slot: 0
  repeats: 6
```

Alternativ die Sendetaste des Codeplatzes nutzen. Funk erlaubt 1–10
Wiederholungen; IR erlaubt 0–3 zusätzliche Wiederholungen. Die Firmware begrenzt
Sendedauer, Frequenzen und Abstände weiterhin. Eine Sendebestätigung bestätigt
keinen tatsächlichen Schaltzustand der Steckdose.

### Bekannten Code eintragen

```yaml
action: lilygo_t_embed.store_code
data:
  device_id: DEINE_HA_GERAETE_ID
  kind: rf
  record:
    slot: 0
    name: Steckdose AN
    kind: rc
    frequency_mhz: 433.92
    code: '5393'
    bits: 24
    protocol: 1
    pulse_us: 350
```

RAW- und IR-Objekte entsprechen dem Format in `docs/PROTOCOL.md`.
`delete_code` löscht einen gewählten Platz dauerhaft. Codes werden auf dem
LilyGO gespeichert; die Integration liest ihre Liste nach Verbindung und
Änderungen neu ein. Mit der Taste **Status und Codes aktualisieren** erneut laden.

### Frequenz und Scan

```yaml
action: lilygo_t_embed.scan
data:
  device_id: DEINE_HA_GERAETE_ID
  band: '433'
```

`band` akzeptiert `433`, `868`, `all`. Alternativ alle drei Felder `start_mhz`,
`end_mhz`, `step_mhz` angeben, ohne `band`. Mit `tune` und `frequency_mhz` wird
die Empfangsfrequenz eingestellt. Der Scan-Sensor zeigt den letzten Messpunkt
oder das Abschlussresultat. Die Integration speichert keine vollständigen
Spektren. Der CC1101 liefert RSSI-Messungen und keine SDR-/IQ-Daten.

### Weitere Boardfunktionen

Die Aktion `execute` deckt die übrigen dokumentierten Firmwareaktionen ab,
z. B. `nrf_config`, `nrf_send`, `feature_set`, `backup`, `stop`, `list`, `ir_list`.
Das Feld `parameters` enthält nur deren Parameter. `session`, `request_id` und
unbekannte Felder lassen sich darüber nicht einschleusen.

```yaml
action: lilygo_t_embed.execute
data:
  device_id: DEINE_HA_GERAETE_ID
  action: nrf_config
  parameters:
    channel: 76
    address_hex: E7E7E7E7E7
```

Die nRF24-Gegenstelle muss zu Kanal, Adresse und weiteren festgelegten
Paketparametern passen. Das ist keine allgemeine Bluetooth-/WLAN-Decodierung.

## Automationen

Die Ereignisentitäten eignen sich für Zustandsauslöser. Für gezielte Filter
wird außerdem `lilygo_t_embed_event` auf dem HA-Ereignisbus veröffentlicht:

```yaml
alias: Klingel erkannt
triggers:
  - trigger: event
    event_type: lilygo_t_embed_event
    event_data:
      device_id: DEINE_HA_GERAETE_ID
      event_type: remote_pressed
      kind: rf
      slot: 2
actions:
  - action: persistent_notification.create
    data:
      message: Die angelernte Klingel wurde empfangen.
```

Eingaben heißen `encoder_left`, `encoder_right`, `encoder_click`, `encoder_hold`,
`side_click`, `side_hold`. Weitere Ereignisse sind `learned`, `ir_learned`,
`received`, `ir_received`, `nrf_received`, `tag_scanned`, `result`, `scan_finished`
und `nrf_energy_scan`. Letzteres enthält `active_channels`. NFC-UIDs werden
zusätzlich der eingebauten HA-Tag-Integration gemeldet; sichtbar unter
**Einstellungen → Tags**. Keine retained Ereignisse werden erneut ausgelöst.

## Befehlsbestätigungen und Diagnose

Jeder Befehl nutzt die aktuelle MQTT-Session und eine zufällige `request_id`.
Gesendet wird mit QoS 0 und `retain: false`. Es gibt keine automatische
Wiederholung oder Sendewarteschlange. Ein weiterer gleichzeitiger Befehl wird
abgelehnt. Bei fehlender Bestätigung nach acht Sekunden ist das Ergebnis
unbekannt; keine Wiederholung erfolgt. Vor einer weiteren Aktion muss wieder
ein aktueller Status eintreffen. `status` und `list` liefern Daten statt einer
separaten Ergebnisbestätigung.

Diagnose-Downloads enthalten eine Positivliste technischer Zustände. Sie
enthalten keine Broker-Zugangsdaten, Sessions, MQTT-Topicnamen, Fernbedienungs-
Codes, Pulsfolgen, NFC-UIDs oder nRF24-Nutzdaten. Automationsereignisse können
UIDs bzw. nRF24-Nutzdaten enthalten und werden entsprechend von HA verarbeitet.

## Über HACS installieren

Bei öffentlichem Repository:

1. In HACS das Menü für **Benutzerdefinierte Repositories** öffnen.
2. `https://github.com/kasi09/Lilygo_CC1101_Plus` eintragen und Typ
   **Integration** auswählen.
3. **LILYGO T-Embed CC1101 Plus** herunterladen und Home Assistant neu starten.
4. Die Integration unter **Einstellungen → Geräte & Dienste** hinzufügen.

Die Komponente wird als benutzerdefiniertes Repository installiert. Sie ist
nicht in den allgemeinen HACS-Katalog aufgenommen. Dessen Veröffentlichungs-
und Branding-Anforderungen gelten zusätzlich für eine spätere Katalogaufnahme.
Bei einem privaten Repository die oben beschriebene manuelle Installation nutzen.

## Paket und Repository-Metadaten

Das Manifest enthält die Dokumentations- und Issue-Links des Repositorys und
`@kasi09` als Codeowner. Ein Installations-ZIP bauen:

```powershell
python tools/package_hacs.py
```

Für einen Fork lassen sich die Metadaten auf ein anderes Repository anpassen:

```powershell
python tools/package_hacs.py --repository https://github.com/DEIN_NAME/DEIN_REPO --owner "@DEIN_NAME"
```

Das Werkzeug passt nur lokale Dateien an und baut das ZIP neu. Es legt weder
ein Repository an noch lädt es Daten hoch.

Offizielle Grundlagen: [HACS-Struktur und Manifest](https://www.hacs.dev/docs/publish/integration/),
[Home Assistant MQTT](https://www.home-assistant.io/integrations/mqtt/).

## Entwicklung und Prüfumfang

Python 3.14.2 oder neuer verwenden. Auf Linux:

```sh
python -m venv .venv
.venv/bin/python -m pip install -r requirements-test.txt
.venv/bin/python -m pytest
.venv/bin/python -m ruff check custom_components tests tools/package_hacs.py
python tools/package_hacs.py
```

Protokolltests benötigen nur pytest und pytest-asyncio und laufen auch unter
Windows. Home Assistant wird unter Windows nicht als Produktivsystem betrieben.
Tests mit simuliertem MQTT-Verkehr prüfen die Python-Anbindung; ein echter
HAOS-/Broker-/Funk-End-to-End-Test bleibt separat erforderlich.

Lokaler Prüfstand vom **20.10.2026**: **57 pytest-Prüfungen erfolgreich**, mit
Home-Assistant-Klassen aus 2026.10.0 und Python 3.14.6. Geprüft wurden unter
anderem 74 Entitäten, Aktionen, Konfigurationsdialog, doppelte Topics, NFC-
Weiterleitung, Abmelden der MQTT-Callbacks, Statusablauf, Sessionwechsel,
Zeitüberschreitungen, Codeplatz-Updates und Inhalt des Installationsarchivs.
MQTT und die Weiterleitung des Plattform-Setups werden in diesen Tests simuliert.
Zusätzlich bestanden die sechs bestehenden USB-Werkzeugtests und Ruff.

Für die optionalen Browser-Regressionsprüfungen des Einrichtungsportals:

```sh
python -m pip install -r requirements-browser-test.txt
python -m playwright install chromium
python -m pytest tests/test_setup_portal_browser.py
```

Unter Windows verwenden die Tests standardmäßig das installierte Google Chrome
mit einem getrennten Testprofil. `PORTAL_BROWSER_CHANNEL` kann einen anderen
installierten Playwright-Browserkanal vorgeben. Die Tests verwenden künstliche
WLAN-Namen und keine persönlichen Browserdaten. Ohne die optionale Playwright-
Abhängigkeit wird das Browser-Testmodul übersprungen.
Firmware 0.5.3 wurde zusätzlich am verbundenen Lilygo mit echten WLAN-Treffern
und der tatsächlich erzeugten HTML-Seite geprüft. Die Standard-USB-Diagnose
`network-info` zeigt Scan-Zähler und Verbindungsflags ohne Zugangsdaten.
Die einzige pytest-Warnung stammt aus der HTTP-Klasse von Home Assistant.
Firmware 0.5.0 wurde mit den bisherigen 54 und 20 neuen Netzwerkprüfungen
erfolgreich gebaut und am 20.10.2026 auf COM4 geflasht. Sie ergänzt den
passwortgeschützten Einrichtungs-Hotspot; die Komponente unterstützt weiterhin
auch Firmware 0.4.0. Der Test mit einem echten HAOS-Broker steht separat aus.
