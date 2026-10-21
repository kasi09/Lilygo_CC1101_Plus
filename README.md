# LILYGO T-Embed CC1101 Plus für Home Assistant

Eigene ESP32-Firmware für den **LILYGO T-Embed CC1101 Plus**. Schwerpunkt:
433-MHz-Steckdosen, Klingeln und einfache Sensoren. Die Firmware läuft auf dem
LilyGO; Home Assistant OS bleibt der Server. Die Verbindung erfolgt über WLAN
und einen MQTT-Broker. Die eigene Home-Assistant-Komponente liegt unter
`custom_components/lilygo_t_embed`; alternativ bleibt MQTT Discovery verfügbar.

**Neue eigene Integration 0.1.0:** [Installation und Bedienung](docs/HACS.md).
Quellcode: [kasi09/Lilygo_CC1101_Plus](https://github.com/kasi09/Lilygo_CC1101_Plus).
Ein lokales Installations-ZIP lässt sich mit `python tools/package_hacs.py` bauen.
Benötigt Home Assistant ab 2026.10.0 und Firmware ab 0.4.0 mit
`GW_MQTT_DISCOVERY false` bzw. die Portal-Auswahl **Eigene Integration (HACS)**.
Firmware **0.5.2** behebt den Sitzungsfehler beim Speichern der Einrichtung.
Beim Öffnen der Seite startet eine WLAN-Suche; gefundene 2,4-GHz-Netze lassen
sich mit Signalstärke und Verschlüsselung auswählen. **Erneut nach WLAN suchen**
aktualisiert die Liste, versteckte WLANs bleiben manuell eintragbar.
WLAN-QR-Code und Display-Anpassungen aus 0.5.1 bleiben enthalten.
Prüfergebnisse stehen im [Hardwaretest](docs/HARDWARE_TEST.md).
0.5.2 wurde am 21.10.2026 erfolgreich gebaut, auf COM4 geflasht und per USB
auf stabilen Start und weiterhin korrekte Hotspot-QR-Codes geprüft.

## WLAN und MQTT ohne erneutes Kompilieren einrichten

1. Die Firmware einmal auf den LilyGO flashen. Ohne gespeicherte WLAN-Daten
   startet sofort ein geschützter Hotspot **LilyGO-Setup-…**.
2. Den **WLAN-QR-Code mit der Handykamera scannen** und den Beitritt bestätigen.
   Hotspot-Name und zufälliges Passwort stehen daneben, falls eine manuelle
   Verbindung nötig ist. Die Verbindung trotz Hinweis auf fehlenden
   Internetzugang beibehalten.
3. Die Einrichtungsseite öffnet sich häufig automatisch. Sonst im Browser
   **http://192.168.4.1/** öffnen.
4. Ein gefundenes WLAN auswählen oder dessen Namen direkt eingeben.
   WLAN-Passwort sowie MQTT-Broker, Port und Zugangsdaten eintragen.
   Für die eigene HACS-Komponente **Eigene Integration (HACS)** auswählen.
5. **Speichern und verbinden** drücken. Die Daten werden auf dem LilyGO in NVS
   gespeichert. Sobald WLAN und gegebenenfalls MQTT funktionieren, schließt der
   Hotspot nach zehn Sekunden. Bei einem Fehler bleibt er für Korrekturen offen.

Bei einem WLAN-Ausfall öffnet sich der Hotspot nach **30 Sekunden** erneut.
Er kann auf der Displayseite **WLAN / MQTT einrichten** auch manuell gestartet
werden. Ein Klick bei geöffnetem Hotspot schließt ihn; dadurch bleibt er bei
weiterhin fehlendem WLAN für den Rest dieses Starts geschlossen. Nach einer
erfolgreichen WLAN-Verbindung ist die automatische Wiederherstellung wieder aktiv.
Das Hotspot-Passwort wird bei jedem erneuten Öffnen neu erzeugt.
Der QR-Code wird dabei ebenfalls neu erzeugt. Er enthält ausschließlich die
temporären Hotspot-Zugangsdaten; die WLAN-/MQTT-Einstellungen folgen im Browser.
Er wird direkt auf dem Gerät berechnet und enthält keine externen Links.
Die schwarze Grafik hat drei Pixel pro Modul und einen weißen Rand von vier
Modulen; Meldungen und Bedienelemente überdecken diesen Rand nicht.

Die WLAN-Suche läuft asynchron. Die Liste zeigt bis zu 24 unterschiedliche
sichtbare Netze nach Signalstärke; ein WLAN-Name wird nicht automatisch
überschrieben. Gleiche WLAN-Namen erscheinen einmal. Die Ergebnisse bleiben
im Einrichtungsportal und werden nicht über MQTT oder in der Codesicherung
veröffentlicht. Bei fehlgeschlagener Suche ist die manuelle Eingabe weiter möglich.

Bei **„Sitzung ungültig“** mit älterer Firmware: auf 0.5.2 aktualisieren und
die Seite **http://192.168.4.1/** frisch öffnen. Ältere offene Tabs enthalten
nach einem Neustart oder erneutem Öffnen des Hotspots einen veralteten
Sitzungsschlüssel. 0.5.2 korrigiert zusätzlich die Browser-Richtlinie, durch
die vorher selbst frische Formular-POSTs fälschlich abgewiesen werden konnten.

In **Weitere Einstellungen** lassen sich eine eindeutige Geräte-ID und die
Home-Assistant-Anbindung wählen. Das Basistopic lautet `lilygo/GERÄTE-ID`,
standardmäßig `lilygo/lilygo_rf`. Eine geänderte Geräte-ID erfordert ein passendes
Basistopic in Home Assistant. Ein leerer Broker schaltet MQTT aus.

Passwortfelder bleiben beim Laden der Seite leer. Bei unverändertem WLAN bzw.
Broker und MQTT-Benutzer bedeutet ein leeres Passwortfeld **beibehalten**.
Offenes WLAN und das Löschen eines MQTT-Passworts werden ausdrücklich ausgewählt.
Gespeicherte Einstellungen haben Vorrang vor `include/config.local.h`.
Die Einrichtungsseite ist ausschließlich an der Hotspot-IP erreichbar und wird
nach dem Schließen beendet. WLAN-/MQTT-Passwörter erscheinen weder in Status,
MQTT, SD-Codesicherung noch im Repository. Sie liegen im NVS-Gerätespeicher.
Während Funklernen und Scans pausiert die Bearbeitung von Einrichtungsanfragen;
beim normalen Portalbetrieb können HTTP-Anfragen den Funkempfang kurz verzögern.

Der Hotspot kann außerdem über USB geöffnet oder geschlossen werden:

```powershell
python tools/gateway.py --port COM4 --seconds 2 network-setup
python tools/gateway.py --port COM4 --seconds 2 network-setup --state off
```

`setup-info` zeigt auf ausdrückliche lokale USB-Anfrage die temporären Hotspot-
Zugangsdaten. Gespeicherte Heim-WLAN-/MQTT-Passwörter werden dabei nicht ausgegeben.
Diese USB-Ausgabe nicht veröffentlichen.

Mit `python tools/gateway.py --port COM4 --seconds 2 display-info` lassen sich
Display-Geometrie, Bildpuffer und sichtbarer Zeichenbereich ohne Zugangsdaten
prüfen. Für das Plus-Panel werden 320×170 im Querformat und ein
ST7789-Zeilenoffset von 35 erwartet. Ob der physische Bildschirm an allen
Seiten sichtbar ist, muss am Gerät kontrolliert werden.

**Hardwareteststand: Version 0.3.1, Prototyp.** Die Firmware integriert die praktisch nutzbaren
Boardfunktionen in ein gemeinsames Home-Assistant-Gerät. Beim Kompilieren werden
Funkgrenzen, Messwertumrechnung, Zusatzfunktionen und Display-Navigation mit
54 Prüfungen kontrolliert, darunter sieben für NFC-Antworten. Version 0.3.1 wurde
am 20.10.2026 erfolgreich gebaut und auf COM4 geflasht (Beispielkonfiguration ohne
Zugangsdaten). Alle übertragenen Daten wurden per Prüfsumme bestätigt.
Das USB-Werkzeug wurde auf Python-Syntax, CLI-Aufruf und mit sechs Hosttests
für Importgrenzen und Gerätebestätigungen geprüft; dabei wurde kein COM-Port geöffnet.
Funk, Display und Bedienung müssen am tatsächlichen Plus-Board
abgenommen werden. Ein erfolgreicher Build ersetzt diesen Hardwaretest nicht.
Der USB-Starttest von 0.3.1 bestätigte Empfangsmodus, Codespeicher, PN532, nRF24
und Akku-/Lade-IC. Mit aktivem NFC waren in 30 Sekunden alle 15 Statusabfragen
erfolgreich, ohne I²C-Zeitüberschreitungen oder Neustarts. 0.3.1 behebt dafür die
blockierende NFC-Abfrage und liest die noch offene Konfigurationsantwort vollständig.
Das Lesen eines echten NFC-Tags sowie Funk-/IR-Bedienung und Display/Tasten sind
weiterhin praktisch zu prüfen. Zugangsdaten sind noch nicht eingerichtet.

## Enthalten

- Empfang und Anlernen der 12 in rc-switch 2.6.4 enthaltenen OOK-Festcode-Protokolle,
  mit maximal 32 Bit pro Telegramm.
- 16 Speicherplätze in NVS; Namen und Codes überleben einen Neustart.
- Eingabe bekannter Codes sowie JSON-Export und -Import.
- Separater experimenteller RAW-Lernmodus für OOK-Pulsfolgen mit bis zu
  256 Pulsen. RAW ist zum Aufnehmen und Wiedergeben gedacht; gespeicherte
  RAW-Folgen werden im normalen Empfang nicht als Ereignis wiedererkannt.
- Senden gespeicherter Codes auf 433,05–434,79 MHz. Andere Bereiche bleiben
  in dieser Version für den Empfang und RSSI-Scans verfügbar.
- RSSI-Scans: 433 MHz, 868 MHz oder nacheinander alle drei CC1101-Bereiche.
- MQTT Discovery: Diagnose-Sensoren, Scan-/Stop-Tasten, Sendetaste je Code und
  Geräteauslöser für wiedererkannte RC-Codes.
- Neun Displayseiten für Empfang, Codes, Lernen, Scanner, Eingänge, Infrarot,
  Bedienung, Gerätewerte und WLAN-/MQTT-Einrichtung, mit
  gepufferter Darstellung, Lern-Countdown und Scan-Pegelgrafik.
- Akku-Prozent, Spannung, Strom und eindeutig benannte interne BQ27220-
  Chiptemperatur auf dem Display und über MQTT Discovery. Auslesen ohne
  Änderung der Ladeparameter oder Kalibrierung; fehlende Werte bleiben unbekannt.
- USB-Versorgung und Ladestatus aus dem BQ25896, ebenfalls nur lesend.
- IR-Empfang, acht dauerhafte IR-Speicherplätze, Lernen, Senden und HA-Auslöser.
- PN532-NFC-Leser: Tag-UIDs als Home-Assistant-Tag-Scanner und Live-Anzeige.
- RGB-Ring als HA-Licht (Farbe, Helligkeit, WLAN-Statuseffekt), Display als Schalter,
  kurzer Signalton und Encoder-/Tastenereignisse für Automationen.
- Optionaler relativer Mikrofonpegel in dBFS; startet aus, keine Audioaufzeichnung.
- nRF24-Paketbrücke für passend konfigurierte Gegenstellen; startet aus.
  Zusätzlicher grober 2,4-GHz-Aktivitätstest über die RPD-Schwelle des Moduls.
- SD-Kartenprüfung ohne Formatierung und manuelle JSONL-Sicherung der Funk-/IR-Codes.
- USB-JSON-Befehle sowie Python-Bedienwerkzeug.
- Betrieb ohne WLAN über Display und USB ist ebenfalls möglich.

Steckdosen benötigen meist **zwei Speicherplätze**, je einen für AN und AUS.
Eine gesendete Taste bestätigt nicht den tatsächlichen Schaltzustand einer
Steckdose. Deshalb werden zunächst Tasten statt optimistischer Schalter angelegt.

## Grenzen

Der CC1101 empfängt in **300–348, 387–464 und 779–928 MHz**. Antenne und
HF-Anpassung des Boards beeinflussen die praktische Empfindlichkeit. Der
Scanner misst diese Bereiche schrittweise; er ist kein SDR, liefert keine
IQ-Daten und erkennt anhand eines Pegels noch kein Protokoll. Während eines
Scans oder RAW-Anlernens ist die normale Code-Erkennung unterbrochen.

Die Decodierung dieser Version ist auf OOK und rc-switch beschränkt.
FSK-Sensoren, Wetterstationen, Zähler, verschlüsselte Protokolle und Rolling
Codes benötigen eigene Decoder bzw. eine echte Protokollintegration. Ein RAW-
Mitschnitt macht ein Rolling-Code-System nicht dauerhaft nachbildbar. Bei RAW
können Störungen als Pulsfolge aufgenommen werden; Aufnahmen müssen praktisch
geprüft werden. RAW trennt Rahmen bei mindestens 8 ms Pause und verwirft
Pulse unter 80 µs oder überlange Rahmen. Beim Aufnehmen die Taste kurz halten,
damit wiederholte Rahmen die Synchronisation ermöglichen.

Die rc-switch-Bibliothek meldet den Zahlenwert 0 nicht als empfangenen Code.
Bekannte Null-Codes lassen sich manuell speichern. Kurze Wiederholungen desselben
RC-Codes werden zu einem Ereignis zusammengefasst (500 ms Pause bis zum nächsten).

## Einrichten und bauen

1. Diesen Ordner mit VS Code und PlatformIO öffnen oder die PlatformIO-CLI benutzen.
2. Optional `include/config.example.h` nach `include/config.local.h` kopieren.
3. WLAN und MQTT können nach dem Flashen über den Hotspot eingerichtet werden.
   Alternativ Zugangsdaten und eine eindeutige `GW_DEVICE_ID` lokal eintragen.
   Die Broker-Adresse ist die im LAN erreichbare IP bzw. der DNS-Name des
   HAOS-Rechners, nicht der interne Containername `core-mosquitto`.
4. Bauen:

```powershell
python -m platformio run
```

Die leere Beispielkonfiguration baut ebenfalls; ohne lokale Konfiguration
startet der Einrichtungs-Hotspot; MQTT bleibt bis zur Einrichtung aus.
Mit `GW_SETUP_AP_ENABLED false` lässt sich der Hotspot beim Bauen abschalten.
`config.local.h` wird von Git ignoriert. Zugangsdaten
sind nach dem Kompilieren allerdings im Firmware-Abbild enthalten. Keine
Zugangsdaten oder Firmware mit echten Zugangsdaten veröffentlichen.

Boardprofil: ESP32-S3, 16 MB Flash, 8 MB OPI-PSRAM. Die verwendete Arduino-
Plattform ist auf Version 6.13.0 festgelegt; Bibliotheken sind ebenfalls exakt
versioniert. Die Pinbelegung folgt dem Herstellerprojekt. Da dieses Projekt
mehrere Hardware-/Plus-Ausführungen zusammenfasst, ist die tatsächliche
Boardrevision vor dem ersten Flashen zu prüfen, siehe [Hardwaretest](docs/HARDWARE_TEST.md).

Flashen erst mit passendem Board am USB-Datenkabel; `COM7` ersetzen:

```powershell
python -m platformio device list
python -m platformio run --target upload --upload-port COM7
python -m platformio device monitor --port COM7
```

Das Boardprofil startet den ESP32-S3 nach dem Upload per Watchdog neu. Dieser
Ablauf wurde auf COM4 geprüft; der gewöhnliche RTS-Neustart ließ dieses Gerät
beim Starttest im Bootloader. Ein zusätzlicher manueller Neustart ist mit dem
angepassten Profil nicht erforderlich.

Beim Flashen wird die bisherige Firmware ersetzt. Falls das Board nicht erkannt
wird: Encoder-/BOOT-Taste gedrückt halten, RESET betätigen, BOOT wieder loslassen
und den neu erscheinenden COM-Port verwenden. Der Encoder ist beim normalen
Start loszulassen, da sein Taster auch der ESP32-BOOT-Pin ist.

## Bedienung am Gerät

Die Seitentaste wechselt die acht Ansichten. **Seitentaste 0,8 s halten** beendet
einen laufenden Vorgang und kehrt zur Empfangsübersicht zurück. Alle Plätze
werden wie im USB-/MQTT-Protokoll von **0 bis 15** (Funk) bzw. **0 bis 7** (IR) nummeriert.

| Ansicht | Drehen | Encoder kurz drücken | Encoder 0,8 s halten |
| --- | --- | --- | --- |
| Empfang | – | Zu den gespeicherten Codes | – |
| Codes | Speicherplatz wählen | Gewählten Code senden | – |
| Anlernen | Zielplatz wählen | RC-Festcode anlernen | RAW-Pulsfolge anlernen |
| Scanner | 433, 868 oder alle Bereiche wählen | Scan starten | – |
| Eingänge | – | – | – |
| Infrarot | IR-Platz 0–7 wählen | IR-Code senden | IR-Code anlernen |
| Bedienung | Aktion wählen | Signalton, RGB, Mikrofon, NFC, nRF24, SD-Sicherung oder Hardwareprüfung | – |
| Gerätewerte | – | Messwerte neu auslesen | – |

Während des Anlernens zeigt die Firmware die Restzeit und den Zielplatz;
kurzer Druck auf Encoder oder Seitentaste bricht ab. Der Lernversuch endet
nach 15 Sekunden. Belegte Zielplätze sind farbig als zu ersetzend markiert.
Beim erneuten Anlernen am Gerät bleibt der bestehende Name erhalten.
Auf der Funk-Lernseite startet langes Drücken RAW. Auf der IR-Seite startet es
IR-Lernen. Beim Loslassen wird kein zusätzlicher kurzer Tastendruck ausgelöst.
Ein über HA ausgeschaltetes Display wird durch die nächste Bedienaktion geweckt;
dieser erste Tastendruck bzw. Drehschritt führt nur das Aufwecken aus.

Beim Scannen erscheinen eine Pegelgrafik, Messpunktzahl und der stärkste Punkt.
Die Anzeige fasst Messpunkte in 72 Frequenzspalten zusammen; jede Spalte zeigt
den stärksten gemessenen Pegel. USB und MQTT liefern weiterhin die einzelnen
Messpunkte. Kurzer Druck auf Encoder oder Seitentaste stoppt den Scan. Bei
normalem Ende kehrt die Firmware zur ursprünglichen Empfangsfrequenz zurück.

Lernmodus und laufender Scan haben Vorrang vor der Seitennavigation. Die
Fußzeile zeigt jeweils die verfügbaren Tastenaktionen. Deutsche Umlaute in
Namen werden für den eingebauten Displayfont als ae/oe/ue/ss dargestellt;
gespeicherte Namen und JSON-Export behalten ihren UTF-8-Text.

Frequenz nach dem Start: **433,92 MHz**. Eine per `tune` gewählte Frequenz gilt
bis zum nächsten Neustart. Gespeicherte Codes behalten ihre eigene Frequenz;
nach dem Senden kehrt der Empfänger zur zuvor gewählten Frequenz zurück.

## Welche Sensoren sind an Bord?

| Hardware / Quelle | Verfügbare Funktion | Firmwarestand 0.3 |
| --- | --- | --- |
| BQ27220 Akku-Mess-IC | Ladestand, Spannung, Strom, interne Chiptemperatur | Display, Status und HA-Sensoren |
| BQ25896 Lade-IC | USB-Versorgung, lädt / Ladepause / voll | Display, Status, HA; keine Ladeparameteränderung |
| CC1101 | Sub-GHz-Empfang und RSSI | Empfang / Scan implementiert |
| ESP32-WLAN | WLAN-Signalstärke | Display und HA-Sensor |
| PN532 | ISO14443A-UID lesen | Display, MQTT-Tag-Scanner; keine Kartenänderungen |
| Infrarot-Empfänger / -Sender | IR-Fernbedienungen | Lernen, Senden und Empfangsauslöser |
| Mikrofon | Relative Geräuschstärke | Optionaler dBFS-Sensor; kein kalibriertes Schallpegelmessgerät |
| nRF24L01 im Plus-Modell | Zusätzliches 2,4-GHz-Funkmodul | Konfigurierbare Hex-Paketbrücke und RPD-Aktivitätstest |
| RGB-LEDs, Lautsprecher, Display | Licht, Signalton, Anzeige | HA-Steuerung und lokale Bedienseite |
| microSD | Datensicherung | Manuelle Code-Sicherung, keine Formatierung |

NFC, IR und die Funkmodule sind Empfänger bzw. Schnittstellen; sie ersetzen
keine Umgebungssensoren. Die Herstellerbestückung führt keinen eigenen
Raumtemperatur-, Feuchte-, CO₂- oder Bewegungssensor auf. Über die I²C-/Qwiic-
Schnittstellen können externe Sensoren ergänzt werden. Externe Funksensoren
benötigen jeweils einen passenden Protokolldecoder.

**„Akku-IC Chiptemperatur“ misst den BQ27220 selbst, nicht die Raumtemperatur.**
Der Gauge liefert den Akkuladestand als Schätzwert auf Grundlage seiner
bestehenden Konfiguration. Die Firmware schreibt keine Lade-, Profil- oder
Kalibrierungswerte. Sie liest in der Regel alle 5 s bei 100 kHz I²C; manueller
Refresh ist auf höchstens einmal pro Sekunde begrenzt. Während Lernen und Scan
pausiert das regelmäßige Auslesen, um den Funkbetrieb zu priorisieren.
Messwerte über 15 s Alter werden verworfen. Bei fehlenden, ungültigen oder
veralteten Daten zeigt das Display „--“, JSON enthält `null`, und die
zugehörigen HA-Messsensoren werden als nicht verfügbar gemeldet.

IR unterstützt bekannte Protokolle bis 64 Bit ohne langen Klimageräte-Zustand
sowie RAW-Folgen mit höchstens 128 Pulsen. RAW-Wiedergabe nimmt zunächst 38 kHz
an; der IR-Empfänger kann die ursprüngliche Trägerfrequenz nicht messen. Bei
Bedarf `carrier_khz` (30–60) im exportierten Datensatz anpassen. Nicht jedes
empfangbare IR-Protokoll lässt sich senden; die Firmware meldet dies als Fehler.
IR-Empfang wiedererkennt passende gespeicherte Codes, bei RAW mit Zeittoleranz.

NFC liest UIDs, keine geschützten Kartendaten. Eine UID ist kein sicherer
Authentifizierungsnachweis. Der Leser, nRF24-Empfang, Mikrofon und RGB-Ausgabe
pausieren während eines Funk-Lernvorgangs oder CC1101-Scans; beim IR-Lernen
pausieren NFC, nRF24 und Mikrofon. Die Zusatzfunktionen sind Prototyp-Treiber
und müssen auch im gemeinsamen Betrieb am Board geprüft werden.

nRF24 nutzt ab Start Kanal 76 (2476 MHz), Adresse `4841303031` (ASCII `HA001`),
1 Mbit/s, dynamische Payload-Länge, CRC16, kein Auto-ACK und niedrige Sendeleistung.
Die Gegenstelle muss diese Parameter teilen. Kein automatisches Erkennen
beliebiger 2,4-GHz-Sensoren, kein WLAN-/Bluetooth-Scanner. Der Aktivitätstest
liefert je Kanal 0–83 nur eine RPD-Schwellwertmeldung, keine RSSI- oder IQ-Daten.
Kanal und Adresse lassen sich ändern, bleiben wie alle Laufzeitschalter bis zum
Neustart gültig. Lediglich gelernte Funk-/IR-Codes werden dauerhaft gespeichert.

## USB-Werkzeug

`tools/gateway.py` verwendet `pyserial` (bei PlatformIO meist bereits vorhanden).
Bei Bedarf `python -m pip install pyserial` ausführen. Währenddessen den
PlatformIO-Monitor schließen, damit nur ein Programm den COM-Port öffnet.

```powershell
python tools/gateway.py --port COM7 status
python tools/gateway.py --port COM7 learn --slot 0 --name "Steckdose AN"
python tools/gateway.py --port COM7 learn --slot 1 --name "Steckdose AUS"
python tools/gateway.py --port COM7 send --slot 0
python tools/gateway.py --port COM7 list
python tools/gateway.py --port COM7 learn --slot 2 --kind raw --name "Klingel RAW"
python tools/gateway.py --port COM7 tune --frequency 433.92
python tools/gateway.py --port COM7 --output captures/scan.jsonl scan --band all
python tools/gateway.py --port COM7 --seconds 0 --output captures/events.jsonl watch
```

Standardmäßig liest das Werkzeug 20 Sekunden lang Antworten. `--seconds 0`
liest bis Strg+C. Bei einem vollständigen Scan bei Bedarf mehr Zeit vorgeben.
Die JSONL-Datei enthält pro Zeile ein JSON-Objekt. Einen einzelnen `remote`-
Datensatz aus `list` als JSON-Datei sichern und wieder importieren:

```powershell
python tools/gateway.py --port COM7 store --slot 3 --file remote.json
python tools/gateway.py --port COM7 ir-learn --slot 0 --name "TV Power"
python tools/gateway.py --port COM7 ir-send --slot 0
python tools/gateway.py --port COM7 feature --name microphone --state on
python tools/gateway.py --port COM7 backup
python tools/gateway.py --port COM7 command --file command.json
python tools/gateway.py --port COM7 restore --file E:/lilygo/backup.jsonl
```

Importieren speichert nur; es sendet nichts. Das vollständige Befehlsformat
steht in [PROTOCOL.md](docs/PROTOCOL.md).

`backup` schreibt auf die eingelegte FAT-formatierte SD-Karte nach
`/lilygo/backup.jsonl`, die vorherige Sicherung bleibt als `backup.prev.jsonl`.
Gesichert werden nur belegte Funk-/IR-Plätze, keine Zugangsdaten. Beim
Wiederherstellen wird die gesamte Datei zunächst geprüft und jeder Schreibbefehl
einzeln bestätigt. Die enthaltenen Plätze werden überschrieben; andere Plätze
bleiben bestehen. Bei einem Fehler stoppt der Import; bereits geschriebene Plätze
bleiben übernommen. Sicherungsdateien enthalten persönliche Fernbedienungscodes.

## Home Assistant OS

Empfohlen ist jetzt die [eigene Integration mit deutscher Einrichtung](docs/HACS.md).
Sie bietet Anlernen, Codeimport, Sendetasten, Sensoren, Licht und Ereignisse.
Für diesen Weg `GW_MQTT_DISCOVERY false` setzen; Installation und Umstieg stehen
in der verlinkten Anleitung. Die folgenden Schritte beschreiben den weiterhin
verfügbaren alternativen Betrieb mit `GW_MQTT_DISCOVERY true`.

1. MQTT-Broker, beispielsweise Mosquitto, und die MQTT-Integration in Home
   Assistant einrichten. Für den LilyGO einen passenden Broker-Benutzer verwenden.
2. Broker-Adresse und Anmeldedaten in der lokalen Firmware-Konfiguration setzen,
   bauen und nach dem Hardwareabgleich flashen.
3. Nach Verbindung erscheint das Gerät **LILYGO T-Embed CC1101 Plus** über MQTT Discovery.
4. Einen Code am Gerät oder über USB lernen. Danach erscheint seine Sendetaste.
   Bei RC-Codes erscheint zusätzlich ein Geräteauslöser für den Empfang.
5. Eine Automation mit diesem Geräteauslöser oder einem MQTT-Ereignis erstellen.

Für diesen alternativen Discovery-Betrieb genügt die eingebaute MQTT-Integration.
Zusätzlich erscheinen NFC-Tag-Scanner, IR-Sendetasten und Empfangsauslöser,
RGB-Licht, Funktionsschalter, Diagnosewerte sowie Tasten für Signalton,
SD-Sicherung, Hardwareprüfung und den 2,4-GHz-Aktivitätstest. Die sechs
Encoder-/Seitentastenereignisse können optional als Geräteauslöser genutzt
werden; sie behalten gleichzeitig ihre lokale Bedienfunktion. Unter
**Einstellungen → Tags** kann ein gescannter NFC-Tag einer Automation dienen.

Die Entdeckung wird nach MQTT-Neuverbindung und bei Home Assistants
`homeassistant/status = online` erneut veröffentlicht. Verfügbarkeit wird über
Birth/Last Will gemeldet. Discovery und Verfügbarkeit werden retained gespeichert;
Codes, Ereignisse und Sendebefehle werden nicht retained veröffentlicht.

MQTT-Befehle brauchen ein aktuelles `session`-Feld aus dem State-Topic. Es ändert
sich bei jeder Broker-Neuverbindung und verhindert das automatische Ausführen
alter retained Sendebefehle. Die automatisch entdeckten HA-Tasten enthalten es
bereits. Es ersetzt keine Broker-Zugriffskontrolle. Die erste Version verwendet
MQTT ohne TLS für das eigene LAN; keine Internet-Portfreigabe einrichten.

Die HA-JSON-Lichtsteuerung verwendet das getrennte Topic `light/set` ohne
Session-Feld. Es akzeptiert ausschließlich Lichtzustand, Farbe, Helligkeit und
Effekt. Es kann keine Funk-, IR- oder anderen Gerätebefehle ausführen. Den
Lichtzustand publiziert die Firmware retained unter `light/state`.

Beispiel einer Automation für eine auf Platz 2 angelernte Klingel:

```yaml
alias: Funkklingel erkannt
triggers:
  - trigger: mqtt
    topic: lilygo/lilygo_rf/remote/2/event
    payload: press
actions:
  - action: persistent_notification.create
    data:
      title: Funkklingel
      message: Die angelernte Funkklingel wurde betätigt.
mode: single
```

Die MQTT-Topics hängen von `GW_DEVICE_ID` ab. Zum Anlernen per MQTT kann das
JSON-Format aus der Protokolldokumentation verwendet werden. Zugangsdaten
werden nie über Status, Discovery oder USB ausgegeben.

## Weiterentwicklung

Als nächste Schritte sind besonders nützlich:

1. Kompatibilitätsliste mit den tatsächlich vorhandenen Steckdosen, Klingeln und
   Sensoren; Decoder anhand ihrer aufgezeichneten Telegramme ergänzen.
2. AN-/AUS-Paare zu HA-Schaltern zusammenfassen, mit klar sichtbarem Statusmodell.
3. Wetter-/Temperatur-/Türsensoren mit echten Messwert-Decodern und Geräte-IDs.
4. Externe I²C-Umgebungssensoren, etwa Temperatur/Feuchte, passend zur Bestückung.
5. Authentifizierte OTA-Updates und eine komfortablere Konfiguration.

Breitbandige SDR-Analyse mit HackRF/PortaPack bleibt ein getrenntes Folgeprojekt.
Der aktuelle Auftrag konzentriert sich auf den LilyGO.

## Prüfung

Jeder Build enthält die Grenzwertprüfungen in `src/rf_rules_checks.cpp` für
Empfangs-/Sendebereiche, Scan-Grenzen, Code-Längen und Sendedauer. Die geplante
Sendedauer ist auf 500 ms pro Befehl und mindestens 1 s Abstand begrenzt;
lange Codes benötigen gegebenenfalls weniger `repeats`. Dies sind technische
Grenzen und keine automatische Prüfung aller lokalen Funkvorgaben. Nur eigene
bzw. freigegebene Geräte innerhalb der erlaubten Funkparameter verwenden.

Für die reale Abnahme gilt [docs/HARDWARE_TEST.md](docs/HARDWARE_TEST.md).

Die PC-Prüfungen für die Wiederherstellung laufen ohne angeschlossenes Gerät:

```powershell
python -m unittest discover -s tools -p test_gateway.py -v
```

Das Firmware-Abbild entsteht unter `.pio/build/lilygo-cc1101-plus/firmware.bin`.
Zum Flashen den oben beschriebenen PlatformIO-Aufruf verwenden; er überträgt
auch Bootloader und Partitionstabelle an die richtigen Adressen.

## Quellen

- [LILYGO-Herstellerprojekt](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101)
- [Hersteller-Pinbelegung](https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/blob/master/docs/pinmap_cn.md)
- [TI CC1101](https://www.ti.com/product/CC1101)
- [RadioLib](https://github.com/jgromes/RadioLib)
- [rc-switch](https://github.com/sui77/rc-switch/tree/2.6.4)
- [Home Assistant MQTT](https://www.home-assistant.io/integrations/mqtt/)
- [Home Assistant MQTT-Geräteauslöser](https://www.home-assistant.io/integrations/device_trigger.mqtt/)
- [LILYGO Plus-Modell](https://lilygo.cc/en-pl/products/t-embed-cc1101-plus)
- [TI BQ27220 Referenzhandbuch](https://www.ti.com/lit/ug/sluubd4a/sluubd4a.pdf)
- [TI BQ25896 Datenblatt](https://www.ti.com/lit/ds/symlink/bq25896.pdf)
- [Home Assistant MQTT-Tags](https://www.home-assistant.io/integrations/tag.mqtt/)
- [Home Assistant MQTT-Licht](https://www.home-assistant.io/integrations/light.mqtt/)
- [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266/tree/v2.8.6)
- [Adafruit PN532](https://github.com/adafruit/Adafruit-PN532/tree/1.3.4)
- [RF24](https://github.com/nRF24/RF24/tree/v1.5.0)
