/*
 * rom.h
 *
 * ROM-Zugriffsschicht.
 *
 * Es gibt zwei Wege, ein .gb-ROM auf den PicoBoy zu bekommen:
 *
 *  (A) [DEFAULT, empfohlen]  ROM lebt in einer reservierten Flash-Region.
 *      Beginnt bei XIP_BASE + ROM_FLASH_OFFSET (= 0x10100000, also 1 MB
 *      ab Flash-Start). Der Emulator-Code liegt vorne, das ROM dahinter.
 *      Vorteile:
 *        - Firmware muss nur EINMAL geflasht werden.
 *        - Spieltausch ohne Neukompilieren.
 *      Wie laden? Drei Optionen:
 *        1. picotool (offizielles Raspberry-Pi-Tool):
 *             picotool load --offset 0x10100000 game.gb -f
 *           Pico im BOOTSEL-Modus, Spiel ist sofort drauf.
 *        2. Mit dem mitgelieferten Skript tools/gb_to_uf2.py eine UF2
 *           erzeugen, die nur den ROM-Bereich beschreibt:
 *             python tools/gb_to_uf2.py game.gb game.uf2
 *           Pico im BOOTSEL-Modus, game.uf2 auf RPI-RP2 ziehen, fertig.
 *        3. Eigener UF2-Loader / OpenOCD - falls du sowas eh hast.
 *
 *  (B) [Fallback]  ROM in die Firmware einbacken.
 *      Mit  #define USE_BAKED_ROM  vor dem #include "rom.h" wird das
 *      Array aus rom_data.h benutzt. So erstellst du rom_data.h:
 *           xxd -i game.gb > rom_data.h
 *      Dann oben in rom_data.h das Array auf
 *           const unsigned char baked_rom[] PROGMEM = {...};
 *           const unsigned int baked_rom_len = ...;
 *      umbenennen.
 *
 * Beim Start prueft der Emulator zuerst (A): liegt bei
 * XIP_BASE+ROM_FLASH_OFFSET+0x104 die Nintendo-Logo-Bytefolge?
 * Wenn ja, wird dieses ROM benutzt. Sonst Fallback auf (B), sonst
 * Hinweistext auf dem Display.
 */

#ifndef PICOBOY_ROM_H
#define PICOBOY_ROM_H

#include <stdint.h>
#include <stddef.h>
#include <hardware/flash.h>      // XIP_BASE
#include <hardware/regs/addressmap.h>

#ifndef ROM_FLASH_OFFSET
#define ROM_FLASH_OFFSET   (1u * 1024u * 1024u)   // 1 MB ab Flash-Start
#endif

// Maximalgroesse einer DMG-ROM ist 1 MB (32 Mbit). Wir geben dem
// ROM-Bereich 2 MB Luft - reicht auch fuer Spezialitaeten.
#define ROM_FLASH_SIZE     (2u * 1024u * 1024u)

static const uint8_t *const ROM_FLASH_PTR =
    (const uint8_t *)(XIP_BASE + ROM_FLASH_OFFSET);

// Die ersten 16 Bytes des Nintendo-Logos (steht ab 0x104 in jedem
// gueltigen GB-ROM). Wird zur Existenzpruefung benutzt.
static const uint8_t kNintendoLogoHead[16] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D
};

// True, wenn an der Standard-Flash-Position ein gueltig aussehendes ROM liegt.
static inline bool rom_flash_present(void) {
    const uint8_t *p = ROM_FLASH_PTR + 0x104;
    for (int i = 0; i < 16; ++i) {
        if (p[i] != kNintendoLogoHead[i]) return false;
    }
    return true;
}

#ifdef USE_BAKED_ROM
#include "rom_data.h"   // muss baked_rom[] und baked_rom_len definieren
#endif

#endif // PICOBOY_ROM_H
