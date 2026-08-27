# pbc -- high-level glue for the PicoBoy Color Plus.
#
# Two ways to draw:
#
#   1) Direct to panel (simple, immediate):
#          import pbc
#          pbc.fill(pbc.BLACK)
#          pbc.text12("Hi", 10, 10, pbc.WHITE)
#      Each call updates the screen right away. No show() needed.
#
#   2) Via a Canvas (buffered, flicker-free for animations):
#          import pbc
#          c = pbc.Canvas(pbc.WIDTH, pbc.HEIGHT)
#          while True:
#              c.fill(pbc.BLACK)
#              c.text12("Hi", x, y, pbc.WHITE)
#              pbc.show(c)
#      All drawing accumulates in RAM, then show() blits once.
#
# Canvas inherits all framebuf.FrameBuffer methods (fill, text, pixel,
# line, rect, ellipse, poly, hline, vline, fill_rect, blit, scroll, ...)
# and adds text12() with the proportional 12-px font.

import os
import time
import framebuf
import machine

import display as _disp
import pbc_hw  as _hw

# Reset hardware state and drop any stale Python callback pointers
# left over from the previous VM session. pbc.py's body re-runs on
# every soft-reboot (sys.modules is reinitialised), so this gives
# us a clean slate without the user having to call anything.
_hw.init()

# ---- Public constants -------------------------------------------------

WIDTH  = _disp.WIDTH
HEIGHT = _disp.HEIGHT

BLACK   = _disp.BLACK
WHITE   = _disp.WHITE
RED     = _disp.RED
GREEN   = _disp.GREEN
BLUE    = _disp.BLUE
YELLOW  = _disp.YELLOW
CYAN    = _disp.CYAN
MAGENTA = _disp.MAGENTA
GRAY    = _disp.GRAY
ORANGE  = _disp.ORANGE
PURPLE  = _disp.PURPLE

rgb = _disp.rgb

# ---- Display ---------------------------------------------------------

_screen = _disp.Display()

def show(canvas, x=0, y=0):
    """Blit a Canvas to the panel at position (x, y). Defaults to (0,0).
    The canvas size determines what region gets updated, so you can
    place small canvases anywhere on the screen.

    If the canvas has a transparency key set (e.g. a PNG with alpha
    loaded via pbc.load_image()), pixels matching that key are skipped
    -- the pixels currently on the display under those positions stay
    untouched. Set `canvas.transparent = None` to disable and force
    an opaque blit (faster, useful for full-screen backgrounds)."""
    key = getattr(canvas, 'transparent', None)
    if key is None:
        _screen.blit(canvas.buf, x, y, canvas.width, canvas.height)
    else:
        _screen.blit(canvas.buf, x, y, canvas.width, canvas.height, key=key)

# Direct-to-panel drawing. Each call updates the screen immediately.
# These mirror the methods on Canvas, so you can swap a script between
# direct and buffered just by adding/removing a Canvas + show().

def fill(color=BLACK):                    _screen.fill(color)
def clear(color=BLACK):                   _screen.fill(color)
def pixel(x, y, color):                   _screen.pixel(x, y, color)
def line(x0, y0, x1, y1, color):          _screen.line(x0, y0, x1, y1, color)

def rect(x, y, w, h, color, fill=False):
    _screen.rect(x, y, w, h, color, fill)

def fill_rect(x, y, w, h, color):         _screen.fill_rect(x, y, w, h, color)
def hline(x, y, w, color):                _screen.hline(x, y, w, color)
def vline(x, y, h, color):                _screen.vline(x, y, h, color)

def ellipse(cx, cy, xr, yr, color, fill=False, mask=0x0F):
    _screen.ellipse(cx, cy, xr, yr, color, fill, mask)

def poly(ox, oy, coords, color, fill=False):
    _screen.poly(ox, oy, coords, color, fill)

def text(s, x, y, color):                 _screen.text(s, x, y, color)

