# PBC Arduboy -- the Arduboy (ATmega32U4, SSD1306 OLED) for the PicoBoy Color
# Plus, part of the combined firmware only (pbcemu/build.sh builds it with
# -DPBC_APP_OFFSET=...).
#
# Emulator core core/, written for this device. Platform layer from ../zx.
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

project(pbc_arduboy C CXX ASM)
set(CMAKE_C_STANDARD 11)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(CORE ${CMAKE_CURRENT_LIST_DIR}/core)

add_executable(pbc_arduboy
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/ard_video.c
    src/audio_pwm.c
    src/input.c
    src/menu.c
    src/settings.c
    src/flash_fs.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c

    # Arduboy core
    ${CORE}/arduboy.c
)

target_include_directories(pbc_arduboy PRIVATE
    src
    fatfs
    ${CORE}
)

target_compile_definitions(pbc_arduboy PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
    PICO_STACK_SIZE=0x1000
    PICO_MALLOC_PANIC=0                 # malloc may fail: main() looks for the largest block
    PICO_USE_STACK_GUARDS=1
    # SDK 2.2 would move clk_peri to 48 MHz on every clock change and cap
    # the display SPI at 24 MHz (see megadrive/nes).
    PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK=1
    CPU_MHZ=${CPU_MHZ}
    CPU_VREG=${CPU_VREG}
    FLASH_MAX_FREQ_MHZ=${FLASH_MAX_FREQ_MHZ}
    PBC_SPI_HZ=${PBC_SPI_HZ}u
    PBC_ON_DEVICE
    NDEBUG
)

# the AVR's busiest functions in RAM
set_source_files_properties(${CORE}/arduboy.c PROPERTIES
    COMPILE_DEFINITIONS "ARD_RAM=__attribute__((section(\".time_critical.ard\")));ARD_JIT=1"
    # each instruction's code with its own jump to the next: GCC would merge
    # those jumps into a few shared ones (one more branch per instruction)
    COMPILE_OPTIONS "-fno-gcse;-fno-crossjumping")

target_compile_options(pbc_arduboy PRIVATE
    -O2
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-parentheses
    -Wno-sign-compare
    -Wno-misleading-indentation
)

# Size where speed does not matter: the firmware shares 2 MB with the other
# emulators.
set_source_files_properties(
    src/menu.c src/settings.c src/flash_fs.c src/main.c
    fatfs/ff.c fatfs/ffunicode.c fatfs/diskio.c
    PROPERTIES COMPILE_OPTIONS "-Os")

target_link_libraries(pbc_arduboy
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
pico_enable_stdio_usb(pbc_arduboy 0)
pico_enable_stdio_uart(pbc_arduboy 0)

include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_arduboy)
