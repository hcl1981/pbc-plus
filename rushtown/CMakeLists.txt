# Rushtown -- a town seen from above, for the PicoBoy Color Plus
#
#   ./build.sh        builds ../dist/rushtown.uf2
cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD    pico2)

include(pico_sdk_import.cmake)
project(rushtown C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(rushtown
    src/game.c
    src/draw.c
    src/world.c
    src/art.c
    src/gfx.c
    src/snd.c
    src/music.c
    src/speech_data.c
    src/pics.S
    src/pbc/main.c
    src/pbc/display.c
    src/pbc/input.c
    src/pbc/audio.c
    src/pbc/store.c
)
target_include_directories(rushtown PRIVATE src)

# What can be left out again: -DRT_STORY=0 and so on (see src/rt.h).
foreach(part ISLANDS STORY MUSIC SPEECH ART)
    set(RT_${part} 1 CACHE STRING "Rushtown: build with this part (1) or without (0)")
    target_compile_definitions(rushtown PRIVATE RT_${part}=${RT_${part}})
endforeach()
target_compile_definitions(rushtown PRIVATE
    PICO_FLASH_SIZE_BYTES=16777216      # the pico2 board file says 4 MB
    PICO_STACK_SIZE=0x1000            # the stack bank is 4 KB
    PICO_USE_STACK_GUARDS=1
)
target_compile_options(rushtown PRIVATE -Wall -Wextra -Wno-unused-parameter -O2 -fno-strict-aliasing)
# drawing is where the time goes
set_source_files_properties(src/gfx.c src/draw.c PROPERTIES COMPILE_FLAGS "-O3")
target_link_libraries(rushtown
    pico_stdlib hardware_spi hardware_dma hardware_pwm hardware_gpio
    hardware_flash hardware_sync hardware_clocks
)
# no serial console: UART0's default pins are the joystick's CENTER and RIGHT
pico_enable_stdio_uart(rushtown 0)
pico_enable_stdio_usb(rushtown 0)
# panic and failed assertions end on the panel (src/pbc/main.c)
target_link_options(rushtown PRIVATE -Wl,--wrap=panic -Wl,--wrap=hard_assertion_failure)
pico_add_extra_outputs(rushtown)
