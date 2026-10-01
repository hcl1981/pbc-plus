# Collision demo for the PicoBoy Color Plus -- all three checks.
#
# Part 1 (sprites): move the white ball with the joystick.
#   red LED    = ball touches the red lava strip   -> hero.on_color(pbc.RED)
#   yellow LED = ball touches the blue ball        -> hero.touches(enemy)
#   Press A to go on to part 2.
# Part 2 (turtle): the turtle drives around and turns whenever the
#   yellow frame is just ahead                     -> t.on_color(pbc.YELLOW)
#
# Needs no image files: the sprite pictures are drawn in code.
import pbc
import turtle

# ---- part 1: sprites ------------------------------------------------

bg = pbc.Canvas(pbc.WIDTH, pbc.HEIGHT)
bg.fill(pbc.BLACK)
bg.fill_rect(0, 200, pbc.WIDTH, 20, pbc.RED)
bg.text("Lava", 104, 206, pbc.WHITE)
pbc.Sprite._background = bg

def ball(size, color):
    img = pbc.Canvas(size, size, transparent=pbc.MAGENTA)
    img.fill(pbc.MAGENTA)                       # transparent corners
    r = size // 2 - 1
    img.ellipse(size // 2, size // 2, r, r, color, True)
    return img

hero = pbc.Sprite(ball(16, pbc.WHITE))
enemy = pbc.Sprite(ball(16, pbc.BLUE))
hero.set_to(112, 40)
enemy.set_to(40, 120)
hero.show()
enemy.show()

while not pbc.was_pressed_a():
    dx = (2 if pbc.pressed_right() else 0) - (2 if pbc.pressed_left() else 0)
    dy = (2 if pbc.pressed_down() else 0) - (2 if pbc.pressed_up() else 0)
    if dx or dy:
        hero.set_by(dx, dy)
    pbc.led_red(hero.on_color(pbc.RED))
    pbc.led_yellow(hero.touches(enemy))
    pbc.delay(16)

pbc.leds_off()
hero.hide()
enemy.hide()
pbc.Sprite._background = None

# ---- part 2: turtle -------------------------------------------------

cv = pbc.Canvas(pbc.WIDTH, pbc.HEIGHT)
cv.fill(pbc.BLACK)
# The frame is 4 px thick: the turtle probes one pixel 3 px ahead, so
# a 1-px line could slip between probe and turtle right after a turn.
for i in range(4):
    cv.rect(20 + i, 20 + i, 200 - 2 * i, 240 - 2 * i, pbc.YELLOW)
t = turtle.Turtle(canvas=cv)
t.speed(0)
t.setPenColor(pbc.CYAN)
t.setDirection(30)
for _ in range(600):
    if t.on_color(pbc.YELLOW):
        t.right(110)
    else:
        t.forward(1)
