# PBC 2600 -- Atari 2600 emulator for the PicoBoy Color Plus, part of the
# combined firmware only (pbcemu/build.sh builds it with -DPBC_APP_OFFSET=...).
#
# Emulator core core/, written for this device (6507, TIA, RIOT, bank
# switching). Platform layer from ../msx. See README.md.
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

project(pbc_a2600 C CXX ASM)
set(CMAKE_C_STANDARD 11)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(CORE ${CMAKE_CURRENT_LIST_DIR}/core)

add_executable(pbc_a2600
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/a26_video.c
    src/audio_pwm.c
    src/input.c
    src/menu.c
    src/rom_store.c
    src/settings.c
    src/flash_fs.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c

    # Atari 2600 core
    ${CORE}/a2600.c
    ${CORE}/tia.c
)

target_include_directories(pbc_a2600 PRIVATE
    src
    fatfs
    ${CORE}
)

target_compile_definitions(pbc_a2600 PRIVATE
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

target_compile_options(pbc_a2600 PRIVATE
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
    src/menu.c src/rom_store.c src/settings.c src/flash_fs.c src/main.c
    fatfs/ff.c fatfs/ffunicode.c fatfs/diskio.c
    PROPERTIES COMPILE_OPTIONS "-Os")

target_link_libraries(pbc_a2600
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
pico_enable_stdio_usb(pbc_a2600 0)
pico_enable_stdio_uart(pbc_a2600 0)

include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_a2600)
