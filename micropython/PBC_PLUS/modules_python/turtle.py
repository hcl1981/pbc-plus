"""Classic turtle graphics for the PicoBoy Color Plus.

Coordinate convention: origin at screen centre, x grows right,
y grows up. setDirection(0) points up.

Usage:

    import pbc
    from turtle import Turtle

    t = Turtle()
    t.setPenColor("red")
    t.speed(5)
    for _ in range(36):
        t.forward(80)
        t.right(170)

The turtle owns a private 240x280 RGB565 canvas that's allocated
on first use; every move blits it to the panel. No need to call
pbc.show() yourself.

Speed semantics:
    speed(0)   -- instant, no animation
    speed(1)   -- slowest, 1 pixel per frame
    speed(10)  -- fast, 10 pixels per frame
    speed(N>10) -- still bigger chunks, snappier still
The default is speed(5).
"""

import math
import array
import time
import framebuf
import pbc

# ----------------------------------------------------------------------
# Colours
# ----------------------------------------------------------------------

_NAMED_COLORS = {
    "black":   pbc.BLACK,  "white":   pbc.WHITE,
    "red":     pbc.RED,    "green":   pbc.GREEN,
    "blue":    pbc.BLUE,   "yellow":  pbc.YELLOW,
    "cyan":    pbc.CYAN,   "magenta": pbc.MAGENTA,
    "orange":  pbc.ORANGE, "purple":  pbc.PURPLE,
    "gray":    pbc.GRAY,   "grey":    pbc.GRAY,
}


def _resolve_color(c, default=None):
    if isinstance(c, str):
        return _NAMED_COLORS.get(c, pbc.WHITE if default is None else default)
    return c


# ----------------------------------------------------------------------
# Shared canvas (lazy-allocated on first Turtle()).
# ----------------------------------------------------------------------

_CX = pbc.WIDTH  // 2
_CY = pbc.HEIGHT // 2

_canvas = None
_backup_buf = None
_backup_fb  = None
_BACKUP_SIZE = 28      # big enough for the asymmetric marker (tip at 11)


def _ensure_canvas():
    global _canvas
    if _canvas is None:
        _canvas = pbc.Canvas(pbc.WIDTH, pbc.HEIGHT)
        _canvas.fill(pbc.BLACK)
        pbc.show(_canvas)


def get_canvas():
    """Return the shared turtle canvas (allocate if needed)."""
    _ensure_canvas()
    return _canvas


def _ensure_backup():
    global _backup_buf, _backup_fb
    if _backup_buf is None:
        n = _BACKUP_SIZE
        _backup_buf = bytearray(n * n * 2)
        _backup_fb  = framebuf.FrameBuffer(_backup_buf, n, n, framebuf.RGB565)


# ----------------------------------------------------------------------
# Turtle
# ----------------------------------------------------------------------

