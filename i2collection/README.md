# i2collection — Zweispieler-Sammlung über USB-C

Neun Spiele für **zwei** PicoBoy Color Plus, die über ein USB-C-Kabel
miteinander verbunden sind: Tron (Lightcycle), Pong, Duell, Artillerie,
Bombing Bob, Schach, Wer-nicht-rechtzeitig, Vier gewinnt und Käsekästchen.

```bash
./build.sh          # -> ../dist/i2collection.uf2
```

Gebaut wird über PlatformIO, nicht über das Pico SDK. Einrichtung siehe
[`../docs/BAUEN.md`](../docs/BAUEN.md).

## Verkabelung

USB-C ↔ USB-C: **D+, D− und GND verbinden, VBUS nicht** — beide Geräte
versorgen sich selbst. Dasselbe Kabel wie bei `jumpnbump` und `doom`.

## Rollenwahl

**Eine Firmware für beide Geräte.** Wer Host und wer Gast ist, wird zur
Laufzeit im Menü entschieden:

* **HOST** → das Gerät wird nativer USB-Host und taktet die Verbindung.
* **JOIN** → das Gerät wird USB-Device und wartet.

Möglich macht das eine eigene Dual-Role-Konfiguration
(`include/tusb_config_dualrole.h`): Device- und Host-Anteil von TinyUSB sind
beide einkompiliert, die Rolle wird erst nach der Menüauswahl gestartet.

## Die Verbindung

Früher lief sie über I²C an SDA=GP20/SCL=GP21, heute über die native
USB-Schnittstelle. Die Klasse `LinkClass` behielt dabei absichtlich die
I²C-artigen Methodennamen (`beginTransmission`, `requestFrom`, `write`), damit
die neun Spiele unverändert bleiben konnten. `I2C_ADDR` ist deshalb keine
I²C-Adresse mehr, sondern nur noch eine Kennung im USB-Protokoll.

Nach einem Neustart zeigt das Menü, was zuletzt lief — der Verbindungszustand
wird über den Reset hinweg gehalten, sodass man nach einem Absturz direkt
weiterspielen kann. Ein Watchdog fängt eingefrorene Verbindungen ab und führt
beide Geräte zurück ins Menü, statt mit stehendem Bild hängen zu bleiben.

## TinyUSB-Patch

`tools/patch_tinyusb.py` spielt vor **jedem** Bau die Dateien aus `patches/`
in das Framework-Paket ein. Sie beheben eine Endlosschleife im USB-Interrupt,
die sonst den ganzen Chip anhält. Deshalb ist die Framework-Version in
`platformio.ini` festgenagelt — der Patch muss zur Version passen. Näheres in
[`../docs/BAUEN.md`](../docs/BAUEN.md), Abschnitt „PlatformIO".

## Warum `.cpp` und nicht `.ino`

PlatformIOs automatische Prototyp-Erzeugung für `.ino`-Dateien scheitert hier:
sie setzt Prototypen vor die Definition des Typs `Ship`, was zu „not declared"
führt. Als `.cpp` übersetzt PlatformIO direkt als C++ ohne diese Erzeugung.

## Lizenz

MIT, siehe `LICENSE`. Verwendet Adafruit GFX und Adafruit ST7789 (beide BSD)
sowie TinyUSB (MIT).
