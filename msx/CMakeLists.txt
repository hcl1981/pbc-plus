# PBC MSX -- MSX1 emulator for the PicoBoy Color Plus, part of the combined
# firmware only (pbcemu/build.sh builds it with -DPBC_APP_OFFSET=...).
#
# Emulator core core/msx1, written for this device; Z80 redcode/Z80
# (core/redcode, LGPL-3.0-or-later); C-BIOS 0.29a built in (core/cbios,
# BSD-style). Platform layer from ../sms and ../c64. See README.md.
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s)
endif()
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2)
endif()

# ---- tuning knobs ----------------------------------------------------------
set(CPU_MHZ "200" CACHE STRING "System clock in MHz")
set(CPU_VREG "VREG_VOLTAGE_1_15" CACHE STRING "Core voltage (VREG_VOLTAGE_1_10 .. VREG_VOLTAGE_1_60)")
set(FLASH_MAX_FREQ_MHZ "88" CACHE STRING "Upper limit for the flash (XIP) clock in MHz")
set(PBC_SPI_HZ "75000000" CACHE STRING "ST7789 SPI clock request in Hz")
set(SHOW_FPS "0" CACHE STRING "unused: the launcher switches the statistics on")

include(pico_sdk_import.cmake)

project(pbc_msx C CXX ASM)
set(CMAKE_C_STANDARD 11)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(MSX1 ${CMAKE_CURRENT_LIST_DIR}/core/msx1)
set(REDCODE ${CMAKE_CURRENT_LIST_DIR}/core/redcode)
set(CBIOS ${CMAKE_CURRENT_LIST_DIR}/core/cbios)

add_executable(pbc_msx
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/msx_video.c
    src/msx_input.c
    src/audio_pwm.c
    src/input.c
    src/menu.c
    src/rom_store.c
    src/settings.c
    src/flash_fs.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c

    # MSX1 core and Z80
    ${MSX1}/msx1.c
    ${MSX1}/tms9918.c
    ${MSX1}/ay8910.c
    ${MSX1}/scc.c
    ${REDCODE}/Z80.c

    # C-BIOS
    ${CBIOS}/cbios.S
)

# .incbin finds the ROM images next to cbios.S; rebuild when they change
set_source_files_properties(${CBIOS}/cbios.S PROPERTIES
    COMPILE_OPTIONS "-Wa,-I${CBIOS}"
    OBJECT_DEPENDS "${CBIOS}/cbios_main_msx1.rom;${CBIOS}/cbios_logo_msx1.rom")

# redcode/Z80 with the PicoBoy switches (sms/tools/redcode_patch.py): reads
# straight from the core's 8 KB page map, writes through its handler, code
# and tables in RAM. Without Z80_PBC_SWITCH: smaller, and an MSX1 needs
# less than the Master System.
set_source_files_properties(${REDCODE}/Z80.c ${MSX1}/msx1.c PROPERTIES
    COMPILE_DEFINITIONS "Z80_STATIC;Z80_WITH_LOCAL_HEADER;Z80_PBC_MAP;Z80_PBC_READMAP=msx_readmap;Z80_PBC_WRITE=msx_write;Z80_PBC_RAM=\".time_critical.redcode\";Z80_PBC_RAM_TABLES")

target_include_directories(pbc_msx PRIVATE
    src
    fatfs
    ${MSX1}
    ${REDCODE}
)

target_compile_definitions(pbc_msx PRIVATE
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
    # the core's busiest functions in RAM
    PBC_ON_DEVICE
    NDEBUG
)

target_compile_options(pbc_msx PRIVATE
    -O2
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-parentheses
    -Wno-sign-compare
    -Wno-misleading-indentation
)

# Size where speed does not matter: the firmware shares 2 MB with six other
# emulators. The Z80, the memory and I/O paths and the line drawing stay -O2.
set_source_files_properties(
    src/msx_input.c src/menu.c src/rom_store.c src/settings.c src/flash_fs.c src/main.c
    fatfs/ff.c fatfs/ffunicode.c fatfs/diskio.c
    PROPERTIES COMPILE_OPTIONS "-Os")

target_link_libraries(pbc_msx
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_flash
    hardware_sync
    hardware_clocks
    hardware_vreg
    hardware_watchdog
)

# UART0's default pins are joystick CENTER/RIGHT -- no stdio at all.
pico_enable_stdio_usb(pbc_msx 0)
pico_enable_stdio_uart(pbc_msx 0)

include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_msx)
