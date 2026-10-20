# Abnahme am LilyGO T-Embed CC1101 Plus

Die vollständige Abnahme unten ist **noch offen**. Ein erfolgreicher Build
bestätigt nur die Übersetzbarkeit des Quellcodes.

## Display und WLAN-QR-Code, Firmware 0.5.1, 20.10.2026

Firmware 0.5.1 wurde erfolgreich gebaut: statischer RAM 64.712 Bytes,
Anwendung 1.151.949 Bytes, Firmware-Abbild 1.152.320 Bytes. Die 74 bisherigen
Compilerprüfungen und drei neuen Display-Grenzprüfungen liefen erfolgreich.
57 Home-Assistant-Tests, sechs USB-Werkzeugtests und Ruff bestanden ebenfalls.

Die Display-Initialisierung folgt jetzt auf die anderen SPI-Teilnehmer und
verwendet Rotation 3 aus dem Herstellerbeispiel. Der Bildpuffer übernimmt die
logische Panelgröße; vor jedem Bild wird der vollständige Zeichenbereich
wiederhergestellt. `display_info` macht Größe, Offset und rechten Zeichenrand
über USB prüfbar. Das Layout bleibt für 320×170 Pixel ausgelegt.

Die Einrichtungsseite zeigt einen lokal berechneten WLAN-QR-Code mit
ganzzahligen Modulen und freiem weißen Rand. Name, Passwort und Browser-Adresse
stehen daneben. Die QR-Bibliothek ist Nayuki 1.8.0 unter MIT-Lizenz. Nach
Schließen oder Verlassen der Seite wird der QR-Puffer verworfen; beim erneuten
Öffnen wird er mit dem neuen temporären Hotspot-Passwort neu berechnet.

Nach erneutem Anschließen wurde 0.5.1 auf COM4 geflasht; Esptool bestätigte den
Hash und startete die Anwendung per Watchdog. Der anschließende USB-Test bestand:

- ST7789, Panel 170×320, Querformat 320×170, Rotation 3, Offset x=0/y=35.
- Zeichenbereich und Bildpuffer 320×170; rechter unterer Pixel im Zeichenbereich
  und Textpixel in der rechten Statusanzeige vorhanden.
- WLAN-QR-Code mit unabhängigem ZXing-Decoder ausgelesen: Er enthält genau
  den aktuellen temporären Hotspot und dessen Passwort.
- Alle 15.129 Pixel der QR-Grafik im Bildpuffer einschließlich weißem Rand
  stimmen mit dem erzeugten QR-Code überein.
- Schließen entfernt QR-Puffer und Zugangsdaten-Ausgabe; erneutes Öffnen erzeugt
  ein neues Passwort und einen dazu passenden, erneut decodierbaren QR-Code.
- Zehn erfolgreiche Statusabfragen mit aktivem Hotspot, Codespeicher und
  Netzwerkspeicher verfügbar, Funkfehler 0, NFC und Akkuwerte vorhanden,
  keine Panics oder I²C-Fehler. Freier Heap: 258.524 Bytes.

Die Pixelprüfung betrifft den Bildpuffer. Die physisch vollständige Anzeige und
das Scannen mit einer Handykamera sind noch am Display zu bestätigen.
Es wurden keine aktuellen QR-Codes oder Zugangsdaten ins Repository übernommen.

## Einrichtungs-Hotspot, Firmware 0.5.0, 20.10.2026

Firmware 0.5.0 wurde erfolgreich gebaut und auf COM4 geflasht. Esptool bestätigte
die übertragenen Daten per Hash und startete die Anwendung per Watchdog.
Statischer RAM: 64.536 Bytes; Anwendung: 1.139.505 Bytes; Firmware-Abbild:
1.139.872 Bytes. Die 54 bisherigen und 20 neuen Netzwerk-Compilerprüfungen
liefen erfolgreich. Die 57 Home-Assistant-Tests und sechs USB-Werkzeugtests
bestanden ebenfalls; Firmware 0.5.0 wurde als kompatibel geprüft.

Über USB geprüft:

- Start als 0.5.0 mit automatisch aktivem Einrichtungs-Hotspot und verfügbarem
  NVS-Netzwerkspeicher.
- Temporäre Hotspot-Zugangsdaten nur auf ausdrückliche lokale USB-Anfrage;
  Passwortlänge zwölf Zeichen, korrekte Einrichtungs-URL.
- Schließen beendet die Ausgabe der Zugangsdaten; erneutes Öffnen erzeugt
  ein neues Passwort. Der Test hinterließ den Hotspot geöffnet.
