# Power-Latcher für Raspberry Pi 4 / Radioberry

Der Power-Latcher hält die Versorgung des Raspberry Pi eingeschaltet, bis das Betriebssystem sauber heruntergefahren ist. Ein Taster an GPIO14 fordert den Shutdown an. Am Ende setzt der Pi GPIO15 auf HIGH. Die externe Un-Latch-Schaltung trennt anschließend mit einer RC-Verzögerung die Versorgung.

Diese Anleitung beschreibt die im Aufbau erfolgreich getesteten Einstellungen mit Raspberry Pi 4, Radioberry und labwc-Desktop. Die Einschalt- und Halteschaltung selbst wird durch den zugehörigen Schaltplan festgelegt.

## Ablauf

1. Über die Latch-Schaltung einschalten.
2. Der Raspberry Pi bootet; GPIO15 wird auf LOW gesetzt.
3. Den Shutdown-Taster drücken und loslassen.
4. Das Betriebssystem fährt ohne Ausschaltmenü sauber herunter.
5. GPIO15 wird am Ende des Shutdowns HIGH.
6. Der RC-Zweig lädt das Gate des 2N7002 auf und löst anschließend das Un-Latch aus.

Für den Shutdown ist mit diesen Einstellungen kein dauerhaft laufendes eigenes Skript notwendig.

## GPIO-Belegung

Alle GPIO-Nummern in der Konfiguration sind **BCM-Nummern**, keine physischen Pin-Nummern.

| Funktion | BCM / GPIO | Physischer Pin | Zustand / Anschluss |
|---|---|---|---|
| Shutdown-Taster | GPIO14 | 8 | Eingang mit Pull-up; Taster schaltet gegen GND |
| Poweroff / Un-Latch | GPIO15 | 10 | LOW im Betrieb; HIGH am Ende des Shutdowns |
| Gemeinsame Masse | GND | z. B. 9 | Masse der Schaltung und des Raspberry Pi |

GPIO14 und GPIO15 müssen für diese Funktion verfügbar sein. UART, serielle Konsole oder andere Programme dürfen sie nicht gleichzeitig verwenden. Die GPIOs arbeiten mit 3,3-V-Pegeln; keine 5 V anlegen.

## 1. UART und serielle Konsole freigeben

Falls die serielle Schnittstelle aktiviert ist:

```bash
sudo raspi-config
```

Unter **Interface Options → Serial Port** sowohl die serielle Login-Konsole als auch die serielle Hardware deaktivieren. Die genaue Menübezeichnung hängt von der installierten Version ab. Prüfen, ob zusätzliche UART-Overlays GPIO14/15 belegen.

## 2. Boot- und Shutdown-Konfiguration

Die tatsächlich verwendete `config.txt` bearbeiten. Bei Raspberry Pi OS Bookworm liegt sie normalerweise hier:

```bash
sudo cp /boot/firmware/config.txt /boot/firmware/config.txt.pwr-latcher.bak
sudo nano /boot/firmware/config.txt
```

Bei älteren Images ist der Pfad `/boot/config.txt`. Keine neue Datei anlegen, wenn das Image einen anderen Boot-Pfad verwendet.

Folgende Einstellungen in einem passenden, aktiven Abschnitt eintragen, beispielsweise am Dateiende unter `[all]`. Bereits vorhandene Zeilen für diese beiden Overlays anpassen und nicht doppelt eintragen.

```ini
[all]

# Taster → Shutdown (GPIO14, Pin 8, gegen GND)
dtoverlay=gpio-shutdown,gpio_pin=14,active_low=1,gpio_pull=up

# GPIO15 / Pin 10 beim Booten als Ausgang auf LOW
gpio=15=op,dl

# Poweroff-Signal → Un-Latch (GPIO15, Pin 10, aktiv HIGH)
dtoverlay=gpio-poweroff,gpiopin=15
```

**Die beiden Polaritäten unterscheiden sich:**