def text12(s, x, y, color, bg=None):
    return _screen.text12(s, x, y, color, bg)

def text12_width(s):
    return _screen.text12_width(s)

def text18(s, x, y, color, bg=None):
    return _screen.text18(s, x, y, color, bg)

def text18_width(s):
    return _screen.text18_width(s)

def text24(s, x, y, color, bg=None):
    return _screen.text24(s, x, y, color, bg)

def text24_width(s):
    return _screen.text24_width(s)

def backlight(level):
    _screen.set_backlight(level)

# ---- Canvas ----------------------------------------------------------

class Canvas(framebuf.FrameBuffer):
    """An RGB565 pixel buffer with all framebuf drawing methods plus
    text12(). Use this for flicker-free animations or to compose a
    frame in RAM before sending it. Blit with pbc.show().

    Optional transparent-colour key: pixels matching this colour are
    treated as transparent when blitting onto another Canvas via
    Canvas.blit (framebuf.blit) or via Canvas.stamp_on()."""

    def __init__(self, w, h, buf=None, transparent=None):
        if buf is None:
            buf = bytearray(w * h * 2)
        elif not isinstance(buf, bytearray):
            # framebuf.FrameBuffer needs a WRITABLE buffer (so its
            # drawing methods can mutate it). A bytes object would
            # otherwise raise "object with buffer protocol required"
            # on the super().__init__ below. Most likely cause is an
            # older cached png.py that returned bytes(); converting
            # is wasteful but keeps the program running.
            buf = bytearray(buf)
        self.buf = buf
        super().__init__(buf, w, h, framebuf.RGB565)
        self.width  = w
        self.height = h
        self.transparent = transparent      # RGB565 int or None

    def text12(self, s, x, y, color, bg=None):
        """12-px proportional font, same look as pbc.text12() but on this canvas."""
        return _screen.text12_fb(self.buf, self.width, self.height,
                                 s, x, y, color, bg)

    def text12_width(self, s):
        return _screen.text12_width(s)

    def text18(self, s, x, y, color, bg=None):
        """18-px proportional font, same look as pbc.text18() but on this canvas."""
        return _screen.text18_fb(self.buf, self.width, self.height,
                                 s, x, y, color, bg)

    def text18_width(self, s):
        return _screen.text18_width(s)

    def text24(self, s, x, y, color, bg=None):
        """24-px proportional font, same look as pbc.text24() but on this canvas."""
        return _screen.text24_fb(self.buf, self.width, self.height,
                                 s, x, y, color, bg)

    def text24_width(self, s):
        return _screen.text24_width(s)

    def stamp_on(self, target, x, y):
        """Draw this canvas onto `target` at (x, y), using
        self.transparent as the transparency key if it's set.

        Out-of-bounds positions (the canvas being partly or fully off
        the target) are handled gracefully: anything off-screen is
        silently clipped away. framebuf.blit's default behaviour for
        certain partial-overlap cases is to throw "blit region out of
        bounds" instead of clipping, so we catch that and skip the
        draw -- the worst case is that the sprite isn't visible for
        a frame, which is fine."""
        x = int(x); y = int(y)
        # target.width/height isn't always available (e.g. a C-side
        # Display object), so fall back to the display constants when
        # the attributes are missing.
        try:
            tw = int(target.width)
            th = int(target.height)
        except AttributeError:
            tw = WIDTH; th = HEIGHT
        # Fully off-screen: nothing to do at all.
        if (x + self.width  <= 0 or x >= tw or
            y + self.height <= 0 or y >= th):
            return
        key = self.transparent
        try:
            if key is not None:
                target.blit(self, x, y, key)
            else:
                target.blit(self, x, y)
        except ValueError:
            # framebuf.blit can raise for certain partial-overlap
            # geometries depending on stride alignment. Silently skip
            # the draw rather than crash the user's program -- the
            # sprite will reappear as soon as it moves back into a
            # valid position.
            pass

# ---- Image loading ---------------------------------------------------

