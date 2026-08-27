# PicoBoy Color Plus -- MicroPython board variant

This is the `PBC_PLUS` board variant for the MicroPython rp2 port,
targeting the RP2350A with 16 MB external flash.

It provides a REPL over USB-CDC, the standard `machine` module, and two
C modules on top: `pbc_display` (ST7789 240x280, drawing primitives, three
font sizes, PNG loading) and `pbc_hw` (buttons with callbacks, accelerometer,
battery voltage, RGB LED, tone output). The Python layer `pbc.py` wraps both
and adds `Canvas`, `Sprite`, `load_image()`, a program menu and `help()`.

## Files in this directory

| File | Purpose |
| --- | --- |
| `mpconfigboard.h` | MicroPython board config (name, default SPI/I2C pins) |
| `mpconfigboard.cmake` | CMake glue: platform = `rp2350-arm-s`, custom board header dir, manifest |
| `pbc_plus.h` | pico-sdk board header: 16 MB flash, crystal config |
| `manifest.py` | Frozen-module manifest (defaults only for now) |
| `board.json` | Metadata for the MicroPython build system |
| `pins.csv` | Pin assignment reference (informational) |
| `README.md` | This file |

## Building (Linux)

> If you are working inside the PBC+ repository, `../build.sh` does all of
> this for you — it fetches MicroPython v1.28.0, mirrors this directory into
> the rp2 boards folder and builds. The steps below are the manual equivalent,
> useful when dropping this board into an existing MicroPython checkout.


Tested with Ubuntu 24.04. Requires `arm-none-eabi-gcc` >= 13,
`cmake` >= 3.20, `python3`, and `build-essential`.

```bash
sudo apt install gcc-arm-none-eabi cmake build-essential python3
```

Then:

```bash
git clone https://github.com/micropython/micropython.git
cd micropython
git checkout v1.28.0
git submodule update --init lib/pico-sdk lib/tinyusb
( cd lib/pico-sdk && git submodule update --init )

# Drop this whole directory into the rp2 boards folder:
cp -r /path/to/PBC_PLUS ports/rp2/boards/

# Cross-compiler for frozen .py modules:
make -C mpy-cross

# Build the firmware:
cd ports/rp2
make BOARD=PBC_PLUS submodules
make BOARD=PBC_PLUS
```

The resulting UF2 lives at:

```
ports/rp2/build-PBC_PLUS/firmware.uf2
```

## Flashing

Hold BOOTSEL on the PBC+ while plugging in USB. An `RP2350` mass-
storage volume mounts on the host; drag-drop `firmware.uf2` onto it.
The device reboots automatically.

## Smoke test

After flashing, the PBC+ enumerates as a USB-CDC serial device. Open
it with Thonny, `mpremote`, or any terminal program (baud rate is
irrelevant for CDC). You should land in the MicroPython REPL.

Quick sanity check from the REPL:

```python
import sys
print(sys.implementation)
# (name='micropython', version=(1, 28, 0), ...)

from machine import Pin
red = Pin(14, Pin.OUT)   # status LED, red
red.on()                  # should light up
red.off()
```

If the LED toggles, step 1 is good and we can move on to the display
C module.

## Roadmap

1. [x] Board variant + minimal build (this).
2. [ ] C user module `display`: ST7789 driver, PIO-DMA byte-swap,
       `framebuf`-compatible API for both direct and buffered drawing,
       12 px sans-serif font, PNG decoder, backlight PWM.
3. [ ] C user module `pbc_hw`: SK6805 PIO driver, STK8BA58 over I2C,
       battery ADC, tone PWM, status LEDs, button state tracking.
4. [ ] Frozen Python modules: rewritten `pbc.py` (English API,
       `pressed_a()` / `was_pressed_a()` / `on_press_a()`, separate
       per-axis accelerometer accessors) and `turtle.py` (bug-fixed).
5. [ ] Filesystem image with default `main.py` that calls the menu.

## License

MIT, matching MicroPython itself.
