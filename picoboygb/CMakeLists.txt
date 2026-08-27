# ===========================================================================
# PicoBoyGB (SDK build) - Game Boy DMG emulator for the PicoBoy Color Plus (RP2350)
# ---------------------------------------------------------------------------
# Plain Raspberry Pi Pico SDK build of the emulator that originally shipped as
# an Arduino sketch (../PicoBoyGB/PicoBoyGB.ino). The Arduino FatFS/FatFSUSB
# stack is replaced by vendored ChaN FatFs (./fatfs) on a flash region plus a
# TinyUSB mass-storage device (src/usb_*.c) so .gb ROMs can still be dropped
# onto the device over USB.
#
# Configure with build_picoboygb.sh, or manually:
#   cmake -S . -B build -DPICO_SDK_PATH=... -DPICO_BOARD=pico2 \
#         -DPICO_PLATFORM=rp2350-arm-s
#   cmake --build build -j
# ===========================================================================
cmake_minimum_required(VERSION 3.13)

# RP2350A in secure ARM mode, Pico 2 board profile (unless overridden).
if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s)
endif()
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2)
endif()

include(pico_sdk_import.cmake)

project(picoboygb C CXX ASM)

# The board profile defaults to 4 MB; the PicoBoy Color Plus has 16 MB and our ROM
# region / FatFs partition / save region rely on that (see src/flash_fs.h).
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

add_executable(picoboygb
    src/main.cpp
    src/minigb_apu.cpp
    src/flash_fs.c
    src/usb_descriptors.c
    src/usb_msc.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c
)

target_include_directories(picoboygb PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src        # tusb_config.h, headers
    ${CMAKE_CURRENT_LIST_DIR}/fatfs      # ff.h, ffconf.h, diskio.h
)

# 16 MB flash so ROM (0x100000), FatFs (0x300000) and save (0xFF0000) fit.
target_compile_definitions(picoboygb PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
)

target_link_libraries(picoboygb
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

# We use USB for mass storage, not for stdio - keep both stdio backends off so
# nothing else grabs the USB controller.
pico_enable_stdio_usb(picoboygb 0)
pico_enable_stdio_uart(picoboygb 0)

# .uf2 (+ .bin/.hex/.dis/.elf.map). On RP2350 this also runs picotool to sign
# the image so the bootrom accepts it.
pico_add_extra_outputs(picoboygb)
