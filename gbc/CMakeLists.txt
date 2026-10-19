# PBC Game Boy Color -- Game Boy and Game Boy Color emulator for the
# PicoBoy Color Plus
#
# Emulator core Peanut-GB with Game Boy Color support (tvecera's
# gbc-rtc-fix branch, via PicoPlus-devel/pico-peanutGB), sound minigb_apu
# from ../picoboygb; platform layer from ../nes and ../c64, flash FatFs +
# USB stick from ../picoboygb. See README.md.
#
#   ./build.sh                 or by hand:
#   cmake -S . -B build -DPICO_SDK_PATH=... && cmake --build build -j
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s)
endif()
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2)
endif()

# ---- tuning knobs ----------------------------------------------------------
# 250 MHz at 1.25 V is what PicoBoyGB runs on this board. Above 1.30 V the RP2350 voltage limit is lifted in
# main.c -- the board's small buck-boost converter may not like it.
set(CPU_MHZ "200" CACHE STRING "System clock in MHz")
set(CPU_VREG "VREG_VOLTAGE_1_15" CACHE STRING "Core voltage (VREG_VOLTAGE_1_10 .. VREG_VOLTAGE_1_60)")
set(FLASH_MAX_FREQ_MHZ "100" CACHE STRING "Upper limit for the flash (XIP) clock in MHz")
set(PBC_SPI_HZ "75000000" CACHE STRING "ST7789 SPI clock request in Hz")
set(SHOW_FPS "0" CACHE STRING "Show fps and load under the picture: 0/1")

include(pico_sdk_import.cmake)

project(pbc_gbc C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(GBCORE ${CMAKE_CURRENT_LIST_DIR}/core)

add_executable(pbc_gbc
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/video.c
    src/audio_pwm.c
    src/input.c
    src/menu.c
    src/rom_store.c
    src/settings.c
    src/flash_fs.c
    src/usb_msc.c
    src/usb_descriptors.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c

    # Peanut-GB (single header, implemented in src/main.c) and minigb_apu
    ${GBCORE}/minigb_apu.c
)

target_include_directories(pbc_gbc PRIVATE
    src                 # tusb_config.h
    fatfs
    ${GBCORE}
)

target_compile_definitions(pbc_gbc PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
    PICO_STACK_SIZE=0x1000
    # SDK 2.2 moves clk_peri to the fixed 48 MHz USB clock whenever the
    # system clock is changed; the display SPI could then never exceed
    # 24 MHz (measured: 23 fps instead of 60 on the NES). Keep clk_peri on
    # the system clock as SDK 2.1 did.
    PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1
    PICO_USE_STACK_GUARDS=1
    CPU_MHZ=${CPU_MHZ}
    CPU_VREG=${CPU_VREG}
    FLASH_MAX_FREQ_MHZ=${FLASH_MAX_FREQ_MHZ}
    PBC_SPI_HZ=${PBC_SPI_HZ}u
    SHOW_FPS=${SHOW_FPS}
    # Peanut-GB with the Game Boy Color extension; minigb_apu at the rate
    # of the speaker timer (src/audio_pwm.h)
    PEANUT_FULL_GBC_SUPPORT=1
    AUDIO_SAMPLE_RATE=22222
)

target_compile_options(pbc_gbc PRIVATE
    -O2
)

target_link_libraries(pbc_gbc
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_flash
    hardware_sync
    hardware_clocks
    hardware_vreg
    hardware_watchdog
    tinyusb_device
    tinyusb_board
)

# USB belongs to the mass-storage stick, and UART0's default pins are
# joystick CENTER/RIGHT -- no stdio at all.
pico_enable_stdio_usb(pbc_gbc 0)
pico_enable_stdio_uart(pbc_gbc 0)

# UF2 etc. -- or, for the combined firmware (pbcemu/build.sh), a .bin
# linked behind the launcher.
include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_gbc)
