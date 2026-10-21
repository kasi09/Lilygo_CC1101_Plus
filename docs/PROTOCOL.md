# USB- und MQTT-Protokoll v0.5

Firmware 0.5.2 korrigiert die Portal-Antwort auf `Referrer-Policy: same-origin`.
Mit dem vorherigen `no-referrer` setzen Browser bei normalen Formular-POSTs
`Origin: null`, was die Herkunftsprüfung zurückweist. Der zufällige Formular-
Sitzungsschlüssel und die Herkunftsprüfung bleiben erforderlich.

WLAN-Suche ausschließlich auf dem geöffneten Einrichtungs-Hotspot:

- `POST /scan` mit URL-kodiertem `token`: startet eine asynchrone Suche oder
  verwendet eine bereits laufende Suche; Antwort 202 mit `scanning:true`.
- `GET /networks`: liefert währenddessen `scanning:true`, danach
  `scanning:false` und `networks` mit bis zu 24 Einträgen aus `ssid`, `rssi`
  und `secure`. Doppelte Namen werden zusammengefasst, stärkster Eintrag zuerst.
- Fehler beim Start: 403 bei ungültiger Sitzung, 503 bei momentan nicht
  möglicher Suche. Abgebrochene oder fehlgeschlagene Suchen melden `scan_failed`.

Die Seite startet die Suche beim Öffnen selbst und bietet eine manuelle
Wiederholung. Nach dem Speichern wartet sie zunächst auf die Übernahme der
Einstellungen. Neue Stations-Verbindungsversuche und Profilübernahme warten,
bis eine bereits laufende WLAN-Suche beendet ist. RF-Lernen und RF-Scans
behalten Vorrang vor der Bearbeitung von Portal-Anfragen.
WLAN-Suchergebnisse erscheinen weder im normalen Gerätestatus noch über MQTT.

