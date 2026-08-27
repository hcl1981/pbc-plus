add_library(common_pico INTERFACE)
target_sources(common_pico INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/i_glob.c
        # PicoBoy: USB-keyboard pipeline replaced with GPIO polling
        ${CMAKE_CURRENT_LIST_DIR}/i_input_picoboy.c
        ${CMAKE_CURRENT_LIST_DIR}/i_system.c
        ${CMAKE_CURRENT_LIST_DIR}/i_timer.c
        # i_video.c stays -- patched in-place with #ifdef PICOBOY_BUILD blocks
        ${CMAKE_CURRENT_LIST_DIR}/i_video.c
        # PicoBoy ST7789 driver (called from i_video.c hooks)
        ${CMAKE_CURRENT_LIST_DIR}/picoboy_display.c
        # PicoBoy LED-based diagnostic markers
        ${CMAKE_CURRENT_LIST_DIR}/picoboy_diag.c
        ${CMAKE_CURRENT_LIST_DIR}/stubs.c
        # PicoBoy: I2S DAC pipeline replaced with PWM audio on GP15
        ${CMAKE_CURRENT_LIST_DIR}/i_picosound_picoboy.c
)

# Multiplayer-Transport.
#   PICOBOY_NET_USB=ON  (default): Punkt-zu-Punkt ueber D+/D- des USB-C-Ports
#                                  (usblink.c + piconet_usb.c)
#   PICOBOY_NET_USB=OFF          : Original-I2C-Transport von rp2040-doom
#                                  (piconet.c, braucht SDA/SCL an GPIOs)
option(PICOBOY_NET_USB "Multiplayer ueber den USB-C-Port statt ueber I2C" ON)
if (PICOBOY_NET_USB)
    target_sources(common_pico INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/piconet_usb.c
            ${CMAKE_CURRENT_LIST_DIR}/usblink.c
    )
    target_compile_definitions(common_pico INTERFACE PICOBOY_NET_USB=1)
    # Diagnosezeile im Netzwerk-Wartebildschirm (zum Einmessen der Verbindung).
    # Mit -DPICOBOY_NET_DEBUG_SCREEN=OFF abschalten.
    option(PICOBOY_NET_DEBUG_SCREEN "Link-Diagnose im Streifen unter dem Bild anzeigen" OFF)
    if (PICOBOY_NET_DEBUG_SCREEN)
        target_compile_definitions(common_pico INTERFACE PICONET_DEBUG_SCREEN=1)
    endif()
else()
    target_sources(common_pico INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/piconet.c
    )
endif()
if (PICO_ON_DEVICE)
    target_sources(common_pico INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/blit.S
            ${CMAKE_CURRENT_LIST_DIR}/picoflash.c
            )
    pico_wrap_function(common_pico malloc)
    pico_wrap_function(common_pico calloc)
    pico_wrap_function(common_pico free)
    target_compile_definitions(common_pico INTERFACE
            PICO_HEAP_SIZE=0
            USE_ZONE_FOR_MALLOC=1
    )
    target_link_libraries(common_pico INTERFACE  hardware_i2c)
endif()
target_include_directories(common_pico INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/..
        ${CMAKE_CURRENT_LIST_DIR}/../../textscreen
        )

# Bilder auslassen, wenn die Spiellogik hinterherhinkt
option(PICOBOY_FRAME_SKIP "Bilder auslassen, wenn die Tic-Rate zurueckfaellt" ON)

target_compile_definitions(common_pico INTERFACE
        NO_USE_MOUSE=1
        # PicoBoy build: ST7789 SPI display + GPIO input + piezo (silent)
        # instead of VGA scanvideo + I2S DAC + USB-host keyboard.
        PICOBOY_BUILD=1
        # I2S DMA/PIO audio path is replaced with silent stubs at runtime,
        # but i_main.c emits a bi_decl(bi_3pins_with_names(...)) that
        # references PICO_AUDIO_I2S_DATA_PIN / PICO_AUDIO_I2S_CLOCK_PIN_BASE
        # as plain numeric constants -- they MUST be defined to compile,
        # even though the values are never used. Pick free solder-pad GPIOs.
        PICO_AUDIO_I2S_DATA_PIN=5
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=6
        # PICO_AUDIO_I2S_PIO=1     # actual I2S path stays disabled
        # PICO_AUDIO_I2S_DMA_IRQ=1
        PICO_USE_SW_SPIN_LOCKS=0
        PICO_USE_STACK_GUARDS=0
        # Bilder auslassen, wenn die Spiellogik hinterherhinkt
        # (mit -DPICOBOY_FRAME_SKIP=OFF abschaltbar)
        $<$<BOOL:${PICOBOY_FRAME_SKIP}>:PICOBOY_FRAME_SKIP=1>
)

# scanvideo headers are still pulled in by pd_render.cpp / i_video.c (the
# scanvideo PIO is just never started on the PicoBoy build).
pico_generate_pio_header(common_pico ${CMAKE_CURRENT_LIST_DIR}/video_doom.pio)
target_link_libraries(common_pico INTERFACE
        pico_stdlib
        pico_multicore
        pico_scanvideo_dpi
        # PicoBoy additions
        hardware_spi
        hardware_dma
        hardware_pwm
        hardware_clocks
)

# pico_cd: USB host removed for PicoBoy (no USB-A connector on the device).
add_library(pico_cd INTERFACE)