- `gpio-shutdown`: `active_low=1` ist richtig, weil der Taster gegen GND schaltet.
- `gpio-poweroff`: Kein `active_low=1` hinzufügen. Das Un-Latch benötigt HIGH beim Abschalten.

Die `gpio=`-Zeilen greifen erst, wenn die Firmware die Konfiguration verarbeitet. Sie garantieren keinen LOW-Pegel ab dem ersten Moment des Einschaltens. Den Anfangszustand des Un-Latch-Eingangs muss bei Bedarf die Hardware sicherstellen. Der 1-MΩ-Widerstand am Gate entlädt den Kondensator, hält aber nicht automatisch die GPIO-Seite der BAT54 auf LOW.

Speichern und neu starten:

```bash
sudo reboot
```

## 3. Direkter Shutdown unter labwc, ohne Menü

Beim verwendeten Desktop wird das vom GPIO-Taster erzeugte Power-Tasten-Ereignis durch labwc verarbeitet. Wenn dabei das Ausschaltmenü erscheint, die Tastenzuordnung auf `systemctl poweroff` ändern.

Die Befehle als Benutzer der grafischen Sitzung ausführen, im getesteten Aufbau `pi2`. Bei einem anderen Desktop-Benutzer dessen Konfiguration bearbeiten.

```bash
mkdir -p ~/.config/labwc
[ -f ~/.config/labwc/rc.xml ] || cp /etc/xdg/labwc/rc.xml ~/.config/labwc/rc.xml
cp ~/.config/labwc/rc.xml ~/.config/labwc/rc.xml.pwr-latcher.bak
nano ~/.config/labwc/rc.xml
```

Innerhalb des vorhandenen `<keyboard> ... </keyboard>`-Abschnitts folgenden Eintrag ergänzen. Falls bereits eine Zuordnung für `XF86PowerOff` existiert, diese ersetzen und keine zweite anlegen:

```xml
<keybind key="XF86PowerOff" onRelease="yes">
  <action name="Execute">
    <command>systemctl poweroff</command>
  </action>
</keybind>
```

Beispiel mit den vorhandenen Wiederholungseinstellungen:

```xml
<keyboard>
  <repeatRate>25</repeatRate>
  <repeatDelay>600</repeatDelay>
  <keybind key="XF86PowerOff" onRelease="yes">
    <action name="Execute">
      <command>systemctl poweroff</command>
    </action>
  </keybind>
</keyboard>
```

Nur den betreffenden Eintrag ergänzen; andere Einstellungen erhalten. Die Datei darf vollständig in einer Zeile stehen. In diesem Fall vor `</keyboard>` einfügen. Im getesteten Image heißt das äußere XML-Element `<openbox_config>`; dieses beibehalten.

Speichern in nano: **Strg+O**, Enter, **Strg+X**.

Im Terminal direkt auf dem Pi-Desktop neu laden:

```bash
labwc --reconfigure
```

Falls über SSH die Meldung `LABWC_PID not set` erscheint, als derselbe Benutzer wie die grafische Sitzung ausführen:

```bash
pkill -HUP -x labwc
```

Alternativ neu starten. Danach fährt der Pi beim Loslassen des Tasters direkt herunter. Diese Zuordnung gilt auch für andere Power-Tasten-Ereignisse, die labwc in dieser Sitzung empfängt.

## 4. Verzögerung der Un-Latch-Schaltung

Verwendeter RC-Zweig laut Aufbau:

| Bauteil | Bestückung |
|---|---|
| Diode im Signalweg | BAT54 |
| Ladewiderstand | 160 kΩ |
| Kondensatoren am Gate gegen GND | 3 × 22 µF parallel = 66 µF |
| Entladewiderstand am Gate gegen GND | 1 MΩ |
| Un-Latch-MOSFET | 2N7002 |

Signalweg: GPIO15 über BAT54 in Durchlassrichtung und 160 kΩ zum Gate-Knoten. Am Gate-Knoten liegen 66 µF und 1 MΩ gegen GND. Die übrige MOSFET- und Latch-Beschaltung richtet sich nach dem Schaltplan. Bei polarisierten Kondensatoren liegt Plus am Gate-Knoten und Minus an GND.

