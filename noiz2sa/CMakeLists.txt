cmake_minimum_required(VERSION 3.13)

# --- Pflichtangaben fuer den PicoBoy Color Plus (CLAUDE.md Abschnitt 2) ---
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD    pico2)

include(pico_sdk_import.cmake)
project(noiz2sa_pbc C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(PBC_SPI_HZ "" CACHE STRING "SPI-Takt zum Panel in Hz (leer = 62500000)")

function(pbc_target tgt)
    target_include_directories(${tgt} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
    if(PBC_SPI_HZ)
        target_compile_definitions(${tgt} PRIVATE PBC_SPI_HZ=${PBC_SPI_HZ}u)
    endif()
    target_compile_definitions(${tgt} PRIVATE
        PICO_FLASH_SIZE_BYTES=16777216
        PICO_STACK_SIZE=0x1000
        PICO_USE_STACK_GUARDS=1
    )
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_link_libraries(${tgt}
        pico_stdlib hardware_spi hardware_dma hardware_pwm hardware_gpio
        hardware_flash hardware_sync hardware_clocks
    )
    # Keine serielle Konsole: UART0 laege auf GP0/GP1 = Joystick CENTER/RIGHT.
    pico_enable_stdio_uart(${tgt} 0)
    pico_enable_stdio_usb(${tgt} 0)
    target_link_options(${tgt} PRIVATE
        -Wl,--wrap=panic -Wl,--wrap=hard_assertion_failure)
    pico_add_extra_outputs(${tgt})
endfunction()

set(NOIZ_CORE
    src/game.c
    src/render.c
    src/bml.c
    src/bml_data.c
    src/segfont.c
    src/pbc/display.c
    src/pbc/input.c
    src/pbc/audio.c
    src/pbc/store.c
    src/pbc/leds.c
    src/pbc/fatal.c
)

add_executable(noiz2sa src/main.c ${NOIZ_CORE})
pbc_target(noiz2sa)

add_executable(noiz2sa_selftest selftest/selftest.c ${NOIZ_CORE})
pbc_target(noiz2sa_selftest)