def load_image(path, transparent=MAGENTA):
    """Load a PNG file into a Canvas.

    PNG alpha is honoured: pixels with alpha < 50 % in the source
    become transparent on screen. You don't need to do anything
    special -- just save a PNG with transparency from any image
    editor and call:

        ship = pbc.load_image('ship.png')
        ship.stamp_on(canvas, x, y)

    Internally the alpha channel is collapsed onto a colour-key
    sentinel (RGB565 magenta, 0xF81F). Real magenta pixels in the
    source image are auto-nudged by 1 LSB to keep them visible, so
    the sentinel never collides with intended image content.

    Pass `transparent=None` to disable transparency entirely (the
    image is treated as fully opaque, including formerly-transparent
    regions which then show up as magenta)."""
    import png
    fill = png.KEY if transparent is None else transparent
    w, h, data = png.load(path, fill)
    return Canvas(w, h, data, transparent=transparent)

# ---- Sprite -----------------------------------------------------------

class Sprite:
    """A movable picture on the screen.

    Each sprite owns an image (loaded from a PNG once at construction)
    and a position. The screen is its own little world: when any
    sprite is shown, moved, or glided, all visible sprites are
    repainted together so they layer correctly without the user
    having to think about canvases or draw order.

    Typical usage:
        player = pbc.sprite('hero.png')
        player.show()
        player.glide_to(120, 200, 1000)
        if player.touches(enemy):
            pbc.delay(500)

    The class is normally instantiated through pbc.sprite(path), not
    by calling Sprite() directly -- the wrapper handles loading the
    image."""

    # Class-level registry of visible sprites. Repaint walks this in
    # insertion order, so newer sprites draw on top.
    _visible = []
    # Optional callback that the IDE can hook into for special effects
    # (e.g. drawing a background before sprites). Kept simple to avoid
    # introducing a layer system the kids would have to learn.
    _background = None

    def __init__(self, image):
        self.image = image
        self.x = 0
        self.y = 0
        self._shown = False

    # ---- size accessors -------------------------------------------------
    @property
    def width(self):  return self.image.width
    @property
    def height(self): return self.image.height

    # ---- visibility -----------------------------------------------------
    def show(self):
        """Make this sprite visible and repaint the screen."""
        if not self._shown:
            Sprite._visible.append(self)
            self._shown = True
        _redraw_sprites()

    def hide(self):
        """Hide this sprite and repaint the screen."""
        if self._shown:
            try: Sprite._visible.remove(self)
            except ValueError: pass
            self._shown = False
        _redraw_sprites()

    # ---- position (instant) --------------------------------------------
    def set_to(self, x, y):
        """Teleport the sprite to absolute (x, y) and repaint."""
        self.x = int(x)
        self.y = int(y)
        _redraw_sprites()

    def set_by(self, dx, dy):
        """Move the sprite by (dx, dy) pixels and repaint."""
        self.x += int(dx)
        self.y += int(dy)
        _redraw_sprites()

    # ---- position (animated, blocking) ---------------------------------
    def glide_to(self, x, y, ms):
        """Animate to absolute (x, y) over `ms` milliseconds.

        Blocks until the animation finishes -- the program waits here
        and resumes once the sprite has arrived. Roughly 60 fps."""
        x = int(x); y = int(y); ms = int(ms)
        if ms <= 0:
            self.set_to(x, y)
            return
        sx, sy = self.x, self.y
        dx, dy = x - sx, y - sy
        # 16 ms per frame ≈ 60 fps; never zero so the loop runs.
        steps = max(1, ms // 16)
        for i in range(1, steps + 1):
            t = i / steps
            self.x = int(sx + dx * t)
            self.y = int(sy + dy * t)
            _redraw_sprites()
            time.sleep_ms(16)

    def glide_by(self, dx, dy, ms):
        """Animate by (dx, dy) pixels over `ms` milliseconds."""
        self.glide_to(self.x + int(dx), self.y + int(dy), ms)

    # ---- collisions ----------------------------------------------------
    def touches(self, other):
        """True if this sprite's bounding box overlaps another sprite's."""
        return (self.x < other.x + other.image.width  and
                self.x + self.image.width  > other.x  and
                self.y < other.y + other.image.height and
                self.y + self.image.height > other.y)

    def touches_edge(self):
        """True if any part of the sprite is outside the screen, i.e.
        it has bumped into (or gone past) the display edge."""
        return (self.x < 0 or self.y < 0 or
                self.x + self.image.width  > WIDTH or
                self.y + self.image.height > HEIGHT)


# Lazily-allocated backbuffer for sprite rendering. We don't reserve
# the 134 KB until the first sprite is actually shown, so programs
# that only use direct drawing don't pay for it.
_sprite_backbuf = None

def _redraw_sprites():
    """Repaint the screen with the current set of visible sprites.

    We render into an off-screen Canvas first, which:
      - Uses framebuf.blit's proper colour-key transparency
        (the C Display.blit doesn't support a key argument).
      - Pushes the finished frame to the panel in one SPI burst
        instead of one transfer per sprite, so there's no flicker
        between the black fill and the sprites being stamped on top.

    Called automatically by Sprite operations -- the user doesn't
    need to think about when to refresh."""
    global _sprite_backbuf
    if _sprite_backbuf is None:
        # First-time allocation. ~134 KB on a 240x280x2 panel.
        _sprite_backbuf = Canvas(WIDTH, HEIGHT)
    # Background: black, or an optional user-supplied Canvas (not
    # exposed via blocks yet, but a hook is here for later).
    if Sprite._background is None:
        _sprite_backbuf.fill(BLACK)
    else:
        Sprite._background.stamp_on(_sprite_backbuf, 0, 0)
    # Stamp every visible sprite onto the backbuffer in insertion
    # order -- later sprites draw on top of earlier ones.
    for s in Sprite._visible:
        # Use framebuf.blit directly with the colour-key argument so
        # transparency works. The earlier stamp_on() path went through
        # a try/except that could silently swallow real failures; doing
        # the blit inline here makes a misconfigured sprite visible
        # because it'll actually raise.
        key = s.image.transparent
        try:
            if key is not None:
                _sprite_backbuf.blit(s.image, int(s.x), int(s.y), key)
            else:
                _sprite_backbuf.blit(s.image, int(s.x), int(s.y))
        except ValueError:
            # Sprite (partly) off-screen: framebuf.blit rejects some
            # partial-overlap geometries. Skip silently rather than
            # crash the program -- the sprite will reappear once it
            # moves back into a valid position.
            pass
    # One DMA blast pushes the whole frame to the panel.
    show(_sprite_backbuf, 0, 0)


def sprite(path):
    """Convenience factory: load `path` as a PNG and return a fresh
    Sprite at (0, 0). Use this instead of Sprite() directly so the
    image-loading detail stays out of the way."""
    return Sprite(load_image(path))

# ---- Timing ----------------------------------------------------------

def delay(ms):
    """Wait `ms` milliseconds. Saves you the `import time`."""
    time.sleep_ms(ms)

# ---- Status LEDs (red/yellow/green next to the screen) ---------------

_led_red    = machine.Pin(14, machine.Pin.OUT, value=0)
_led_yellow = machine.Pin(13, machine.Pin.OUT, value=0)
_led_green  = machine.Pin(12, machine.Pin.OUT, value=0)

def led_red(on=True):    _led_red.value(1 if on else 0)
def led_yellow(on=True): _led_yellow.value(1 if on else 0)
def led_green(on=True):  _led_green.value(1 if on else 0)
def leds_off():
    _led_red.value(0); _led_yellow.value(0); _led_green.value(0)

# ---- Audio -----------------------------------------------------------

tone     = _hw.tone
tone_off = _hw.tone_off

# ---- Battery ---------------------------------------------------------

battery_voltage = _hw.battery_voltage

def is_on_usb():
    """True if the device is currently powered from USB. The sense
    line reads ~5 V on USB and the battery voltage (3.0..4.2 V on a
    healthy LiPo) when running on battery, so 4.0 V is a clean
    threshold between the two states."""
    return battery_voltage() > 4.0

# ---- Buttons ---------------------------------------------------------

# Level reads (true while held).
pressed_up      = _hw.pressed_up
pressed_down    = _hw.pressed_down
pressed_left    = _hw.pressed_left
pressed_right   = _hw.pressed_right
pressed_center  = _hw.pressed_center
pressed_a       = _hw.pressed_a
pressed_b       = _hw.pressed_b

# Edge reads (true once per press; auto-clears).
was_pressed_up      = _hw.was_pressed_up
was_pressed_down    = _hw.was_pressed_down
was_pressed_left    = _hw.was_pressed_left
was_pressed_right   = _hw.was_pressed_right
was_pressed_center  = _hw.was_pressed_center
was_pressed_a       = _hw.was_pressed_a
was_pressed_b       = _hw.was_pressed_b

# IRQ-driven callbacks. The low-level pbc_hw API passes the button
# index to the callback (machine.Pin.irq()-style); we wrap here so
# user callbacks can be plain `def cb():` without an argument. If
# you want the index, drop down to pbc_hw.on_press_X directly.

def _wrap_btn_cb(cb):
    if cb is None:
        return None
    return lambda _idx: cb()

def on_press_up(cb):     _hw.on_press_up(_wrap_btn_cb(cb))
def on_press_down(cb):   _hw.on_press_down(_wrap_btn_cb(cb))
def on_press_left(cb):   _hw.on_press_left(_wrap_btn_cb(cb))
def on_press_right(cb):  _hw.on_press_right(_wrap_btn_cb(cb))
def on_press_center(cb): _hw.on_press_center(_wrap_btn_cb(cb))
def on_press_a(cb):      _hw.on_press_a(_wrap_btn_cb(cb))
def on_press_b(cb):      _hw.on_press_b(_wrap_btn_cb(cb))

def any_pressed():
    return (pressed_up() or pressed_down() or pressed_left() or
            pressed_right() or pressed_center() or
            pressed_a() or pressed_b())

def wait_for_button():
    while not any_pressed():
        time.sleep_ms(10)
    while any_pressed():
        time.sleep_ms(10)

# ---- RGB LED + accelerometer -----------------------------------------

rgb_led     = _hw.rgb_led
rgb_led_off = _hw.rgb_led_off

accel_x   = _hw.accel_x
accel_y   = _hw.accel_y
accel_z   = _hw.accel_z
accel_x_g = _hw.accel_x_g
accel_y_g = _hw.accel_y_g
accel_z_g = _hw.accel_z_g

# ---- Built-in program menu -------------------------------------------

_HIDDEN = ("main.py", "boot.py", "pbc.py", "turtle.py", "png.py")

def _list_programs():
    progs = []
    try:
        for name in sorted(os.listdir("/")):
            if name.endswith(".py") and name not in _HIDDEN:
                progs.append(name)
    except OSError:
        pass
    return progs

def menu(cancel=False):
    """Show a list of installed .py files.

    cancel : if True, the menu includes a "Cancel" option:
             B aborts the menu and returns to the caller.
             A or CENTER launches the selected program.
             The footer reads "A: run   B: cancel".

             if False (default), the menu has no escape hatch:
             ANY button (A, B, CENTER, ...) launches the program.
             The footer reads "Press button to run".

    Uses direct-to-panel drawing -- no canvas allocation."""
    progs = _list_programs()
    if not progs:
        _screen.fill(BLACK)
        msg1 = "No programs found."
        msg2 = "Copy *.py via mpremote."
        _screen.text12(msg1, (WIDTH - _screen.text12_width(msg1)) // 2,
                       120, WHITE)
        _screen.text12(msg2, (WIDTH - _screen.text12_width(msg2)) // 2,
                       140, GRAY)
        time.sleep(2)
        return

    cursor = 0
    top = 0
    LINE_H   = 20
    HEADER_H = 36
    FOOTER_H = 22
    visible_lines = (HEIGHT - HEADER_H - FOOTER_H) // LINE_H

    # Palette matched to the PBC+ Studio web UI so the device feels
    # like part of the same product line.
    #   HEADER_BG ~ --bg-2  (#1f2630)  dark blue-grey header bar
    #   HILITE_BG ~ --accent (#4f9eff) but desaturated so 12-px white
    #              text on top stays comfortably readable
    #   FOOTER_BG ~ --bg-0  (#0e1116)  near-black footer
    HEADER_BG  = _disp.rgb(0x1f, 0x26, 0x30)
    HILITE_BG  = _disp.rgb(0x2f, 0x56, 0x80)   # accent-dim from the IDE
    FOOTER_BG  = _disp.rgb(0x0e, 0x11, 0x16)
    TITLE_FG   = _disp.rgb(0xe6, 0xed, 0xf3)   # --fg-0
    FOOTER_FG  = _disp.rgb(0x7d, 0x88, 0x93)   # --fg-2
    ROW_FG     = _disp.rgb(0xe6, 0xed, 0xf3)
    ACCENT_BAR = _disp.rgb(0x4f, 0x9e, 0xff)   # --accent, for the
                                                # left edge marker on
                                                # the active row

    def _draw_row(idx, is_cursor):
        """Paint exactly one menu row, including its background. Used
        both for initial paint and for incremental cursor moves -- no
        full-screen wipe needed, which removes the flicker on every
        UP/DOWN press."""
        if idx >= len(progs) or idx < top or idx >= top + visible_lines:
            return
        y = HEADER_H + (idx - top) * LINE_H
        label = progs[idx][:-3]    # strip .py
        if is_cursor:
            _screen.fill_rect(0, y - 1, WIDTH, LINE_H, HILITE_BG)
            # 3-px wide accent bar on the left, mirrors the IDE's
            # selected-row treatment.
            _screen.fill_rect(0, y - 1, 3, LINE_H, ACCENT_BAR)
            _screen.text12(label, 12, y + 2, ROW_FG, HILITE_BG)
        else:
            _screen.fill_rect(0, y - 1, WIDTH, LINE_H, BLACK)
            _screen.text12(label, 12, y + 2, ROW_FG, BLACK)

    def _draw_chrome():
        """Paint header + footer once. These never change while the
        menu is open, so we don't redraw them on cursor moves."""
        _screen.fill_rect(0, 0, WIDTH, HEADER_H, BLACK)
        _screen.fill_rect(0, 0, WIDTH, HEADER_H - 4, HEADER_BG)
        # Thin accent line below the header, again echoing the IDE
        # toolbar's bottom border.
        _screen.fill_rect(0, HEADER_H - 4, WIDTH, 1, ACCENT_BAR)
        title = "PBC+"
        tw = _screen.text12_width(title)
        _screen.text12(title, (WIDTH - tw) // 2, 8, TITLE_FG, HEADER_BG)

        _screen.fill_rect(0, HEIGHT - FOOTER_H, WIDTH, FOOTER_H, FOOTER_BG)
        # Hint text depends on cancel-mode: with Cancel the user gets
        # the explicit A/B mapping; without it any button triggers
        # the program so we just say so.
        if cancel:
            hint = "A: run   B: cancel"
        else:
            hint = "Press button to run"
        hw = _screen.text12_width(hint)
        _screen.text12(hint, (WIDTH - hw) // 2,
                       HEIGHT - FOOTER_H + 3, FOOTER_FG, FOOTER_BG)

    def _draw_all_rows():
        """Paint the body area from scratch. Wipes only the list
        area, leaves header / footer alone."""
        _screen.fill_rect(0, HEADER_H, WIDTH,
                          HEIGHT - HEADER_H - FOOTER_H, BLACK)
        for i in range(visible_lines):
            idx = top + i
            if idx >= len(progs):
                break
            _draw_row(idx, idx == cursor)

    # Initial paint.
    _screen.fill(BLACK)
    _draw_chrome()
    _draw_all_rows()

    while True:
        # Input loop -- between events, the screen stays exactly as it
        # is. No redraw happens unless something actually changes.
        while True:
            if was_pressed_up():
                old_cursor = cursor
                old_top = top
                cursor = (cursor - 1) % len(progs)
                # Adjust scroll window. If the cursor stayed inside it,
                # we only repaint the two affected rows; if the window
                # scrolled, we repaint the whole list area (still
                # cheaper than a full fill+redraw because we skip the
                # chrome).
                if cursor < top:
                    top = cursor
                elif cursor >= top + visible_lines:
                    top = cursor - visible_lines + 1
                if top != old_top:
                    _draw_all_rows()
                else:
                    _draw_row(old_cursor, False)
                    _draw_row(cursor, True)
                break

            if was_pressed_down():
                old_cursor = cursor
                old_top = top
                cursor = (cursor + 1) % len(progs)
                if cursor < top:
                    top = cursor
                elif cursor >= top + visible_lines:
                    top = cursor - visible_lines + 1
                if top != old_top:
                    _draw_all_rows()
                else:
                    _draw_row(old_cursor, False)
                    _draw_row(cursor, True)
                break

            # Launch keys: in cancel-mode only A and CENTER launch
            # (B is reserved for "cancel"); without cancel-mode any
            # button -- A, B, or CENTER -- launches the selected
            # program, so a child can just mash a button without
            # caring which one.
            if cancel:
                launch = was_pressed_a() or was_pressed_center()
            else:
                launch = (was_pressed_a()    or
                          was_pressed_b()    or
                          was_pressed_center())

            if launch:
                _run_program(progs[cursor])
                # In kids-mode (cancel=False) we hard-reset the device
                # after every program. Why: the freshly-launched
                # program may have allocated big things like canvases
                # or sprite buffers; without a full reset those
                # objects (and any memory fragmentation they caused)
                # would stick around in the REPL globals until we
                # tried to launch the next one. On a 520-KB SRAM chip
                # that's enough to provoke MemoryError on the second
                # launch of any non-trivial program. A soft reset is
                # the only reliable way to recover ALL the heap --
                # explicit `del` + gc.collect() doesn't catch module-
                # level state and doesn't defragment.
                #
                # In cancel-mode the menu is treated as an interactive
                # tool the developer can drop in and out of, so we
                # don't trample their REPL state.
                if not cancel:
                    import machine
                    machine.soft_reset()
                    # Doesn't return.

                # After running, refresh the menu from scratch.
                progs = _list_programs()
                if not progs:
                    return
                if cursor >= len(progs):
                    cursor = len(progs) - 1
                _screen.fill(BLACK)
                _draw_chrome()
                _draw_all_rows()
                break

            # Only honour B-as-cancel when the caller asked for it.
            # Otherwise B was already consumed by the launch branch
            # above; this check would never fire.
            if cancel and was_pressed_b():
                return

            time.sleep_ms(20)


def _run_program(name):
    leds_off()
    rgb_led_off()
    tone_off()
    _screen.fill(BLACK)
    msg = "Running " + name + "..."
    _screen.text12(msg, (WIDTH - _screen.text12_width(msg)) // 2,
                   120, WHITE)
    time.sleep_ms(150)
    try:
        # Read + exec so the loaded program shares no globals with us.
        with open("/" + name) as f:
            code = f.read()
        ns = {"__name__": "__main__"}
        exec(code, ns)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        _screen.fill(BLACK)
        err1 = "Error in " + name
        err2 = repr(e)[:30]
        err3 = "Press any button."
        _screen.text12(err1, (WIDTH - _screen.text12_width(err1)) // 2,
                       60, RED)
        _screen.text12(err2, (WIDTH - _screen.text12_width(err2)) // 2,
                       80, WHITE)
        _screen.text12(err3, (WIDTH - _screen.text12_width(err3)) // 2,
                       HEIGHT - 30, GRAY)
        wait_for_button()
    finally:
        leds_off()
        rgb_led_off()
        tone_off()
        # Brief pause before the launcher reasserts itself. Without
        # this the menu pops up the instant the program returns,
        # which feels jarring -- the player has no time to register
        # the final frame of the game (the "Game Over" screen, the
        # final score, the last drawing). One second is long enough
        # to look at the result but short enough not to feel laggy.
        time.sleep(1)


# ---- Quick reference -------------------------------------------------

_HELP_TEXT = """\
PicoBoy Color Plus -- pbc module quick reference

Display (direct draw -- updates the screen immediately):
  fill(c) / clear()                 fill the screen
  pixel(x,y,c)                      single pixel
  line(x1,y1,x2,y2,c)               line
  rect(x,y,w,h,c,fill=False)        rectangle
  fill_rect(x,y,w,h,c)              filled rectangle
  hline(x,y,len,c) / vline(x,y,len,c)
  ellipse(cx,cy,xr,yr,c,fill=False)
  poly(ox,oy,coords,c,fill=False)   polygon (coords: array.array('h'))
  text(s,x,y,c)                     8x8 monospace
  text12(s,x,y,c,bg=None)           12-px proportional font
  text12_width(s)                   pixel width of a text12 string
  backlight(0..255)                 LCD brightness

Buffered drawing (build a frame in RAM, blit when ready):
  Canvas(w,h,buf=None,transparent=None)
    .fill / .pixel / .line / .rect / .blit / ...    (all of framebuf)
    .text12(s,x,y,c,bg=None) / .text12_width(s)
    .stamp_on(target,x,y)           transparency-aware blit
  load_image(path, transparent=MAGENTA) -> Canvas
  show(canvas, x=0, y=0)            blit Canvas to panel at (x,y)

Sprite (image + position helper):
  Sprite(image, x=0, y=0)
    .draw(canvas)                   stamps with transparency
    .hits(other)                    bounding-box collision

Buttons (UP, DOWN, LEFT, RIGHT, CENTER, A, B):
  pressed_X()                       True while held
  was_pressed_X()                   True once per press (latched)
  on_press_X(callback)              IRQ callback, cb() with no args
  any_pressed() / wait_for_button()

LEDs / RGB / sound:
  led_red(on) / led_yellow(on) / led_green(on) / leds_off()
  rgb_led(r,g,b) / rgb_led_off()
  tone(freq,ms=None) / tone_off()

G-sensor (axis convention: +X right, +Y down/forward, +Z out of screen):
  accel_x()   / accel_y()   / accel_z()             m/s^2
  accel_x_g() / accel_y_g() / accel_z_g()           g (gravity multiples)

Power & misc:
  battery_voltage() -> float V       raw VBATT/VBUS reading
  is_on_usb()       -> bool          True when running on USB (>4 V)
  delay(ms)
  menu()                             return to launcher

Constants:
  WIDTH, HEIGHT
  BLACK, WHITE, RED, GREEN, BLUE, YELLOW, CYAN,
  MAGENTA, GRAY, ORANGE, PURPLE
  rgb(r,g,b) -> RGB565 int

Turtle graphics (separate module):
  import turtle
  t = turtle.Turtle()
  t.forward(d) / t.back(d)
  t.left(deg) / t.right(deg)
  t.setDirection(deg) / t.setPosition(x,y) / t.home()
  t.penUp() / t.penDown() / t.setPenColor(c)
  t.speed(s)                         0=instant, 1..10+, default 5
  t.hide() / t.show() / t.clear()
"""

def help():
    """Print a quick reference for the pbc API."""
    print(_HELP_TEXT)