- `request_id` wird korrekt bestätigt und nicht in folgende Antworten übernommen.
- Zehn aufeinanderfolgende Statusabfragen bei aktivem Hotspot: Codespeicher
  verfügbar, Funkfehler 0, PN532 und Akku-IC erreichbar, Akkuwerte vorhanden,
  ansteigende Betriebszeit und keine I²C-Zeitüberschreitungen oder Panics.
  Freier Heap am Ende: 258.716 Bytes.
- Normale Statusobjekte enthalten keine WLAN-/MQTT-/Hotspot-Passwörter.

Ein WLAN-Scan vom Laptop wurde vom Betriebssystem abgewiesen; deshalb sind
Funk-Sichtbarkeit, Verbindung eines externen Clients, Captive-Portal-Verhalten
und das tatsächliche Speichern über HTTP noch mit Handy oder Laptop zu prüfen.
Es wurden keine echten WLAN-/MQTT-Zugangsdaten angefordert oder protokolliert.
Ein Betrieb mit echtem HAOS-Broker steht weiterhin separat aus.
Die HTTP-Einrichtungsseite bindet ausschließlich an die Hotspot-IP; DNS und
Webserver werden beim Schließen gestoppt. Formulare nutzen einen zufälligen
Sitzungswert und prüfen den Ursprung. Einzelne HTTP-Anfragen können den normalen
Funkempfang verzögern; bei Lernen/Scan wird die HTTP-Bearbeitung pausiert.

## Eigene Home-Assistant-Komponente, 20.10.2026

Custom Component 0.1.0 liegt lokal unter `custom_components/lilygo_t_embed`.
Die Firmware 0.4.0 wurde für diese Anbindung erfolgreich gebaut: 49.500 Bytes
statischer RAM, 786.065 Bytes Flash. Sie ergänzt abschaltbare MQTT-Discovery,
das Statusfeld `discovery_enabled` und `request_id` in unmittelbaren Antworten.
Die bisherigen 54 Compilerprüfungen liefen mit durch.

Die neue Firmware wurde noch nicht übertragen: Bei der USB-Portabfrage am
20.10.2026 wurde kein angeschlossener serieller Port gefunden. Der letzte
bestätigte Flash-/Gerätetest bleibt damit 0.3.1. WLAN-/MQTT-Zugangsdaten sind
weiterhin nicht konfiguriert. Ein Test mit echtem HAOS, Broker und Gerät steht aus.
Die lokale Installation und Prüfkommandos stehen in `docs/HACS.md`.
57 lokale pytest-Prüfungen der Integration, sechs bestehende USB-Werkzeugtests
und Ruff waren erfolgreich. Dabei wurde MQTT simuliert; die Tests ersetzen
keinen Betrieb mit echtem Broker, Funkgerät und HAOS.

## Bisheriger USB-Starttest

Am 19.10.2026 wurde Version 0.3.0 auf COM4 geflasht. Esptool bestätigte die
Prüfsummen von Bootloader, Partitionstabelle, Startdaten und Anwendung.
Erkannt: ESP32-S3 Revision 0.2, 16 MB Quad-Flash und 8 MB integrierter PSRAM.
Die Plus-Boardrevision selbst wurde nicht visuell überprüft.

Der Firmwarestatus meldete `mode:receive`, `radio_error:0`, `storage_ok:true`,
433,92 MHz sowie erreichbare PN532- und nRF24-Treiber. Das ist noch kein
Nachweis für Fernbedienungsempfang, NFC-Tag-Lesen oder nRF24-Paketübertragung.
IR war als Treiber initialisiert; eine physische IR-Selbstprüfung gibt es nicht.
Eine SD-Karte war nicht eingebunden. Display und Tasten wurden nicht visuell geprüft.

Bei aktivem NFC traten I²C-Zeitüberschreitungen (Fehler 263) und fehlende
Akku-/Ladewerte auf. Kontrollversuch über USB: NFC ausgeschaltet, sechs Sekunden
gewartet, Status erneut gelesen. BQ27220 und BQ25896 waren dann erreichbar,
Akkuwerte verfügbar und im folgenden dreisekündigen Fenster keine Timeouts.
Nach erneutem Aktivieren von NFC traten binnen vier Sekunden zwei Timeouts auf.
Keine Ladeparameter oder Kalibrierungswerte wurden verändert.

