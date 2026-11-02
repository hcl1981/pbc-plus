# pbc_multi.cmake -- build an emulator as part of the combined firmware
#
# Called by the emulators' CMakeLists.txt in place of pico_add_extra_outputs().
# Without PBC_APP_OFFSET nothing changes: the emulator is a firmware of its own.
# pbcemu/build.sh passes
#   -DPBC_APP_OFFSET=0x...  flash offset the emulator is linked to
#   -DPBC_APP_SIZE=0x...    room it has there
# The emulator then takes its game from the launcher (common/pbc_launch.h),
# the ROM region moves to 0x200000 (5 MB) behind the packed firmwares and
# the USB drive (FatFs) starts at 0x700000 instead of 0x600000.
# The record of which game is in it moves too (0xFF8000): the standalone
# firmwares' record at 0xFF0000 describes their region at 0x100000.

function(pbc_multi target)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_FUNCTION_LIST_DIR})
    if(NOT DEFINED PBC_APP_OFFSET)
        pico_add_extra_outputs(${target})
        return()
    endif()
    # picotool refuses to make a UF2 of an image that does not start at the
    # beginning of the flash; pbcemu/build.sh packs the .bin itself.
    pico_add_bin_output(${target})
    pico_add_dis_output(${target})
    # The SDK writes this file in pico_sdk_init(); the linker script includes
    # it from the build directory.
    file(WRITE ${CMAKE_BINARY_DIR}/pico_flash_region.ld
        "FLASH(rx) : ORIGIN = 0x10000000 + ${PBC_APP_OFFSET}, LENGTH = ${PBC_APP_SIZE}\n")
    # The linker script only INCLUDEs it, so CMake does not know the link
    # depends on it: without this a new offset would leave the old .elf.
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${CMAKE_BINARY_DIR}/pico_flash_region.ld)
    target_compile_definitions(${target} PRIVATE
        PBC_MULTI=1
        ROM_FLASH_OFFSET=0x200000u
        ROM_FLASH_SIZE=0x500000u
        ROM_INFO_OFFSET=0xFF8000u
        PBGB_FS_FLASH_OFFSET=0x700000u
    )
    # which emulator this is (pbcemu/src/devices.h), for the loading screen
    set(app_of_pbc_megadrive APP_MD)
    set(app_of_pbc_sms APP_SMS)
    set(app_of_pbc_nes APP_NES)
    set(app_of_pbc_pce APP_PCE)
    set(app_of_pbc_gbc APP_GBC)
    set(app_of_pbc_c64 APP_C64)
    set(app_of_pbc_msx APP_MSX)
    set(app_of_pbc_a2600 APP_A26)
    if(NOT DEFINED app_of_${target})
        message(FATAL_ERROR "pbc_multi: no APP_* for ${target}")
    endif()
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pbc_loading.c
        PROPERTIES COMPILE_DEFINITIONS PBC_APP=${app_of_${target}})
    # Loading screen and messages in the launcher's look (pbc_loading.c),
    # drawn with its gfx.c and fonts; the linker keeps only what is used.
    set(launcher ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../pbcemu/src)
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pbc_loading.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pbc_link.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pbc_lockstep.c
        ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/usblink.c
        ${launcher}/gfx.c
        ${launcher}/assets.c)
    set_source_files_properties(${CMAKE_CURRENT_FUNCTION_LIST_DIR}/pbc_loading.c
        PROPERTIES INCLUDE_DIRECTORIES ${launcher})
    set_source_files_properties(${launcher}/gfx.c
        PROPERTIES COMPILE_DEFINITIONS GFX_STRIP COMPILE_OPTIONS -O3)
endfunction()
