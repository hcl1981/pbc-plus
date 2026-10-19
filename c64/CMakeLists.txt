# PBC C64 -- Commodore 64 emulator for the PicoBoy Color Plus
#
# Emulator core Frodo4 as ported in frank-c64
# (https://github.com/rh1tech/frank-c64); platform layer from ../nes and
# ../megadrive, flash FatFs + USB stick from ../picoboygb. See README.md.
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
# frank-c64 runs at 378 MHz and more, but with HDMI on the second core.
# Here core 0 measured under 30 % load at 300 MHz, so 200 MHz at barely
# raised voltage is plenty and spares the batteries. Above 1.30 V the RP2350
# voltage limit is lifted in main.c.
set(CPU_MHZ "200" CACHE STRING "System clock in MHz")
set(CPU_VREG "VREG_VOLTAGE_1_15" CACHE STRING "Core voltage (VREG_VOLTAGE_1_10 .. VREG_VOLTAGE_1_60)")
set(FLASH_MAX_FREQ_MHZ "88" CACHE STRING "Upper limit for the flash (XIP) clock in MHz")
set(PBC_SPI_HZ "62500000" CACHE STRING "ST7789 SPI clock request in Hz")
set(SHOW_FPS "0" CACHE STRING "Show fps and load under the picture: 0/1")

include(pico_sdk_import.cmake)

project(pbc_c64 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)   # Frodo4 uses operator<=>

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(FRODO ${CMAKE_CURRENT_LIST_DIR}/core)

add_executable(pbc_c64
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/c64_video.c
    src/c64_input.c
    src/c64_audio.c
    src/c64_roms.c
    src/audio_pwm.c
    src/input.c
    src/menu.c
    src/settings.c
    src/d64_dir.c
    src/flash_fs.c
    src/usb_msc.c
    src/usb_descriptors.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c

    # frank-c64 glue (core/rp2350), adapted where noted in the files
    ${FRODO}/rp2350/C64_rp2350.cpp
    ${FRODO}/rp2350/Display_rp2350.cpp
    ${FRODO}/rp2350/Prefs_rp2350.cpp
    ${FRODO}/rp2350/ROM_data.cpp
    ${FRODO}/rp2350/SID_rp2350.cpp
    ${FRODO}/rp2350/Tape_stub.cpp
    ${FRODO}/rp2350/fatfs_stdio.c

    # Frodo4 core, line-based (the same set frank-c64 builds)
    ${FRODO}/CPUC64.cpp
    ${FRODO}/VIC.cpp
    ${FRODO}/CIA.cpp
    ${FRODO}/IEC.cpp
    ${FRODO}/Cartridge.cpp
    ${FRODO}/REU.cpp
    ${FRODO}/1541d64.cpp
    ${FRODO}/1541gcr.cpp
    ${FRODO}/CPU1541.cpp
    ${FRODO}/VIA.cpp
)

target_include_directories(pbc_c64 PRIVATE
    src                 # tusb_config.h
    fatfs
    ${CMAKE_CURRENT_LIST_DIR}   # "fatfs/ff.h" as frank-c64 includes it
    ${FRODO}
    ${FRODO}/rp2350
)

target_compile_definitions(pbc_c64 PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
    PICO_STACK_SIZE=0x1000
    PICO_USE_STACK_GUARDS=1
    # SDK 2.2 would move clk_peri to 48 MHz on every clock change and cap
    # the display SPI at 24 MHz (see megadrive/nes).
    PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1
    CPU_MHZ=${CPU_MHZ}
    CPU_VREG=${CPU_VREG}
    FLASH_MAX_FREQ_MHZ=${FLASH_MAX_FREQ_MHZ}
    PBC_SPI_HZ=${PBC_SPI_HZ}u
    SHOW_FPS=${SHOW_FPS}
    # frank-c64 configuration
    FRODO_RP2350=1
    ENABLE_DEBUG_LOGS=0
    ENABLE_PS2_KEYBOARD=0
)

# As in frank-c64.
target_compile_options(pbc_c64 PRIVATE
    -O3
    -fno-strict-aliasing
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
)

target_link_libraries(pbc_c64
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
pico_enable_stdio_usb(pbc_c64 0)
pico_enable_stdio_uart(pbc_c64 0)

# UF2 etc. -- or, for the combined firmware (pbcemu/build.sh), a .bin
# linked behind the launcher.
include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_c64)
