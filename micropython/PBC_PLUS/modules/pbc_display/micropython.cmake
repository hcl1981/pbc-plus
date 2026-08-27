# pbc_display: ST7789 + DMA C user module for the PicoBoy Color Plus.
#
# Pulled in from boards/PBC_PLUS/mpconfigboard.cmake. Sources compile
# into a single INTERFACE library that gets linked to MicroPython's
# `usermod` aggregator target.

add_library(usermod_pbc_display INTERFACE)

target_sources(usermod_pbc_display INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/display.c
    ${CMAKE_CURRENT_LIST_DIR}/drawing.c
    ${CMAKE_CURRENT_LIST_DIR}/st7789.c
    ${CMAKE_CURRENT_LIST_DIR}/png_decode.c
    ${CMAKE_CURRENT_LIST_DIR}/idat_stream.c
)

target_include_directories(usermod_pbc_display INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod_pbc_display INTERFACE
    hardware_spi
    hardware_dma
    hardware_pwm
)

target_link_libraries(usermod INTERFACE usermod_pbc_display)
