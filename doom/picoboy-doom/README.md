# PicoBoy Color Plus Doom — Build-fertige Version

Drop-in build der Pico-Doom-Portierung von Graham Sanderson (`rp2040-doom-rp2`,
GPLv2) für den **PicoBoy Color Plus** (RP2350, 16 MB Flash, ST7789 240×280 SPI,
5-Wege-Joystick + A/B, Piezo).

## Status

| Bereich | Stand |
|---|---|
| CMake + Toolchain | ✅ konfiguriert für Pico-SDK 2.1.0 / RP2350 |
| Compile aller 182 Quellen | ✅ |
| Linker | ✅ alle Symbole aufgelöst |
| UF2 erzeugen | ✅ `src\doom_tiny.uf2` |
| Hardware-Pfad (Backlight + SPI + ST7789) | ✅ verifiziert mit Test-Pattern |
| Doom-Renderer auf Display | ⚠️ noch offen — dieser Build aktiviert Doom-Mode (`PICOBOY_TEST_PATTERN = 0`); falls schwarz, kannst du wieder auf Test-Pattern umschalten |

## Tastenbelegung

| PicoBoy           | Doom-Aktion                            |
|-------------------|----------------------------------------|
| Joystick UP       | Vor                                    |
| Joystick DOWN     | Zurück                                 |
| Joystick LEFT     | Links drehen                           |
| Joystick RIGHT    | Rechts drehen                          |
| CENTER            | Benutzen / Tür                         |
| Button B          | Schießen                               |
| Button A          | Waffe weiter (1 → 2 → … → 7 → 1)       |
| **A + CENTER**    | **Menü (ESC)** — wichtig zum Starten   |

## Build (PowerShell)

```powershell
cd <pfad-zum-repo>\doom\picoboy-doom
Remove-Item -Recurse -Force build-picoboy -ErrorAction SilentlyContinue
New-Item -ItemType Directory build-picoboy | Out-Null
cd build-picoboy

$env:PICO_SDK_PATH       = "$env:USERPROFILE\.pico-sdk\sdk\2.1.0"
$env:PICO_EXTRAS_PATH    = "$env:USERPROFILE\.pico-sdk\pico-extras"
$env:PICO_TOOLCHAIN_PATH = "$env:USERPROFILE\.pico-sdk\toolchain\13_3_Rel1"
$env:Path = "$env:PICO_TOOLCHAIN_PATH\bin;" +
            "$env:USERPROFILE\.pico-sdk\cmake\v3.29.9\bin;" +
            "$env:USERPROFILE\.pico-sdk\ninja\v1.12.1;" +
            "$env:USERPROFILE\.pico-sdk\picotool\2.2.0-a4\picotool;" +
            "$env:Path"

cmake -G Ninja `
      -DCMAKE_BUILD_TYPE=MinSizeRel `
      -DPICO_PLATFORM=rp2350-arm-s `
      -DPICO_BOARD=pico2 `
      -DPICO_FLASH_SIZE_BYTES=16777216 `
      -DPICO_SDK_PATH="$env:PICO_SDK_PATH" `
      -DPICO_EXTRAS_PATH="$env:PICO_EXTRAS_PATH" `
      -Dpioasm_DIR="$env:USERPROFILE\.pico-sdk\tools\2.1.0\pioasm" `
      -Dpicotool_DIR="$env:USERPROFILE\.pico-sdk\picotool\2.2.0-a4\picotool" `
      ..

cmake --build . --target doom_tiny
picotool load -v src\doom_tiny.uf2

cd ..
# WAD ist NICHT in dieser ZIP -- aus deinem Original-Verzeichnis kopieren
# oder von einer Shareware-Quelle besorgen, dann:
picotool load -v -t bin doom1.whx -o 0x10041000
picotool reboot
```

## Was du nach dem Flashen siehst

**Erwartung:** Doom-Demo läuft, Spielbild erscheint mittig im 240×180-Bereich,
oben/unten je 50 px schwarz. Tastenbelegung wie oben.

**Falls schwarzes Display:** Hardware ist okay (Test-Pattern hat funktioniert),
also klemmt's in der Doom-Renderer-Pipeline → mein Hook `picoboy_finish_update`
in `src/pico/i_video.c` greift `frame_buffer[]` + `palette[]` möglicherweise
nicht richtig ab. Dann zur Diagnose zurückschalten:

1. In `src\pico\picoboy_config.h` den `PICOBOY_TEST_PATTERN`-Wert auf `1` setzen.
2. Neu bauen + flashen (ohne WAD-Schritt nötig).
3. Wenn das Test-Pattern erscheint, bestätigt das: Display okay, Doom-Pipeline kommt nicht an.

## Was wo geändert wurde (relativ zum Upstream rp2040-doom-rp2)

```
src/pico/picoboy_config.h            NEU — Pins (SPI0, SCK=GP18, MOSI=GP19), Maße
src/pico/picoboy_display.h           NEU — ST7789-Treiber-Header
src/pico/picoboy_display.c           NEU — Treiber-Impl mit DMA, MADCTL=0x08 (BGR)
src/pico/i_input_picoboy.c           NEU — Tastenbelegung mit Waffen-Cycle, A+CENTER→ESC
src/pico/i_picosound_picoboy.c       NEU — stiller sound_pico_module + I_PicoSound*-Stubs
src/pico/i_video.c                   PATCHED — fünf #ifdef PICOBOY_BUILD-Blöcke:
                                       1) #include
                                       2) scanvideo-Init übersprungen
                                       3) picoboy_display_init/_input_init in I_InitGraphics
                                       4) picoboy_input_poll in I_StartTic
                                       5) picoboy_finish_update am Dateiende
src/pico/CMakeLists.txt              PATCHED — neue Quellen, hardware_spi/dma/pwm,
                                       Dummy-I2S-Pin-Defines (für bi_decl in i_main.c)
```

## Lizenz

GPLv2 (Doom-Quellcode by id Software, Chocolate-Doom-Stack, rp2040-doom by
Graham Sanderson). Alle PicoBoy-Anpassungen ebenfalls GPLv2.
