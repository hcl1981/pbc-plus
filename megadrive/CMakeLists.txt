# PBC Mega Drive -- Sega Genesis / Mega Drive emulator for the PicoBoy Color Plus
#
# Emulator core from frank-genesis (https://github.com/rh1tech/frank-genesis),
# flash FatFs + USB stick from PicoBoyGB (pbc-plus). See README.md.
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
# System clock and core voltage. 250 MHz at 1.25 V is what PicoBoyGB runs;
# above 1.30 V the RP2350 voltage limit has to be lifted (done in main.c).
# The board's buck-boost converter is small, so higher settings may brown
# out on AAA cells -- try upwards carefully.
set(CPU_MHZ "200" CACHE STRING "System clock in MHz (e.g. 250, 300, 336, 378)")
set(CPU_VREG "VREG_VOLTAGE_1_15" CACHE STRING "Core voltage (VREG_VOLTAGE_1_25 .. VREG_VOLTAGE_1_60)")
set(FLASH_MAX_FREQ_MHZ "100" CACHE STRING "Upper limit for the flash (XIP) clock in MHz")
set(PBC_SPI_HZ "75000000" CACHE STRING "ST7789 SPI clock request in Hz")
set(SHOW_FPS "0" CACHE STRING "Show fps and speed under the picture: 0/1")
set(AUDIO_VOLUME "8" CACHE STRING "Volume of the stand-alone firmware: 0 = off, 1..12")
set(Z80_SLICE_LINES "16" CACHE STRING "Run the Z80 every N scanlines")
set(TEST_SONIC2_ZONE "-1" CACHE STRING "Test build: start Sonic 2 in this zone/act (0x0F00 = Aquatic Ruin 1, 0x1000 = special stage), -1 = off")

include(pico_sdk_import.cmake)

project(pbc_megadrive C CXX ASM)
set(CMAKE_C_STANDARD 11)

# The pico2 board file says 4 MB; the PicoBoy Color Plus has 16 MB and the
# ROM region and FatFs partition depend on it.
set(PICO_FLASH_SIZE_BYTES 16777216)

pico_sdk_init()

set(CORE ${CMAKE_CURRENT_LIST_DIR}/core)

add_executable(pbc_megadrive
    # PicoBoy Color Plus platform
    src/main.c
    src/display.c
    src/video.c
    src/input.c
    src/menu.c
    src/rom_store.c
    src/ssf2.c
    src/audio_pwm.c
    src/ym_core1.c
    src/ym2612_synth.c
    src/flash_fs.c
    src/usb_msc.c
    src/usb_descriptors.c
    fatfs/ff.c
    fatfs/ffunicode.c
    fatfs/diskio.c

    # Gwenesis core from frank-genesis
    ${CORE}/bus/gwenesis_bus.c
    ${CORE}/io/gwenesis_io.c
    ${CORE}/vdp/gwenesis_vdp_gfx.c
    ${CORE}/vdp/gwenesis_vdp_mem.c
    ${CORE}/vdp/gwenesis_vdp_gfx_opt.S
    ${CORE}/vdp/gwenesis_vdp_sprite_opt.S
    ${CORE}/vdp/gwenesis_vdp_line_opt.S
    ${CORE}/savestate/gwenesis_savestate.c
    ${CORE}/sound/gwenesis_sn76489.c
    ${CORE}/sound/ym2612.c
    ${CORE}/sound/z80inst.c
    ${CORE}/sound/z80_benchmark.c
    # Z80: redcode/Z80 (LGPL-3.0-or-later), see core/cpus/redcode/README.md
    ${CORE}/cpus/redcode/Z80.c
    # 68000: Musashi 4.10 (MIT), see core/cpus/musashi/readme.txt
    ${CORE}/cpus/musashi/m68kcpu.c
    ${CORE}/cpus/musashi/m68kops.c
)

set_source_files_properties(${CORE}/cpus/musashi/m68kcpu.c ${CORE}/cpus/musashi/m68kops.c PROPERTIES
    COMPILE_FLAGS "-O3 -ffast-math -funroll-loops -finline-functions -fno-partial-inlining")
set_source_files_properties(src/video.c PROPERTIES COMPILE_FLAGS "-O3")
# redcode/Z80 with the PicoBoy switches (tools/redcode_patch.py): opcodes
# and operands straight from Z80 RAM (code only ever runs from there, as in
# the old core), data through z80inst.c's memory map, the opcodes through a
# switch, code and tables in RAM; in the combined firmware, which has the
# RAM for it, the IX/IY opcodes through a switch as well (sound drivers use
# them a lot). Its z80_run and z80_execute are renamed: z80inst.c has
# functions of these names.
set(REDCODE_DEFS "Z80_WITH_LOCAL_HEADER;Z80_PBC_FETCH8K=Z80_RAM;Z80_PBC_READ=z80_read;Z80_PBC_WRITE=z80_write;Z80_PBC_SWITCH;Z80_PBC_RAM=\".time_critical.redcode\";Z80_PBC_RAM_TABLES;z80_run=redcode_z80_run;z80_execute=redcode_z80_execute")
if(DEFINED PBC_APP_OFFSET)
    list(APPEND REDCODE_DEFS Z80_PBC_SWITCH_XY)
endif()
set_source_files_properties(${CORE}/cpus/redcode/Z80.c PROPERTIES
    COMPILE_DEFINITIONS "${REDCODE_DEFS}"
    COMPILE_FLAGS "-O2")

target_include_directories(pbc_megadrive PRIVATE
    src                 # HDMI.h stand-in, tusb_config.h
    fatfs
    ${CORE}
    ${CORE}/bus
    ${CORE}/io
    ${CORE}/vdp
    ${CORE}/sound
    ${CORE}/savestate
    ${CORE}/cpus/musashi
    ${CORE}/cpus/redcode
)

target_compile_definitions(pbc_megadrive PRIVATE
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
    AUDIO_VOLUME=${AUDIO_VOLUME}
    Z80_SLICE_LINES=${Z80_SLICE_LINES}
    TEST_SONIC2_ZONE=${TEST_SONIC2_ZONE}
    Z80_STATIC
    PBC_YM_CORE1=1
    # Gwenesis / frank-genesis configuration
    PICO_ON_DEVICE=1
    ROM_SWAP=0
    BUS_DISABLE_LOGGING=1
    VDP_GFX_DISABLE_LOGGING=1
    ENABLE_LOGGING=0
    LSB_FIRST=1
)

target_link_libraries(pbc_megadrive
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
    hardware_xip_cache
    tinyusb_device
    tinyusb_board
)

# USB belongs to the mass-storage stick, and UART0's default pins are
# joystick CENTER/RIGHT -- no stdio at all.
pico_enable_stdio_usb(pbc_megadrive 0)
pico_enable_stdio_uart(pbc_megadrive 0)

# UF2 etc. -- or, for the combined firmware (pbcemu/build.sh), a .bin
# linked behind the launcher.
include(${CMAKE_CURRENT_LIST_DIR}/../common/pbc_multi.cmake)
pbc_multi(pbc_megadrive)
