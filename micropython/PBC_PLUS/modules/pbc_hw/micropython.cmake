# pbc_hw — PicoBoy Color Plus hardware peripherals.
#   tone (PWM), battery (ADC), buttons (GPIO + IRQ),
#   RGB LED (SK6805 via PIO), STK8BA58 accelerometer (I2C).
#
# Picked up via USER_C_MODULES from boards/PBC_PLUS/mpconfigboard.cmake.

add_library(usermod_pbc_hw INTERFACE)

target_sources(usermod_pbc_hw INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/pbc_hw.c
    ${CMAKE_CURRENT_LIST_DIR}/tone.c
    ${CMAKE_CURRENT_LIST_DIR}/battery.c
    ${CMAKE_CURRENT_LIST_DIR}/buttons.c
    ${CMAKE_CURRENT_LIST_DIR}/rgb_led.c
    ${CMAKE_CURRENT_LIST_DIR}/accel.c
)

target_include_directories(usermod_pbc_hw INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod_pbc_hw INTERFACE
    hardware_pwm
    hardware_adc
    hardware_gpio
    hardware_pio
    hardware_i2c
    hardware_clocks
    pico_time
)

target_link_libraries(usermod INTERFACE usermod_pbc_hw)
