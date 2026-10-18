# PBC NES -- NES emulator for the PicoBoy Color Plus
#
# Emulator core QuickNES as forked in frank-nes
# (https://github.com/rh1tech/frank-nes); platform layer from ../megadrive,
# flash FatFs + USB stick from ../picoboygb. See README.md.
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
# 250 MHz at 1.25 V is what PicoBoyGB runs on this board; the NES needs far
# less than the Genesis. Above 1.30 V the RP2350 voltage limit is lifted in
# main.c -- the board's small buck-boost converter may not like it.
set(CPU_MHZ "250" CACHE STRING "System clock in MHz")
set(CPU_VREG "VREG_VOLTAGE_1_25" CACHE STRING "Core voltage (VREG_VOLTAGE_1_10 .. VREG_VOLTAGE_1_60)")
set(FLASH_MAX_FREQ_MHZ "88" CACHE STRING "Upper limit for the flash (XIP) clock in MHz")
set(PBC_SPI_HZ "62500000" CACHE STRING "ST7789 SPI clock request in Hz")
set(SHOW_FPS "0" CACHE STRING "Show fps and load under the picture: 0/1")

include(pico_sdk_import.cmake)

project(pbc_nes C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(QN ${CMAKE_CURRENT_LIST_DIR}/core/quicknes)
set(OPLL ${CMAKE_CURRENT_LIST_DIR}/core/emu2413)

add_executable(pbc_nes
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

    # QuickNES core from frank-nes
    ${QN}/nes_emu.cpp
    ${QN}/nes_core.cpp
    ${QN}/nes_cpu.cpp
    ${QN}/nes_ppu.cpp
    ${QN}/nes_ppu_impl.cpp
    ${QN}/nes_ppu_rendering.cpp
    ${QN}/nes_cart.cpp
    ${QN}/nes_mapper.cpp
    ${QN}/nes_apu.cpp
    ${QN}/nes_oscs.cpp
    ${QN}/nes_file.cpp
    ${QN}/nes_state.cpp
    ${QN}/nes_buffer.cpp
    ${QN}/nes_effects_buffer.cpp
    ${QN}/blip_buffer.cpp
    ${QN}/multi_buffer.cpp
    ${QN}/effects_buffer.cpp
    ${QN}/data_reader.cpp
    ${QN}/abstract_file.cpp
    ${QN}/nes_data.cpp
    ${QN}/nes_util.cpp
    ${QN}/apu_state.cpp
    ${QN}/nes_fme7_apu.cpp
    ${QN}/nes_namco_apu.cpp
    ${QN}/nes_vrc6_apu.cpp
    ${QN}/nes_vrc7.cpp
    ${QN}/quicknes.cpp
    ${OPLL}/emu2413.cpp
    ${OPLL}/emu2413_state.cpp
)

target_include_directories(pbc_nes PRIVATE
    src                 # tusb_config.h
    fatfs
    ${QN}
    ${OPLL}
)

target_compile_definitions(pbc_nes PRIVATE
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
    # The SDK maps operator new to malloc, but malloc panics when the heap
    # is exhausted. QuickNES checks every allocation itself and reports
    # "Out of memory" -- e.g. for a CHR ROM whose tile cache does not fit --
    # which only works if malloc can fail quietly.
    PICO_MALLOC_PANIC=0
    # QuickNES configuration as in frank-nes
    NDEBUG
    NO_UNALIGNED_ACCESS
)

# As in frank-nes: no exceptions/RTTI; -mno-unaligned-access because
# QuickNES casts byte pointers to uint32_t* for the tile cache, and LDRD
# faults on unaligned addresses on the Cortex-M33.
target_compile_options(pbc_nes PRIVATE
    -O2
    -mno-unaligned-access
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -Wno-sign-compare
    -Wno-multichar      # QuickNES tags its save-state blocks with 'FOUR' constants
)

target_link_libraries(pbc_nes
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
pico_enable_stdio_usb(pbc_nes 0)
pico_enable_stdio_uart(pbc_nes 0)

pico_add_extra_outputs(pbc_nes)