Die Bestückung mit **3 × 22 µF parallel wurde im Aufbau als passend bestätigt**. Eine exakt gemessene Verzögerungszeit wurde dabei nicht festgehalten.

`R × C` ist die Zeitkonstante und nicht automatisch die Abschaltzeit. Der 1-MΩ-Widerstand bildet außerdem einen Spannungsteiler und verändert die Zeitkonstante. Mit 160 kΩ und 66 µF beträgt sie näherungsweise:

```text
τ = (160 kΩ parallel 1 MΩ) × 66 µF ≈ 9,1 s
```

Die tatsächliche Auslösezeit hängt von Gatespannung, MOSFET-Streuung, Last der Un-Latch-Schaltung, Diodenspannung und Kondensatortoleranz bzw. Leckstrom ab. Größere Kapazität verlängert die Verzögerung ungefähr proportional. Nach kurzem Aus- und Wiedereinschalten kann Restladung die Zeit verkürzen; vor einem Vergleichstest ausreichend entladen lassen.

Das Standard-Overlay bietet keine Wartezeit vor der ersten Aktivierung von GPIO15. `active_delay_ms` verlängert den ersten aktiven Impuls; es verzögert dessen Beginn nicht. Die gewünschte Wartezeit nach dem Shutdown übernimmt hier die externe RC-Schaltung.

## 5. Funktionstest und Fehlersuche

1. Einschalten: Die Versorgung muss während des gesamten Bootens eingeschaltet bleiben.
2. Im Betrieb: GPIO15 soll LOW sein, GPIO14 bei offenem Taster HIGH.
3. Taster drücken und loslassen: Der Pi muss ohne Menü sauber herunterfahren.
4. Am Ende des Shutdowns: GPIO15 wird HIGH; danach schaltet der Latcher verzögert ab.
5. Auch einen erneuten Einschaltvorgang nach kurzer Pause testen.

Die tatsächliche Verzögerung lässt sich mit dem Oszilloskop vom ersten HIGH an GPIO15 bis zum Wegfall der Versorgung messen. Das Ende der sichtbaren Konsolenausgabe ist dafür kein genauer Zeitbezug.

| Problem | Prüfen |
|---|---|
| Ausschaltmenü erscheint weiterhin | `XF86PowerOff` im richtigen Benutzerprofil ändern, labwc neu laden |
| `LABWC_PID not set` | Über SSH `pkill -HUP -x labwc` als Desktop-Benutzer verwenden oder neu starten |
| Taster ohne Wirkung | BCM14 entspricht Pin 8; Taster gegen GND; Overlay aktiv; UART-Konflikt ausschließen |
| Versorgung fällt schon beim Booten aus | GPIO15-Anfangszustand, Signalpolarität und Hardware-Beschaltung prüfen |
| Verzögerung zu kurz oder schwankend | Gate-Spannung messen; Restladung, Kapazität und Leckstrom prüfen |
| Keine Abschaltung nach Shutdown | HIGH an GPIO15, Diodenrichtung, gemeinsame Masse und MOSFET-Beschaltung prüfen |

## Referenzen

- [Raspberry Pi: config.txt und GPIO-Anfangszustände](https://www.raspberrypi.com/documentation/computers/config_txt.html)
- [Raspberry Pi: Overlay-Parameter für gpio-shutdown und gpio-poweroff](https://github.com/raspberrypi/firmware/blob/master/boot/overlays/README)
- [Raspberry Pi: labwc-Standardkonfiguration](https://github.com/raspberrypi-ui/raspberrypi-ui-mods/blob/master/etc/xdg/labwc/rc.xml)
- [Linux: gpio-poweroff-Treiber und Signalabfolge](https://github.com/torvalds/linux/blob/master/drivers/power/reset/gpio-poweroff.c)