Korrektur in 0.3.1: passive NFC-Suche mit separaten IRQ-Phasen für ACK und Antwort,
keine laufenden Statusabfragen während des Wartens auf einen Tag; Antwortlänge,
Prüfsumme und UID-Grenzen werden validiert. Die RFConfiguration-Antwort nach
`setPassiveActivationRetries` wird vollständig gelesen, bevor die Suche startet.
Sieben zusätzliche Compilerprüfungen. Das Handshake-Verfahren folgt Abschnitt
6.2.4 des [NXP-PN532-Handbuchs](https://www.nxp.com/docs/en/user-guide/141520.pdf).

Am 20.10.2026 wurde 0.3.1 erfolgreich gebaut und geflasht, mit bestätigten
Prüfsummen. Danach: 30,1 Sekunden USB-Beobachtung, 15 Statusantworten,
`nfc_enabled:true`, `nfc_available:true`, `gauge_reachable:true`,
`charger_available:true`, `battery_available:true`, `storage_ok:true` und
`radio_error:0`. Keine I²C-Zeitüberschreitungen, Panics oder Uptime-Rücksprünge.
Das lokale Ergebnis steht in `captures/post-flash-health.json`.

Der normale RTS-Neustart hatte das Gerät im Bootloader belassen. Ein
Watchdog-Neustart startete die Anwendung. Das Boardprofil verwendet deshalb
jetzt `upload.after_reset:watchdog_reset`. Mit dem vorhandenen Abbild wurden
Upload und automatischer Neustart erneut ausgeführt und per USB bestätigt:
Version 0.3.1, Empfangsmodus, NFC und Akku-/Lade-IC erreichbar, Funkfehler 0.
Protokolle: `build-upload.log` und `upload-reset-check.log` (lokal ignoriert).

WLAN/MQTT waren nicht eingerichtet. Echte Funk-/IR-Telegramme, NFC-Tags,
Mikrofon, Lautsprecher, RGB, SD-Sicherung und sichtbare Display-/Tastenfunktion
wurden nicht als funktional abgenommen. Bei der Beobachtung war kein NFC-Tag
erkannt worden; der Test bestätigt den Hintergrundbetrieb auf dem gemeinsamen Bus.

## Hardware abgleichen

Die Konfiguration basiert auf dem offiziellen T-Embed-CC1101-Projekt und dessen
Pinmap für V1.0. Die konkrete Plus-Revision und Bestückung müssen am Gerät
abgeglichen werden. Besonders prüfen:

| Funktion | GPIO |
| --- | --- |
| Versorgung externer 3,3-V-Domäne | 15 |
| SPI SCK / MOSI / MISO | 11 / 9 / 10 |
| CC1101 CS / GDO0 / GDO2 | 12 / 3 / 38 |
| Antennenpfad SW1 / SW0 | 47 / 48 |
| Display CS / DC / Backlight | 41 / 16 / 21 |
| SD-CS | 13 |
| nRF24 CS / CE, falls bestückt | 44 / 43 |
| Encoder A / B / Taste | 4 / 5 / 0 |
| Seitentaste | 6 |
| I²C SDA / SCL (Akku, Lade-IC, PN532) | 8 / 18 |
| PN532 Reset / IRQ | 45 / 17 |
| IR-Sender / -Empfänger | 2 / 1 |
| RGB-Ring (8 LEDs) | 14 |
| PDM-Mikrofon Clock / Data | 39 / 42 |
| I²S-Lautsprecher BCK / WS / Data | 46 / 40 / 7 |

Display: ST7789, 170×320 Pixel, quer dargestellt, ohne separaten Reset-Pin.
Die Firmware zieht alle SPI-CS-Leitungen vor der Initialisierung auf HIGH und
setzt den nRF24-CE-Pin auf LOW. Änderungen an Pinbelegung und TFT-Buildflags
müssen konsistent sein. CC1101, Display, SD und nRF24 teilen sich den SPI-Bus;
deren Transaktionen werden im Hauptloop nacheinander ausgeführt.

## Erste Inbetriebnahme

1. Vorhandene Firmware bei Bedarf separat sichern; USB-Port eindeutig zuordnen.
2. Zunächst ohne WLAN-Konfiguration bauen und bewusst auf das passende Board flashen.
3. USB-Status lesen: `mode:receive`, `radio_error:0`, `storage_ok:true` erwarten.
4. Display, Drehrichtung, Encoder-Taster und Seitentaste prüfen.
   Die Seitentaste muss zwischen Empfang, Codes, Lernen, Scanner, Eingängen,
   Infrarot, Bedienung und Gerätewerten wechseln. Auf Codes-/Lernseite wählt Drehen den Platz 0–15;
   auf der Scannerseite das Band. Die Fußzeile muss zur jeweiligen Aktion passen.
5. Über `tune` 433,92 MHz wählen; bekannte Fernbedienung mehrfach drücken.
   Code, Bitlänge und Protokoll müssen reproduzierbar sein.
6. AN und AUS auf getrennte Plätze lernen, `list` exportieren, neu starten,
   Speichererhalt prüfen. Name und Frequenz müssen erhalten bleiben.
7. Mit passender Antenne und eigenem Testgerät zuerst **eine** Wiederholung
   senden, danach bei Bedarf erhöhen. Funktion am Empfänger prüfen. Die
   JSON-Erfolgsmeldung allein bestätigt keinen Funkempfang.
8. Nach dem Senden erneut Empfang prüfen; GPIO3 muss wieder Eingang sein.

## Scan und RAW

1. 433-MHz-Scan ohne Sender und mit eigener Fernbedienung vergleichen.
2. `scan_finished.ok:true` und Rückkehr zu 433,92 MHz prüfen; danach muss
   normale Code-Erkennung wieder funktionieren.
3. Vollscan durchführen; die Lücken 348–387 und 464–779 MHz dürfen nicht
   abgestimmt werden. Reichweite/Empfindlichkeit außerhalb der abgestimmten
   Antennenbereiche separat beurteilen.
4. RAW mit einer bekannten OOK-Fernbedienung aufnehmen, exportieren und
   gegen ihre erwartete Pulsfolge prüfen; danach einmal wiedergeben.
5. RAW ohne Betätigung testen, um die Neigung zu Störungsaufnahmen in der
   konkreten Umgebung festzustellen. Überlange, verrauschte oder unpassende
   Aufnahmen verwerfen. Speicherplatz ggf. erneut lernen.
6. Lernen nach 15 s, `stop` während Lernen/Scan und Senden bei anderer
   Empfangsfrequenz testen. Es darf kein fortlaufender Träger zurückbleiben.
7. Lern-Countdown, RAW-Start durch langes Encoderdrücken und Abbruch mit
   beiden Tasten testen. Nach einem langen Tastendruck darf beim Loslassen
   kein zusätzlicher kurzer Tastendruck ausgeführt werden. Seitentaste lang
   muss stoppen und auf die Übersicht zurückkehren.
8. Lesbarkeit langer Code-Namen und Umlaute prüfen. Anzeige darf nicht über
   die Karten hinausragen. Gepufferte Anzeige und Rückfall ohne Sprite-Puffer
   auf dem tatsächlichen Board prüfen; keine Hardwaremessung ist simuliert.

## Akku-Messwerte

1. SDA GPIO8, SCL GPIO18, BQ27220-Adresse 0x55 überprüfen. Die Firmware
   benutzt reine Registerlesezugriffe bei 100 kHz, keine Profil-/Kalibrierungs-
   oder Ladeparameteränderungen.
2. Akku-Prozent und Spannung gegen Hersteller-Testfirmware bzw. Messgerät
   vergleichen. BATTPRES-Flag und SOC-Konfiguration des vorhandenen Gauge prüfen,
   falls trotz angeschlossenem Akku keine Werte verfügbar sind.
3. Akkustrom beim Laden und im Akkubetrieb vergleichen; Vorzeichen muss erhalten
   bleiben. Chiptemperatur auf plausible interne Erwärmung prüfen und keinesfalls
   mit einem Raumtemperaturfühler gleichsetzen.
4. Ohne erreichbaren Gauge müssen „--“ / JSON `null` und nicht verfügbare
   HA-Messsensoren erscheinen. Keine erfundenen Nullwerte zulassen.
5. Längeren Scan durchführen: Messwerte über 15 s Alter müssen ungültig werden.
   Nach regulärem Auslesen sollen sie wieder verfügbar werden. Manuellen
   Refresh mehrfach drücken: vollständige Polls höchstens einmal pro Sekunde.
6. BQ25896 an Adresse 0x6B prüfen: USB ab-/anstecken, Ladestatus mit Hersteller-
   Testfirmware vergleichen. Nur REG0B lesen; keine Ladeparameter schreiben.

## Zusatzhardware und gemeinsamer Betrieb

1. NFC: ISO14443A-Tag scannen, UID gegen bekannten Leser vergleichen. Gleichen
   Tag liegen lassen: keine Ereignisflut. Entfernen, nach mindestens 1,5 s neu
   auflegen: neues Ereignis. Tags wechseln. In HA Tag-Automation prüfen.
2. IR: bekannte TV-Fernbedienung auf Platz 0 lernen, `ir_list` exportieren,
   Neustart und Senden prüfen. Wiedererkennung als HA-Geräteauslöser prüfen.
   Zusätzlich unbekannte kurze RAW-Folge testen; Träger bei Bedarf anpassen.
   Überlange/Klimageräte-Telegramme müssen sichtbar abgewiesen werden.
3. Mikrofon: startet aus. Einschalten und Ruhe, Sprache und Klatschen vergleichen.
   Relative Pegeländerungen erwarten; keine Raumtemperatur- oder dB-SPL-Werte.
   Abschalten muss Messwert ungültig machen. Lange Funk-Scans/IR-Lernen müssen
   den Sensor pausieren und anschließend wieder freigeben.
4. RGB: Farbe/Helligkeit/AUS und WLAN-Statuseffekt lokal und über HA prüfen.
   Signalton zunächst mit niedriger Lautstärke testen. Kein unbeabsichtigter
   Dauerton nach Ende oder Fehler. Display per HA aus- und einschalten;
   erste lokale Bedienaktion darf nur wecken und keinen Funkcode senden.
5. nRF24: Erkennung ohne/mit Plus-Modul prüfen. Passend konfigurierte zweite
   Gegenstelle mit 1 Mbit/s, dynamischen Payloads, CRC16 und ohne Auto-ACK
   verwenden. Kanal, Adresse, Nutzdaten in beide Richtungen vergleichen.
   Aktivitätstest und anschließenden Paketempfang auf vorherigem Kanal prüfen.
6. SD: Ohne Karte sichtbaren Fehler erwarten. FAT-Karte einlegen und Hardware
   erneut prüfen. Funk-/IR-Codes sichern, JSONL-Datei am PC kontrollieren,
   zweite Sicherung muss `backup.prev.jsonl` erhalten. Exportierte Codes per
   USB wiederherstellen, Bestätigung jedes Platzes prüfen. Keine Formatierung.
7. Zusatzfunktionen einzeln und gemeinsam für mindestens 30 Minuten testen:
   SPI-Konflikte, Empfangsverluste, Speicherverbrauch, Watchdog/Neustarts und
   Stromaufnahme beobachten. NFC-/SD-Fehler dürfen keinen Neustart auslösen.
8. Bei deaktivierter Compile-Zeit-Funktion muss ein Befehl kontrolliert scheitern;
   bei nicht bestücktem/erreichbarem Modul keine erfundenen Messwerte anzeigen.

## Home Assistant

1. WLAN/Broker lokal konfigurieren und neu bauen; Verbindung und Discovery prüfen.
2. Scan-/Stop-Tasten und Sendetasten überprüfen. RC-Empfang muss zum passenden
   Geräteauslöser führen; RAW-Empfangstrigger werden bewusst nicht angeboten.
3. HA neu starten: Discovery muss nach der Birth-Nachricht erneut eintreffen.
4. MQTT-Verbindung trennen: Gateway als offline erwarten; nach Wiederverbindung
   neue `session` prüfen. Ein alter Befehl muss mit `stale_or_missing_session`
   abgewiesen werden.
5. Einen Platz löschen: zugehörige Discovery-Entitäten müssen verschwinden.
6. Netzwerk länger ausfallen lassen: Display-/USB-Bedienung müssen weiterlaufen.
   Ein TCP-Verbindungsversuch ist auf 1 s begrenzt; DNS-Auflösung kann zusätzlich
   verzögern. Der Hauptloop kann daher kurz pausieren; vollständig
   unterbrechungsfreier Empfang ist mit diesem Prototyp nicht zugesichert.
7. Ein gemeinsames Gerät mit Namen LILYGO T-Embed CC1101 Plus und allen
   Zusatzentitäten erwarten. IR-Plätze löschen: Sendetaste und Auslöser müssen
   verschwinden. Encoder-/Seitentastenauslöser einzeln prüfen.
8. Licht-Topic-Ausnahme prüfen: `light/set` braucht keine Session, darf aber
   ausschließlich Licht verändern; `action:send` auf diesem Topic darf nicht senden.

Ergebnisse mit Boardrevision, Firmwareversion und Geräte-/Protokolltyp
dokumentieren. Keine WLAN-/MQTT-Zugangsdaten oder privaten empfangenen Codes
veröffentlichen.
