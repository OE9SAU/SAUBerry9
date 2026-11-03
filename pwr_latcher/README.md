# Power-Latcher für Raspberry Pi 4 / Radioberry

Der Taster löst einen sauberen Shutdown aus. Danach signalisiert GPIO15 der Un-Latch-Schaltung, die Versorgung verzögert abzuschalten. Ein eigenes Shutdown-Skript ist nicht erforderlich.
**Ablauf:** Taster loslassen → sauberer Shutdown → GPIO15 HIGH → RC-Verzögerung → Versorgung aus.

## Anschlüsse

| Funktion | BCM / GPIO | Physischer Pin |
|---|---|---|
| Shutdown-Taster gegen GND | GPIO14 | 8 |
| Un-Latch, aktiv HIGH | GPIO15 | 10 |
| Masse | GND | z. B. 9 |

GPIO14/15 dürfen nicht gleichzeitig vom UART oder der seriellen Konsole verwendet werden. Falls nötig unter `sudo raspi-config` → **Interface Options → Serial Port** beide deaktivieren. GPIO-Pegel: **3,3 V**.

## Raspberry-Pi-Einstellungen

```bash
sudo nano /boot/firmware/config.txt
```

Am Ende ergänzen; vorhandene gleiche Overlay-Einträge anpassen statt doppelt eintragen:

```ini
[all]

# Taster → Shutdown (GPIO14, Pin 8, gegen GND)
dtoverlay=gpio-shutdown,gpio_pin=14,active_low=1,gpio_pull=up

# GPIO15 / Pin 10 beim Booten als Ausgang auf LOW
gpio=15=op,dl

# Poweroff-Signal → Un-Latch (GPIO15, Pin 10, aktiv HIGH)
dtoverlay=gpio-poweroff,gpiopin=15
```

Beim Poweroff-Overlay **kein `active_low=1`** ergänzen. GPIO15 bleibt nach der Initialisierung LOW und wird am Ende des Shutdowns HIGH. Der Pegel unmittelbar nach dem Einschalten muss bei Bedarf durch die Hardware abgesichert sein.

Speichern und neu starten:

```bash
sudo reboot
```

## labwc: direkt herunterfahren, ohne Menü

Als Benutzer des Pi-Desktops ausführen:

```bash
mkdir -p ~/.config/labwc
[ -f ~/.config/labwc/rc.xml ] || cp /etc/xdg/labwc/rc.xml ~/.config/labwc/rc.xml
cp ~/.config/labwc/rc.xml ~/.config/labwc/rc.xml.bak
nano ~/.config/labwc/rc.xml
```

Direkt **vor `</keyboard>`** einfügen. Einen vorhandenen `XF86PowerOff`-Eintrag ersetzen:

```xml
<keybind key="XF86PowerOff" onRelease="yes">
  <action name="Execute">
    <command>systemctl poweroff</command>
  </action>
</keybind>
```

Andere Einstellungen erhalten; XML darf auch in einer Zeile stehen. Speichern mit **Strg+O**, Enter, **Strg+X**.

Konfiguration neu laden, auch über SSH als Desktop-Benutzer:

```bash
pkill -HUP -x labwc
```

Der Taster löst nun beim Loslassen direkt den Shutdown aus.
