# PBCemu -- device and game selection in front of all emulators
#
#   ./build.sh            builds every emulator and this launcher and packs
#                         them into ../dist/pbcemu.uf2 (see build.sh)
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s)
endif()
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2)
endif()

# Directory with the pbc_apps.h written by build.sh (emulator offsets).
set(PBC_APPS_DIR "" CACHE PATH "Directory holding the generated pbc_apps.h")
# Room the launcher has at the start of the flash; the emulators follow.
set(PBC_LAUNCHER_SIZE "0x40000" CACHE STRING "Flash reserved for the launcher")

include(pico_sdk_import.cmake)

project(pbcemu C CXX ASM)
set(CMAKE_C_STANDARD 11)

set(PICO_FLASH_SIZE_BYTES 16777216)
pico_sdk_init()

add_executable(pbcemu
    src/main.c
    src/ui.c
    src/gfx.c
    src/assets.c
    src/devices.c
    src/games.c
    src/prefs.c
    src/display.c
    src/input.c
    src/c64/c64_settings.c
    src/c64/d64_dir.c
    src/flash_fs.c
    src/usb_msc.c
    src/usb_descriptors.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c
)

# The launcher must stay inside its slot: the first emulator follows.
file(WRITE ${CMAKE_BINARY_DIR}/pico_flash_region.ld
    "FLASH(rx) : ORIGIN = 0x10000000, LENGTH = ${PBC_LAUNCHER_SIZE}\n")
set_property(TARGET pbcemu APPEND PROPERTY LINK_DEPENDS ${CMAKE_BINARY_DIR}/pico_flash_region.ld)

if(PBC_APPS_DIR)
    target_include_directories(pbcemu BEFORE PRIVATE ${PBC_APPS_DIR})
endif()
target_include_directories(pbcemu PRIVATE
    src
    fatfs
    ${CMAKE_CURRENT_LIST_DIR}/../common
)

target_compile_definitions(pbcemu PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216
    PICO_STACK_SIZE=0x1000
    PBGB_FS_FLASH_OFFSET=0x700000u  # same as the emulators in the combined firmware
)

target_compile_options(pbcemu PRIVATE -Wall -Wno-unused-function)
set_source_files_properties(src/gfx.c PROPERTIES COMPILE_FLAGS "-O3")

target_link_libraries(pbcemu
    pico_stdlib
    pico_unique_id
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_flash
    hardware_sync
    hardware_watchdog
    tinyusb_device
    tinyusb_board
)

# USB belongs to the mass-storage stick, and UART0's default pins are
# joystick CENTER/RIGHT -- no stdio at all.
pico_enable_stdio_usb(pbcemu 0)
pico_enable_stdio_uart(pbcemu 0)

pico_add_extra_outputs(pbcemu)
