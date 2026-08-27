#
# OpenTyrian fuer den PicoBoy Color Plus (RP2350, 16 MB Flash).
#
# Bauen:
#   cmake -B build -DPICO_SDK_PATH=$HOME/pico/pico-sdk
#   cmake --build build -j
#
# Ergebnis: build/tyrian.uf2 (Firmware). Die Spieldaten kommen getrennt als
# zweites UF2 aus tools/mkdata.py -- siehe README.md.
#
# GPLv2, wie OpenTyrian.
#

cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM rp2350-arm-s CACHE STRING "")
set(PICO_BOARD pico2 CACHE STRING "")

# Das Board meldet von sich aus nur 4 MB. Ohne diese Zeile verschenkt man
# 12 MB -- und die Spieldaten sind fast 9 MB gross.
set(PICO_FLASH_SIZE_BYTES 16777216 CACHE STRING "")

include(pico_sdk_import.cmake)

project(pbc_tyrian C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

# ---------------------------------------------------------------------------
# OpenTyrian-Kern
# ---------------------------------------------------------------------------
#
# Die Quellen unter src/ sind bis auf wenige, in PATCHES.md einzeln begruendete
# Aenderungen der unveraenderte OpenTyrian-Upstream. Was ersetzt wurde, wurde
# geloescht statt umgeschrieben:
#
#   video.c            -> platform/pbc_video.c   (Panel statt Fenster)
#   video_scale*.c     -> entfaellt              (Skalierer fuer Desktop-Fenster)
#
# Alles Uebrige uebersetzt unveraendert gegen die SDL-Attrappe in
# platform/fakesdl/.
#
file(GLOB TYRIAN_SOURCES ${CMAKE_CURRENT_LIST_DIR}/src/*.c)

add_library(tyrian_core STATIC ${TYRIAN_SOURCES})

target_include_directories(tyrian_core PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/platform/fakesdl   # muss VOR allem anderen stehen
    ${CMAKE_CURRENT_LIST_DIR}/platform
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Auf dem Geraet gibt es keine Verzeichnisse -- das Flash-Archiv kennt nur
# Dateinamen. data_dir()/get_user_directory() liefern deshalb einen leeren
# Pfad, den xipfs ohnehin wegschneidet.
target_compile_definitions(tyrian_core PUBLIC
    TYRIAN_DIR=""

    # OpenTyrians gesamter Netzwerkcode steckt hinter diesem Schalter. Ohne ihn
    # gibt es network.c praktisch nicht -- und damit auch keinen Multiplayer.
    # Darunter liegt hier nicht SDL_net, sondern die Attrappe in
    # platform/fakesdl/SDL_net.h ueber dem USB-Link.
    #
    # PUBLIC, nicht PRIVATE: die Plattformschicht (pbc_netmenu.c) haengt
    # ebenfalls daran. Sonst wird sie still zu einer leeren Datei uebersetzt
    # und der Fehler zeigt sich erst als fehlendes Symbol beim Binden.
    WITH_NETWORK=1
)

# pbc_prelude.h biegt fopen/fread/... auf das Flash-Archiv um. Nur die
# OpenTyrian-Quellen bekommen es -- die Plattformdateien brauchen echtes stdio.
# Nur auf C-Dateien: die Pico-SDK bringt ueber ihre INTERFACE-Bibliotheken
# auch Assemblerquellen mit, und ein -include davor laesst den Assembler
# C-Deklarationen als Befehle lesen.
target_compile_options(tyrian_core PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include>
    $<$<COMPILE_LANGUAGE:C>:${CMAKE_CURRENT_LIST_DIR}/platform/pbc_prelude.h>
)

# Bewusst KEIN pico_stdlib hier: die INTERFACE-Bibliotheken der SDK schleusen
# eigene Quellen (crt0.S, Laufzeit) in jedes Ziel, das sie einbindet -- die
# gehoeren einmal in die ausfuehrbare Datei, nicht zusaetzlich in diese
# Bibliothek. Die OpenTyrian-Quellen brauchen ohnehin keine SDK-Kopfdateien;
# sie sehen die Hardware nur ueber die Plattformschicht.
target_link_libraries(tyrian_core PUBLIC pico_base_headers)

# OpenTyrians main() heisst hier opentyrian_main(); der echte Startpunkt steht
# in platform/pbc_main.c und richtet vorher die Hardware ein.
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/opentyr.c
    PROPERTIES COMPILE_DEFINITIONS "main=opentyrian_main"
)

# ---------------------------------------------------------------------------
# Plattformschicht
# ---------------------------------------------------------------------------
add_executable(tyrian
    platform/pbc_main.c
    platform/pbc_display.c
    platform/pbc_video.c
    platform/pbc_hud.c
    platform/pbc_input.c
    platform/pbc_audio.c
    platform/pbc_link.c
    platform/pbc_netmenu.c
    platform/usblink.c
    platform/xipfs.c
)

target_include_directories(tyrian PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/platform/fakesdl
    ${CMAKE_CURRENT_LIST_DIR}/platform
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(tyrian
    tyrian_core
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_gpio
    hardware_clocks
    hardware_flash
    hardware_sync
)

# Auf Tempo statt auf Groesse.
#
# Anfangs stand hier -Os, aus Sorge um das erste Megabyte Flash (danach beginnt
# das Datenarchiv, siehe PBC_XIPFS_ADDR). Die Sorge war unbegruendet: die
# Firmware braucht rund ein Viertel davon. Und der Engpass ist inzwischen
# nachweislich die Spiellogik, nicht die Bildausgabe -- die schiebt nur noch
# 150 der 280 Zeilen.
target_compile_options(tyrian_core PRIVATE -O2 -fno-strict-aliasing)
target_compile_options(tyrian PRIVATE -O2 -fno-strict-aliasing)

# Der Multiplayer-Link belegt den USB-Port. Die Ausgaben gehen deshalb auf UART,
# nicht auf USB-CDC -- sonst waeren Diagnose und Link nicht gleichzeitig
# moeglich.
# exit()/abort() gibt es auf einem Geraet ohne Betriebssystem nicht sinnvoll --
# umgelenkt auf eine lesbare Meldung, die stehenbleibt. Siehe pbc_main.c.
target_link_options(tyrian PRIVATE -Wl,--wrap=exit -Wl,--wrap=abort -Wl,--wrap=panic -Wl,--wrap=hard_assertion_failure)

# Stapelwaechter: die MPU loest aus, sobald der Stapel unter sein Ende laeuft.
# Ohne ihn frisst ein Ueberlauf still die Spieldaten -- genau das hat diesen
# Port zuerst lahmgelegt (JE_loadMap brauchte 76 KB auf einem 4-KB-Stapel).
# Zusammen mit isr_hardfault in pbc_main.c wird daraus eine lesbare Meldung
# statt eines schwarzen Bildschirms.
target_compile_definitions(tyrian PRIVATE
    PICO_USE_STACK_GUARDS=1

    # Voreinstellung sind 2 KB. Der Stapel liegt in SCRATCH_Y, und das sind
    # 4 KB -- die zweite Haelfte lag ungenutzt brach, waehrend der Waechter
    # bereits bei 2 KB ausloeste.
    PICO_STACK_SIZE=0x1000
)

# Musik ist AUS.
#
# Die OPL-Emulation bildet den Klangchip in Software nach und rechnet je
# Ausgabewert -- sie ist der groesste Rechenzeitposten der Tonausgabe und
# laeuft in einer Unterbrechung. Die Klaenge (Schuesse, Explosionen) bleiben
# davon unberuehrt, die belasten den Mischer kaum.
#
# Wieder einschalten: cmake -B build -DPBC_MUSIC=ON
option(PBC_MUSIC "Musik (OPL-Emulation) einschalten" OFF)
if(NOT PBC_MUSIC)
    # Auch fuer tyrian_core: loudness.c gehoert dorthin, und nur dort laesst
    # sich der Musikzweig wirklich herausuebersetzen. Ein Laufzeit-Flag
    # genuegt nicht -- Laden und Optionsmenue setzen es selbst zurueck.
    target_compile_definitions(tyrian_core PUBLIC PBC_NO_MUSIC=1)
endif()

pico_enable_stdio_usb(tyrian 0)
pico_enable_stdio_uart(tyrian 1)

pico_add_extra_outputs(tyrian)
