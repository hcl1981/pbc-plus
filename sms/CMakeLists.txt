# PBC SMS/GG -- Sega Master System and Game Gear emulator for the PicoBoy Color Plus
#
# Emulator core SMS Plus as ported in pico-smsplus
# (https://github.com/PicoPlus-devel/pico-smsplus); platform layer from
# ../nes and ../c64, flash FatFs + USB stick from ../picoboygb. See README.md.
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
# 250 MHz at 1.25 V is what PicoBoyGB runs on this board. Above 1.30 V the
# RP2350 voltage limit is lifted in main.c.
set(CPU_MHZ "200" CACHE STRING "System clock in MHz")
set(CPU_VREG "VREG_VOLTAGE_1_15" CACHE STRING "Core voltage (VREG_VOLTAGE_1_10 .. VREG_VOLTAGE_1_60)")
set(FLASH_MAX_FREQ_MHZ "88" CACHE STRING "Upper limit for the flash (XIP) clock in MHz")
set(PBC_SPI_HZ "62500000" CACHE STRING "ST7789 SPI clock request in Hz")
set(SHOW_FPS "0" CACHE STRING "Show fps and load under the picture: 0/1")

include(pico_sdk_import.cmake)

project(pbc_sms C CXX ASM)
set(CMAKE_C_STANDARD 11)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(SMSP ${CMAKE_CURRENT_LIST_DIR}/core/smsplus)

add_executable(pbc_sms
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/sms_video.c
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

    # SMS Plus core from pico-smsplus
    ${SMSP}/loadrom.c
    ${SMSP}/render.c
    ${SMSP}/sms.c
    ${SMSP}/sn76496.c
    ${SMSP}/system.c
    ${SMSP}/tms.c
    ${SMSP}/vdp.c
    ${SMSP}/z80.c
    ${SMSP}/emu2413/emu2413.c
)

target_include_directories(pbc_sms PRIVATE
    src                 # tusb_config.h
    fatfs
    ${SMSP}
)

target_compile_definitions(pbc_sms PRIVATE
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
    # pico-smsplus configuration; the sound chips run at the speaker rate
    MB_SMS
    LSB_FIRST=0
    NDEBUG
    SMS_AUD_RATE=22222
)

# As in pico-smsplus.
target_compile_options(pbc_sms PRIVATE
    -O2
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-cast-function-type
    -Wno-maybe-uninitialized
)

target_link_libraries(pbc_sms
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
pico_enable_stdio_usb(pbc_sms 0)
pico_enable_stdio_uart(pbc_sms 0)

# UF2 etc. -- or, for the combined firmware (pbcemu/build.sh), a .bin
# linked behind the launcher.
include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_sms)
