# Bauen

Wer nur spielen will, braucht das hier nicht: die fertigen `.uf2` liegen in
`dist/`. BOOTSEL gedrückt halten, USB anstecken, Datei auf das erscheinende
Laufwerk kopieren, fertig.

Dieser Text ist für alle, die selbst bauen wollen.

## Kurzfassung

```bash
sudo apt-get install -y cmake ninja-build gcc-arm-none-eabi \
                        build-essential libusb-1.0-0-dev pkg-config

./build_all.sh                 # alles
cd spout && ./build.sh         # oder ein einzelnes Projekt
```

Beim ersten Lauf werden Pico SDK 2.1.0, `pico-extras` und `picotool` nach
`~/.cache/pbcp-sdks` geholt. Das dauert ein paar Minuten und passiert nur
einmal — alle Projekte teilen sich diesen Zwischenspeicher.

## Was wofür gebraucht wird

| Werkzeug | Wofür |
|---|---|
| `cmake` | Bausystem aller Pico-SDK-Projekte |
| `arm-none-eabi-gcc` | Übersetzer für den RP2350 (ARM Cortex-M33) |
| `gcc` / `build-essential` | Übersetzer fürs eigene System: `picotool` und die Hilfswerkzeuge |
| `git` | holt SDK, `pico-extras` und MicroPython |
| `python3` | Hilfsskripte des SDK, Datenpacker von Tyrian |
| `ninja` | optional, nur schneller — ohne ihn wird Make benutzt |
| `libusb-1.0-0-dev` | nur nötig, wenn `picotool` per USB flashen soll |
| PlatformIO | nur für `jumpnbump` und `i2collection` |

Fehlt etwas, sagt das jeweilige `build.sh` beim Start genau, was fehlt, wofür
es gebraucht wird und wie man es auf dem erkannten System nachinstalliert.

## Andere Distributionen

```bash
# Fedora
sudo dnf install -y cmake ninja-build arm-none-eabi-gcc-cs gcc gcc-c++ make git python3

# Arch
sudo pacman -S --needed cmake ninja arm-none-eabi-gcc base-devel git python

# macOS
brew install cmake ninja git python3
brew install --cask gcc-arm-embedded
```

## Ein einzelnes Projekt bauen

Jeder Projektordner hat ein eigenes `build.sh`. Es kümmert sich selbst um das
SDK und legt sein Ergebnis in `dist/` ab:

```bash
cd noiz2sa
./build.sh          # bauen
./build.sh clean    # Bauordner löschen und neu bauen
```

`build_all.sh` ist nur eine Schleife über genau diese Skripte — es entscheidet
nichts eigenständig.

## PlatformIO (nur jumpnbump und i2collection)

Diese beiden bauen nicht über das Pico SDK, sondern über den Arduino-Kern
`arduino-pico`. PlatformIO ist ein Python-Programm und gehört in eine eigene
Umgebung, damit es sich nicht mit Systempaketen ins Gehege kommt:

```bash
python3 -m venv ~/.pio-venv
~/.pio-venv/bin/pip install platformio
PATH=$HOME/.pio-venv/bin:$PATH ./build_all.sh
```

Beim ersten Lauf lädt PlatformIO Kern und Werkzeugkette nach — einige hundert
MB, einmalig.

**Die Version ist mit Absicht festgenagelt.** In `platformio.ini` steht

```ini
platform = https://github.com/maxgerhardt/platform-raspberrypi.git#bd6fb6a
```

Das ist der Stand „Update to Arduino-Pico 5.6.1". Der Grund: `tools/patch_tinyusb.py`
korrigiert vor jedem Bau Dateien **im** Framework-Paket (eine Endlosschleife im
USB-Interrupt, die sonst den ganzen Chip anhält), und dieser Eingriff muss zur
Version passen. Zeigt `platform` stattdessen auf den neuesten Stand, zieht
PlatformIO Arduino-Pico 6.0.0 samt neuerer Werkzeugkette — und die übersetzt
den 5.6.1-Kern nicht mehr:

```
'__getreent' was not declared in this scope   (malloc-lock.cpp)
```

Wer die Version anhebt, muss den Patch mit anheben.

## Ohne Internet bauen

Die Fremdquellen lassen sich auch selbst bereitstellen. Wenn diese Variablen
gesetzt sind, wird nichts nachgeladen:

```bash
export PICO_SDK_PATH=/pfad/zu/pico-sdk         # Version 2.1.0
export PICO_EXTRAS_PATH=/pfad/zu/pico-extras   # Zweig sdk-2.1.0, nur für Doom
export MICROPYTHON_PATH=/pfad/zu/micropython   # Version v1.28.0
```

PlatformIO braucht beim ersten Lauf zwingend Netzzugang; danach liegt alles
unter `~/.platformio`.

## Bekannte Klippen

**CMake 4 lehnt alte Projekte ab.** Seit Version 4 wird `cmake_minimum_required`
unter 3.5 nicht mehr akzeptiert. Das Pico SDK 2.1.0 und einige mitgelieferte
Fremdbibliotheken stammen aus der Zeit davor. Die Build-Skripte setzen deshalb
`CMAKE_POLICY_VERSION_MINIMUM=3.5`, sobald sie CMake 4 erkennen — ohne fremde
Quelltexte anzufassen.

**GCC 15 und der PIO-Assembler.** `pioasm` aus dem SDK benutzt `uint8_t`, ohne
`<cstdint>` einzubinden. Bis GCC 14 kam das über andere Header mit herein, GCC 15
räumt die Kette auf:

```
error: 'uint8_t' does not name a type
```

Die Build-Skripte tragen das fehlende `#include` im SDK-Zwischenspeicher nach.
Betrifft nur Projekte mit PIO-Programmen, etwa `micropolis`.

**Board und Plattform bei micropolis.** Als einziges Projekt setzt sein
`CMakeLists.txt` `PICO_BOARD` erst nach dem Einbinden von
`pico_sdk_import.cmake` — da steht die SDK-Vorgabe `pico` (RP2040) schon fest.
Ohne `-DPICO_PLATFORM=rp2350-arm-s -DPICO_BOARD=pico2` entsteht eine
RP2040-UF2. Die sieht gültig aus und **startet auf dem Gerät nicht**. Sein
`build.sh` gibt beides mit.

Damit so etwas nicht unbemerkt durchgeht, prüft jedes `build.sh` am Ende die
erzeugte Datei: UF2-Kennung, Blocklänge und ob wirklich RP2350-Blöcke drin
sind. Eine RP2040-UF2 führt zum Fehler statt zu einer stillen Fehlfunktion.

## Doom braucht eine WAD

Die Firmware allein spielt nichts. Die Level kommen aus einer WAD-Datei, die
als **zweite** UF2 an eine eigene Flash-Adresse geht (0x10048000). Das Werkzeug
dafür liegt in `doom/wad2uf2/`:

```bash
doom/wad2uf2/bin/whd_gen-linux-x86_64  DOOM.WAD     # -> DOOM.uf2
```

Unter Windows die `.exe` doppelklicken oder die WAD daraufziehen. Der genaue
Ablauf und die Flash-Adressen stehen in `doom/FLASHEN.md`.

Die WADs selbst sind kommerzielle Daten von id Software. Sie liegen diesem
Repo **nicht** bei und dürfen nicht weitergegeben werden — jede und jeder
benutzt die eigene Kopie. `.gitignore` schließt sie aus, damit sie nicht
versehentlich eingecheckt werden.
