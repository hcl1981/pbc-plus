// PicoBoy Color Plus (PBC+) board configuration.
//
// Hardware:
//   - RP2350A microcontroller (30 GPIOs, ARM Cortex-M33)
//   - 16 MB external QSPI flash (W25Q128)
//   - ST7789 240x280 SPI display (SPI0)
//   - STK8BA58 I2C accelerometer (I2C0, INT on GP22)
//   - SK6805-EC14 RGB LED (PIO-driven, GP11)
//   - 5-way joystick + A/B buttons (active-low, GP0-4 / GP27-28)
//   - 3 status LEDs (red/yellow/green on GP14/13/12)
//   - PWM speaker on GP15
//   - VBAT/VBUS sense on GP29 (1:2 divider)
//
// Step 1: minimal build, default machine module only.
// Custom C user modules (display, pbc_hw) follow in step 2/3.

#define MICROPY_HW_BOARD_NAME                   "PicoBoy Color Plus"
#define MICROPY_HW_MCU_NAME                     "RP2350"

// Default SPI0 pins -- ST7789 display.
// MISO (GP16) is unused by the display but kept as the default for
// machine.SPI(0) so the constructor works without explicit pin args.
#define MICROPY_HW_SPI0_SCK                     (18)
#define MICROPY_HW_SPI0_MOSI                    (19)
#define MICROPY_HW_SPI0_MISO                    (16)

// Default I2C0 pins -- STK8BA58 accelerometer.
#define MICROPY_HW_I2C0_SDA                     (20)
#define MICROPY_HW_I2C0_SCL                     (21)

// USB: CDC for REPL only. MSC stays disabled. The default rp2 port
// configuration already does this; no overrides needed here.
//
// (If/when WebREPL or other features are added later, they live in
// frozen Python modules, not in this header.)
