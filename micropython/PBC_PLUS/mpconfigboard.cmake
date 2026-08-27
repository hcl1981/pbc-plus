# cmake configuration for the PicoBoy Color Plus (PBC+) board.
#
# Targets the RP2350A in ARM (Cortex-M33) secure mode. All chip-
# variant defaults (RP2350A, 30 GPIOs, boot2, etc.) are inherited
# from the stock Pico 2 board header via pbc_plus.h, so we only need
# to point the build at our custom header directory and pick the
# platform.

set(PICO_PLATFORM "rp2350-arm-s")

# Custom pico-sdk board header in this directory (pbc_plus.h).
set(PICO_BOARD "pbc_plus")
list(APPEND PICO_BOARD_HEADER_DIRS ${MICROPY_PORT_DIR}/boards/PBC_PLUS)

# Frozen module manifest (currently inherits the rp2 port defaults
# only; pbc.py / turtle.py are added in step 4).
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)

# Register C user module(s) via the standard USER_C_MODULES list.
# The build system iterates this list AFTER the `usermod` target is
# defined, which avoids the "target usermod does not exist" error
# you get if you try to include() the module's cmake file directly
# from here.
list(APPEND USER_C_MODULES
    ${CMAKE_CURRENT_LIST_DIR}/modules/pbc_display/micropython.cmake
    ${CMAKE_CURRENT_LIST_DIR}/modules/pbc_hw/micropython.cmake
)