class Turtle:
    def __init__(self, canvas=None):
        # canvas=None  -> standard behaviour: allocate a fresh black
        #                 canvas the first time any turtle is created;
        #                 subsequent turtles share that same canvas.
        # canvas=<Canvas> -> adopt the user's canvas as the shared
        #                    turtle backing store. The student has
        #                    presumably already drawn a background on
        #                    it -- we don't clear it, and we don't
        #                    push it to the panel here (the first
        #                    turtle move will trigger pbc.show()).
        global _canvas
        if canvas is not None:
            _canvas = canvas
        else:
            _ensure_canvas()
        self.color    = pbc.WHITE
        self.draw_pen = True
        self.visible  = True
        self._speed   = 5            # default: medium animation
        # Position in screen coords; (0,0) turtle = (_CX, _CY) screen.
        self._x = float(_CX)
        self._y = float(_CY)
        # Angle in screen-coord convention (y grows down):
        # 0=right, 90=down, 180=left, 270=up. setDirection(0)
        # translates user 0 -> screen 270 (up).
        self._angle = 270.0
        self._prev_pos   = None
        self._first_draw = True
        self._draw_turtle()
        pbc.show(_canvas)

    # ---- pen and visibility -----------------------------------------

    def setPenColor(self, color):
        """Set pen colour. Accepts a name ('red') or an RGB565 int."""
        self.color = _resolve_color(color, pbc.WHITE)

    def penUp(self):
        self.draw_pen = False

    def penDown(self):
        self.draw_pen = True

    def hide(self):
        self._restore_backup()
        self.visible = False
        self._first_draw = True
        pbc.show(_canvas)

    def show(self):
        # Erase any marker already on screen first, otherwise calling
        # show() when a cursor is already drawn (e.g. right after the
        # constructor, which draws one) leaves the old triangle behind
        # and stacks a second one on top -> a doubled cursor. Every
        # other redraw path (left/right/forward/...) restores first;
        # show() must too.
        was_visible = self.visible
        self.visible = True
        if was_visible:
            self._restore_backup()
        self._draw_turtle()
        pbc.show(_canvas)

    # ---- speed ------------------------------------------------------

    def speed(self, s):
        """Set animation speed. 0 = instant (no animation), 1 = very
        slow, 10 = fast. Bigger numbers chunk more pixels per blit."""
        if s < 0:
            s = 0
        self._speed = s

    # ---- direction --------------------------------------------------

    def left(self, angle):
        self._restore_backup()
        # Screen y grows down, so user "left" = subtract angle.
        self._angle = (self._angle - angle) % 360
        self._draw_turtle()
        pbc.show(_canvas)

    def right(self, angle):
        self._restore_backup()
        self._angle = (self._angle + angle) % 360
        self._draw_turtle()
        pbc.show(_canvas)

    def setDirection(self, angle):
        self._restore_backup()
        # User 0 = up; up in screen coords = 270.
        self._angle = (270 - angle) % 360
        self._draw_turtle()
        pbc.show(_canvas)

    # ---- position ---------------------------------------------------

    def setPosition(self, x, y):
        """Teleport to (x, y) in turtle coords."""
        self._restore_backup()
        self._x = _CX + x
        self._y = _CY - y
        self._first_draw = True
        self._draw_turtle()
        pbc.show(_canvas)

    def home(self):
        self.setPosition(0, 0)

    # ---- movement ---------------------------------------------------

    def forward(self, dist):
        """Walk forward `dist` pixels along current heading.

        With speed > 0 the motion is animated (you see the turtle
        glide pixel by pixel, in chunks proportional to speed).
        With speed = 0 the turtle teleports to the end position
        and only one frame is shown."""
        rad = math.radians(self._angle)
        dx, dy = math.cos(rad), math.sin(rad)
        sign  = 1 if dist >= 0 else -1
        adist = abs(dist)
        steps = int(adist)

        if self._speed == 0:
            # Instant: walk all pixels (drawing path), one show at end.
            self._restore_backup()
            for _ in range(steps):
                self._x += sign * dx
                self._y += sign * dy
                if self.draw_pen:
                    _canvas.pixel(int(self._x), int(self._y), self.color)
            rem = adist - steps
            if rem > 0:
                self._x += sign * dx * rem
                self._y += sign * dy * rem
            self._draw_turtle()
            pbc.show(_canvas)
            return

        # Animated: chunk pixels into blocks of `speed` size.
        block = max(1, int(self._speed))
        walked = 0
        while walked < steps:
            n = min(block, steps - walked)
            self._restore_backup()
            for _ in range(n):
                self._x += sign * dx
                self._y += sign * dy
                if self.draw_pen:
                    _canvas.pixel(int(self._x), int(self._y), self.color)
            self._draw_turtle()
            pbc.show(_canvas)
            walked += n

        # Catch up the fractional remainder so polygons close cleanly.
        rem = adist - steps
        if rem > 0:
            self._restore_backup()
            self._x += sign * dx * rem
            self._y += sign * dy * rem
            self._draw_turtle()
            pbc.show(_canvas)

    def back(self, dist):
        self.forward(-dist)

    def moveTo(self, x, y):
        """Walk to (x, y) in turtle coords, drawing if pen is down."""
        saved = self._angle
        tx = _CX + x
        ty = _CY - y
        dx = tx - self._x
        dy = ty - self._y
        self._angle = math.degrees(math.atan2(dy, dx)) % 360
        distance = math.sqrt(dx * dx + dy * dy)
        self.forward(distance)
        self._angle = saved
        self._draw_turtle()
        pbc.show(_canvas)

    # ---- canvas ----------------------------------------------------

    def clear(self, color=None):
        """Wipe the canvas. `color` may be a name, an RGB565 int, or
        None (defaults to black)."""
        c = pbc.BLACK if color is None else _resolve_color(color, pbc.BLACK)
        _canvas.fill(c)
        self._first_draw = True
        self._prev_pos = None
        self._draw_turtle()
        pbc.show(_canvas)

    # ---- internal cursor rendering --------------------------------

    def _draw_turtle(self):
        """Save the area under the cursor and draw the marker triangle."""
        if not self.visible:
            return
        _ensure_backup()
        n = _BACKUP_SIZE
        half = n // 2
        x = int(self._x); y = int(self._y)
        bx = max(0, min(x - half, pbc.WIDTH  - n))
        by = max(0, min(y - half, pbc.HEIGHT - n))
        # Save what's under the cursor area (so we can erase later).
        _backup_fb.blit(_canvas, -bx, -by)
        # Draw the marker on top.
        self._draw_triangle(x, y, self._angle)
        self._prev_pos   = (x, y)
        self._first_draw = False

    def _restore_backup(self):
        """Paint the saved area back over the cursor."""
        if self._first_draw or self._prev_pos is None or not self.visible:
            return
        if _backup_fb is None:
            return
        n = _BACKUP_SIZE
        half = n // 2
        px, py = self._prev_pos
        rx = max(0, min(px - half, pbc.WIDTH  - n))
        ry = max(0, min(py - half, pbc.HEIGHT - n))
        _canvas.blit(_backup_fb, rx, ry)

    def _draw_triangle(self, x, y, heading):
        """Filled, asymmetric arrow marker so the heading is obvious."""
        TIP_LEN  = 9     # forward tip
        BASE_LEN = 6     # back-side vertices (was 4 -- a bit wider now)
        head_rad = math.radians(heading)
        # Tip: forward in heading direction. round() (not int()) gives
        # symmetric pixel placement across opposite headings, so the
        # marker doesn't visually jitter as it rotates.
        tx = x + round(TIP_LEN * math.cos(head_rad))
        ty = y + round(TIP_LEN * math.sin(head_rad))
        # Back-left and back-right at +/-150 deg from heading.
        bl_rad = math.radians(heading + 150)
        blx = x + round(BASE_LEN * math.cos(bl_rad))
        bly = y + round(BASE_LEN * math.sin(bl_rad))
        br_rad = math.radians(heading - 150)
        brx = x + round(BASE_LEN * math.cos(br_rad))
        bry = y + round(BASE_LEN * math.sin(br_rad))

        coords = array.array('h', [
            tx - x,  ty - y,
            blx - x, bly - y,
            brx - x, bry - y,
        ])
        _canvas.poly(x, y, coords, self.color, True)
