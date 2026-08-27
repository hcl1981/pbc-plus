cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(micropolis C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Target the RP2350 (Pico 2 family). Adjust if your PBC+ board file differs.
if(NOT PICO_BOARD)
    set(PICO_BOARD pico2)
endif()

pico_sdk_init()

# ---------------------------------------------------------------------------
# Engine: the real vendored Micropolis simulation core + original tile art.
# (Compiles + runs headless; verified on host. On the embedded target the core
#  links against newlib; the city save lives in flash via save.c.)
# ---------------------------------------------------------------------------
file(GLOB MICROPOLIS_CORE
    vendor/micropolis/sim/s_*.c
    vendor/micropolis/sim/rand.c
    vendor/micropolis/sim/random.c)
# tool layer (w_tool.c = tools/footprints/costs, w_con.c = auto-connect)
list(APPEND MICROPOLIS_CORE
    vendor/micropolis/sim/w_tool.c
    vendor/micropolis/sim/w_con.c)
set(ENGINE_SOURCES
    src/engine_micropolis.c
    src/micropolis_glue.c
    ${MICROPOLIS_CORE})
set(TILESET_SOURCES src/tiles_micropolis.c)   # real SimCity art (flash)

add_executable(micropolis
    src/main.c
    src/st7789.c
    src/input.c
    src/save.c
    src/render.c
    src/ui.c
    src/tool_icons.c
    src/core_sync.c
    src/led.c

    ${ENGINE_SOURCES}

    ${TILESET_SOURCES}
)

target_include_directories(micropolis PRIVATE src)
target_include_directories(micropolis PRIVATE vendor/micropolis/sim/headers)
target_compile_definitions(micropolis PRIVATE HEADLESS=1)

# compile the WS2812/SK6805 PIO program -> ws2812.pio.h (on the target's path)
pico_generate_pio_header(micropolis ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

# the vendored core + glue + adapter are early-1990s C
set_source_files_properties(
    ${MICROPOLIS_CORE} src/micropolis_glue.c src/engine_micropolis.c
    PROPERTIES COMPILE_FLAGS "-std=gnu89 -w -fcommon")

target_link_libraries(micropolis
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_dma
    hardware_pwm
    hardware_flash
    hardware_pio
)

# USB serial for debugging (UART off by default)
pico_enable_stdio_usb(micropolis 1)
pico_enable_stdio_uart(micropolis 0)

pico_add_extra_outputs(micropolis)   # .uf2 / .elf / .bin