Grundlage der Browser-Korrektur: [Referrer-Policy und Origin](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Headers/Referrer-Policy#effect_on_the_origin_header).

Firmware 0.5.1 ergänzt den lokal berechneten WLAN-QR-Code auf dem Display.
`{"action":"display_info"}` liefert ausschließlich über USB die logische
Displaygröße, Panelgröße, Rotation, ST7789-Offsets, Puffergröße und
`right_edge_in_view`, `right_header_drawn` sowie `qr_ready`. Die Angaben enthalten
keine Zugangsdaten. `right_header_drawn` prüft Textpixel im rechten Bildpuffer.
Für die Plus-Variante: Panel 170×320, Querformat 320×170, Rotation 3,
Offset x=0/y=35, Bildpuffer und Zeichenbereich jeweils 320×170.

Zur lokalen QR-Diagnose kann `setup_info` optional `"qr":true` enthalten.
Die zusätzlichen Felder `qr_modules` und `qr_hex` beschreiben den zuletzt
gezeichneten WLAN-QR-Code, sofern die Einrichtungsseite bereits gerendert wurde.
`qr_hex` ist das hexadezimale Nayuki-Format: erstes Byte ist die Modulanzahl,
danach Modulbits zeilenweise, jeweils niederwertigstes Bit zuerst.
`qr_render_ok` prüft bei vorhandenem Bildpuffer alle 123×123 Pixel einschließlich
des weißen Randes gegen den QR-Inhalt. Das prüft die Grafik im Speicher, nicht
die physische Anzeige des Panels.
Diese Ausgabe enthält wie das Hotspot-Passwort temporäre Zugangsdaten und darf
nicht veröffentlicht werden. QR-Daten werden weder über MQTT noch im normalen
Status ausgegeben. Die Diagnosebefehle werden über MQTT mit `usb_only` abgelehnt.

Firmware 0.5.0 ergänzt einen passwortgeschützten Einrichtungs-Hotspot und
dauerhafte WLAN-/MQTT-Einstellungen. Neue Statusfelder:

| Feld | Bedeutung |
| --- | --- |
| `wifi_connected`, `mqtt_connected` | Aktueller Verbindungszustand |
| `setup_active` | Einrichtungs-Hotspot geöffnet |
| `network_config_saved` | Gültiges Netzwerkprofil im NVS gespeichert |
| `network_storage_ok` | Netzwerk-Einstellungsspeicher verfügbar |

Nur über **lokales USB**, nie über MQTT:

```json
{"action":"network_setup"}
{"action":"network_setup","enabled":false}
{"action":"setup_info"}
```

`network_setup` öffnet den Hotspot, sofern kein Funk-/IR-Lernen oder Scan läuft.
Mit `enabled:false` wird er wie per Display geschlossen; `enabled` ist optional
und muss, falls vorhanden, ein Boolean sein.
`setup_info` liefert bei aktivem Hotspot einmalig Name, temporäres Hotspot-
Passwort und URL direkt auf USB. Die Antwort wird nicht über MQTT veröffentlicht.
Es werden dabei niemals gespeicherte Heim-WLAN- oder MQTT-Passwörter ausgegeben.
USB-Mitschnitte dieses Befehls enthalten das aktuelle Hotspot-Passwort und
gehören nicht ins Repository. Normaler `status` enthält keine Zugangsdaten.
Das WLAN-/MQTT-Profil ist getrennt von Funk-/IR-Codes gespeichert und wird durch
die JSONL-Codesicherung nicht exportiert.

Ab Firmware 0.4.0 enthält `state` zusätzlich `discovery_enabled` (Boolean).
`GW_MQTT_DISCOVERY false` schaltet die Veröffentlichung der bisherigen
MQTT-Discovery-Konfigurationen für die eigene `lilygo_t_embed`-Komponente ab.
Vorhandene retained Konfigurationen werden damit nicht automatisch gelöscht.

Befehle dürfen eine optionale `request_id` aus maximal 32 kleinen Hex-Zeichen
mitführen. Unmittelbare `result`-Antworten spiegeln diese Kennung zurück.
Spätere Ereignisse, Lernzeitüberschreitungen und physische Bedienaktionen
übernehmen sie nicht. `status` und `list` antworten weiterhin mit ihren Daten.
Die Kennung ergänzt die Session, ersetzt sie aber nicht.

USB: 115200 Baud, UTF-8, **ein JSON-Objekt pro Zeile**, abgeschlossen mit LF.
Eingaben sind auf 4096 Bytes begrenzt. Antworten sind ebenfalls JSON-Zeilen.
Logs der ESP32-Laufzeit beim Booten können zusätzlich als Klartext erscheinen.

MQTT-Standardbasis: `lilygo/lilygo_rf`.

| Topic relativ zur Basis | Inhalt | Retained |
| --- | --- | --- |
| `command` | Befehlsobjekt inklusive aktueller `session` | niemals verwenden |
| `state` | Zustand, Zähler und aktuelle MQTT-Session | nein |
| `availability` | `online` / `offline` | ja |
| `events` | Ergebnis, Empfang oder angelernter Code | nein |
| `remotes` | Ein Datensatz je Speicherplatz nach `list` | nein |
| `remote/0/event` bis `remote/15/event` | `press` für passende RC-Codes | nein |
| `scan` | Scan-Messpunkte und Abschlussmeldung | nein |
| `nfc/tag` | `tag_scanned` mit `uid`, HA-MQTT-Tag-Scanner | nein |
| `ir/events` | `ir_received` mit Protokoll/Code-Identität | nein |
| `ir/remotes` | Acht Datensätze nach `ir_list` | nein |
| `ir/remote/0/event` bis `ir/remote/7/event` | `press` bei wiedererkanntem IR-Code | nein |
| `nrf/events` | `nrf_received`, Kanal und Hex-Payload | nein |
| `nrf/scan` | `nrf_energy_scan` mit 84 Kanal-/Boolean-Paaren | nein |
| `input` | `encoder_left/right/click/hold`, `side_click/hold` | nein |
| `light/set` | HA-JSON-Lichtbefehl, als einzige Ausnahme ohne Session | nein |
| `light/state` | HA-JSON-Lichtzustand | ja |

Ab Firmware 0.2 enthält `state` zusätzlich:

| Feld | Bedeutung |
| --- | --- |
| `battery_pct` | Vom Gauge geschätzter Ladestand in % |
| `battery_voltage_v` | Akkuspannung in V |
| `battery_current_ma` | Vorzeichenbehafteter Akkustrom in mA |
| `gauge_temperature_c` | Interne BQ27220-Chiptemperatur in °C, keine Raumtemperatur |
| `battery_available` | Akku-Prozent und Spannung sind aktuell und plausibel |
| `current_available` | Akkustrom ist aktuell auslesbar |
| `chip_temperature_available` | Interne Chiptemperatur ist aktuell auslesbar |
| `gauge_reachable` | Der Gauge hat bei der letzten aktuellen Abfrage geantwortet |

Nicht verfügbare Messwerte sind `null`; Daten über 15 s Alter werden nicht
weiter als gültige Messungen veröffentlicht. Das normale I²C-Abfrageintervall
beträgt 5 s; bei Lernen oder Scan wird es pausiert. Statusmeldungen erscheinen
weiterhin alle 15 s oder nach Befehlen; Änderungen der Zusatzfunktionen werden
höchstens einmal pro Sekunde zusammengefasst. Die vier Akku-
MQTT-Discovery-Sensoren prüfen sowohl Gateway- als auch Messwertverfügbarkeit.

Funk-`slot` ist eine Ganzzahl von 0 bis 15, IR-`slot` von 0 bis 7. Alle Beispiele gelten direkt für
USB. Bei MQTT **zusätzlich** `"session":"<Wert aus state>"` setzen. Der Wert
wechselt bei jeder MQTT-Verbindung. Home Assistant Discovery setzt ihn in
Sendetasten automatisch ein. Diese Session verhindert veraltete Befehle; sie
ist kein Authentifizierungsgeheimnis. Ein `result` mit `ok:true` bestätigt das
Ausführen des Firmwarebefehls, nicht den Empfang durch das Funkgerät.

## Lesen und Stoppen

```json
{"action":"status"}
{"action":"list"}
{"action":"stop"}
```

`list` erzeugt 16 einzelne `remote`-Objekte einschließlich leerer Plätze und
der vollständigen RAW-Pulsfolgen. Die Objekte lassen sich mit `action:store`
wieder importieren. Nicht als ein einziges Array senden.

## Lernen

```json
{"action":"learn","slot":0,"name":"Steckdose AN","kind":"rc"}
{"action":"learn","slot":1,"name":"Klingel RAW","kind":"raw"}
```

Namen: maximal 32 UTF-8-Bytes. Erfolgreiches Anlernen ersetzt den Zielplatz.
Nach spätestens 15 s kommt `learn_timeout`. Die aktuell eingestellte
Empfangsfrequenz wird mitgespeichert. Ein `learned`-Objekt enthält das Ergebnis.
RC-Erkennung und RAW-Aufnahme laufen als getrennte Modi.

## Bekanntes Telegramm speichern

```json
{"action":"store","slot":0,"name":"Steckdose AN","kind":"rc","frequency_mhz":433.92,"code":"5393","bits":24,"protocol":1,"pulse_us":350}
```

`code` ist eine vorzeichenlose 32-Bit-Zahl oder ihre Dezimaldarstellung als
String. Die Stringdarstellung wird beim Export verwendet. Erlaubt sind
1–32 Bit, Protokoll 1–12 und Basispulse von 80–1500 µs. Der Zahlenwert muss in
die Bitlänge passen. Die Protokollnummern beziehen sich auf rc-switch 2.6.4.

RAW-Import verwendet `kind:raw`, `first_high` (Boolean), `gap_us` (8000–40000)
und `pulses_us` mit 16–256 Ganzzahlen. Jede Dauer muss zwischen 80 und 40000 µs
liegen, der Rahmen inklusive Pause darf höchstens 500000 µs lang sein.
Die Pegel wechseln nach jedem Puls; `first_high` bestimmt den ersten Pegel.
`gap_us` beschreibt den darauf folgenden Separatorpegel. Die Aufnahme hat
keine automatische Protokoll-, Prüfsummen- oder Rolling-Code-Erkennung.

## Senden und Löschen

```json
{"action":"send","slot":0,"repeats":6}
{"action":"delete","slot":0}
```

1–10 Wiederholungen, Standard 6; zusätzlich höchstens 500 ms geplante
Gesamtdauer. Fehler `tx_airtime_exceeded` mit weniger Wiederholungen beheben.
`tx_cooldown` bedeutet, dass der vorherige Sendevorgang weniger als eine Sekunde
zurückliegt. Senden auf Frequenzen außerhalb 433,05–434,79 MHz wird abgewiesen.
Der Compiler-Schalter `GW_TX_ENABLED=false` deaktiviert das Senden vollständig.
Das gilt für CC1101, IR und nRF24; RGB und Signalton bleiben bedienbar.

## Frequenz und Scans

```json
{"action":"tune","frequency_mhz":433.92}
{"action":"scan","band":"433"}
{"action":"scan","band":"868"}
{"action":"scan","band":"all"}
{"action":"scan","start_mhz":433.05,"end_mhz":434.79,"step_mhz":0.05}
```

Individuelle Scans müssen vollständig in einem der drei gültigen CC1101-
Bereiche liegen. Schrittweite 0,025–2 MHz, höchstens 2001 Messpunkte.
`all` scannt die drei Bereiche nacheinander mit 0,5-MHz-Schritten. Pro Schritt
werden mindestens 20 ms Einschwingzeit abgewartet. Schritte sind gleichmäßig;
der letzte Punkt kann unter `end_mhz` liegen. RSSI ist ein orientierender
Empfängerpegel, keine kalibrierte Spektrumanalysatormessung.

Währenddessen kommen `scan_sample`-Objekte mit `frequency_mhz` und `rssi_dbm`.
`scan_finished` enthält Punktzahl, stärksten gemessenen Punkt und `ok`.
`stop` bricht ab; bei regulärem Ende wird die vorherige Empfangsfrequenz
wiederhergestellt. Ein Scan erkennt keine Geräte und speichert keine Codes.

## IR-Fernbedienungen

```json
{"action":"ir_list"}
{"action":"ir_learn","slot":0,"name":"TV Power"}
{"action":"ir_send","slot":0,"repeats":0}
{"action":"ir_delete","slot":0}
```

15 s Lernzeit, maximal 32 UTF-8-Bytes pro Name. Bekannte Protokolle bis 64 Bit
ohne langen Klimageräte-Zustand werden mit Protokollnummer aus
IRremoteESP8266 2.8.6 und Hex-Code gespeichert. Zusätzlich wird, soweit passend,
die RAW-Folge erfasst. Unbekannte Signale werden als RAW gespeichert:
8–128 Pulse, jeweils 20–65535 µs und insgesamt höchstens 500 ms.
Standardträger ist 38 kHz; er wird vom Empfänger nicht gemessen.

Ein `ir_remote`- oder `ir_learned`-Datensatz lässt sich mit `action:ir_store`
wieder importieren. Felder: `slot`, `name`, `decoded`, `carrier_khz` (30–60),
`pulses_us`. Bei `decoded:true` zusätzlich `protocol`, `bits`, `code_hex`
(1–16 Hex-Ziffern, optional `0x`). Der Code muss in seine Bitlänge passen.
`repeats` bezeichnet zusätzliche Wiederholungen (0–3), Standard 0; die geplante
Sendedauer wird auf 500 ms begrenzt, Abstand mindestens 500 ms. Nicht unterstützte
Sendeprotokolle liefern einen Fehler, sofern auch keine RAW-Wiedergabe möglich ist.

Wiedererkannte gespeicherte IR-Codes erzeugen `press`; RAW-Vergleiche verwenden
25 % bzw. mindestens 100 µs Toleranz je Puls. Wiederholungen werden unterdrückt.
Der Schalter `ir` betrifft den Empfang; gespeicherte IR-Codes bleiben sendbar.

## Funktionsschalter, Licht und Signalton

```json
{"action":"feature_set","feature":"microphone","enabled":true}
{"action":"feature_set","feature":"nfc","enabled":false}
{"action":"feature_set","feature":"ir","enabled":true}
{"action":"feature_set","feature":"nrf","enabled":true}
{"action":"screen_set","enabled":false}
{"action":"light_set","state":"ON","brightness":32,"color":{"r":0,"g":180,"b":255},"effect":"Statisch"}
{"action":"beep","frequency_hz":880,"duration_ms":150,"volume":20}
{"action":"probe_features"}
{"action":"backup"}
```

Licht: Helligkeit und RGB-Komponenten 0–255; Effekt `Statisch` oder `Status`.
Status färbt den Ring bei WLAN-Verbindung grün, sonst orange. Auf `light/set`
das gleiche Objekt **ohne** `action` und `session` senden. Dieses getrennte Topic
kann nur Licht verändern. Sonstige MQTT-Befehle brauchen weiterhin die Session.
Signalton: 200–4000 Hz, 20–300 ms, Lautstärkewert 0–50, keine kalibrierte Lautstärke.

Mikrofon und nRF24 starten aus, Licht aus, Display/NFC/IR-Empfang an.
Laufzeitschalter werden nicht dauerhaft gespeichert. `probe_features` prüft NFC,
nRF24 und SD erneut. SD wird nur eingebunden, niemals formatiert.
`backup` schreibt belegte Funk-/IR-Plätze als JSONL nach `/lilygo/backup.jsonl`.
Die vorherige Sicherung bleibt in `backup.prev.jsonl`; Zugangsdaten werden nicht
gesichert. Wiederherstellung über `tools/gateway.py restore --file ...`.

Während Funk-Lernen, CC1101-Scan oder IR-Lernen liefern weitere aktive
Zusatzbefehle `device_busy`. `status`, `list`, `ir_list` und `stop` bleiben möglich.
`stop` beendet auch IR-Lernen. Das getrennte Licht-Topic kann seinen Sollzustand
setzen, die LED-Ausgabe wartet bis zum Ende des Lernens/Scans.

## nRF24-Paketbrücke

```json
{"action":"nrf_config","channel":76,"address_hex":"4841303031"}
{"action":"feature_set","feature":"nrf","enabled":true}
{"action":"nrf_send","payload_hex":"01020304"}
{"action":"nrf_scan"}
```

Kanal 0–83 (2400 MHz + Kanal), genau fünf Adressbytes als zehn Hex-Ziffern,
Payload 1–32 Bytes als zusammenhängender Hex-Text. Fest eingestellt:
1 Mbit/s, dynamische Payloads, CRC16, kein Auto-ACK und niedrige Sendeleistung.
Gegenstellen müssen identisch konfiguriert sein. Ein Sendeerfolg ist keine
Empfangsbestätigung. Live-Ereignisse sind auf zehn pro Sekunde begrenzt;
`nrf_event_dropped` zählt unterdrückte Ereignisse, `nrf_count` alle gelesenen Pakete.
`nrf_scan` ist ein empfangender RPD-Schwellwerttest, keine RSSI-Messung oder
Protokollerkennung. Er unterbricht kurz den Paketempfang und stellt den
konfigurierten Kanal anschließend wieder her.

## Weitere Statusfelder

| Felder | Bedeutung |
| --- | --- |
| `charger_available`, `usb_power`, `charging`, `charge_status` | BQ25896 erreichbar; externe Versorgung; Laden; `idle/precharge/charging/done` |
| `nfc_available`, `nfc_enabled`, `nfc_uid`, `nfc_count` | PN532-Erreichbarkeit, Schalter, letzte UID, Zähler |
| `ir_available`, `ir_enabled`, `ir_last_code`, `ir_count` | IR-Treiber aktivierbar (keine physische Selbstprüfung), Schalter, letzter Code, Zähler |
| `nrf_available`, `nrf_enabled`, `nrf_last_payload`, `nrf_count` | nRF24 erkannt, Empfangsschalter, Hex-Payload, Zähler |
| `nrf_channel`, `nrf_address_hex`, `nrf_event_dropped` | Paketkonfiguration und unterdrückte MQTT-/USB-Ereignisse |
| `microphone_enabled`, `noise_available`, `noise_dbfs` | Mikrofon aktiv; aktueller relativer Pegel (keine dB-SPL-Messung) |
| `screen_on`, `rgb_on` | Anzeige-/Lichtzustand |
| `sd_mounted`, `sd_total_mb`, `sd_used_mb` | FAT-Karte eingebunden; Kapazität/belegt in MiB |
| `free_heap_bytes`, `uptime_s` | Freier Heap und Betriebszeit |

Mikrofonpegel entstehen aus kurzen PCM-Blöcken mit entferntem Gleichanteil,
16 kHz / 16 Bit, etwa einmal pro Sekunde. Keine Aufnahme oder Übertragung der
Audiodaten. Pegel über drei Sekunden Alter bzw. bei pausiertem Mikrofon werden
als nicht verfügbar gemeldet. NFC liefert ISO14443A-UIDs, keine Kartengeheimnisse.
Eine dauerhaft aufgelegte gleiche Karte erzeugt nicht fortlaufend neue Auslöser.

Seit 0.3.1 erfolgen NFC-Bestätigung und Tag-Antwort in getrennten IRQ-Phasen;
die Konfigurationsantwort wird vorher vollständig gelesen. Ungültige oder
unvollständige Antworten erzeugen `nfc_io_error` und setzen `nfc_available:false`.
`probe_features` bzw. erneutes Aktivieren des NFC-Lesers initialisiert ihn neu.
