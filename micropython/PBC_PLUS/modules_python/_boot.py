# _boot -- mounts the filesystem at start-up (PicoBoy Color Plus).
#
# Replaces the _boot.py of the rp2 port, which formats the flash
# without a word whenever the filesystem cannot be mounted. Here:
#
#   - empty flash (new device)      -> format, as before
#   - anything else that won't mount -> error screen; the files are
#     only erased after the user has pressed A and B together
#   - formatting fails               -> error screen, start without files
#
# USB comes up only after this module has run, so while the error
# screen waits for the buttons there is no REPL yet.

import vfs
import rp2


def _mount(bdev):
    vfs.mount(vfs.VfsLfs2(bdev, progsize=256), "/")


def _empty(bdev):
    # littlefs keeps its superblock in blocks 0 and 1.
    buf = bytearray(4096)
    blank = b"\xff" * 4096
    for block in (0, 1):
        bdev.readblocks(block, buf)
        if buf != blank:
            return False
    return True


def _screen(title, lines):
    import display
    d = display.Display()
    d.fill(display.BLACK)
    d.text18(title, (display.WIDTH - d.text18_width(title)) // 2, 44, display.RED)
    y = 92
    for s in lines:
        d.text12(s, (display.WIDTH - d.text12_width(s)) // 2, y, display.WHITE)
        y += 20 if s else 10
    return d


def _ask():
    """Error screen; returns once A and B are held down together."""
    import time
    from machine import Pin
    _screen("File system error", (
        "The files on this device",
        "cannot be read.",
        "",
        "Press A and B together",
        "to erase all files and",
        "start with an empty device.",
    ))
    a = Pin(27, Pin.IN, Pin.PULL_UP)
    b = Pin(28, Pin.IN, Pin.PULL_UP)
    held = 0
    while held < 5:                      # 5 x 20 ms: no glitch, no bounce
        held = held + 1 if not a.value() and not b.value() else 0
        time.sleep_ms(20)
    _screen("Erasing ...", ("Please wait.",))


def _boot():
    bdev = rp2.Flash()
    try:
        _mount(bdev)
        return
    except Exception:
        pass
    try:
        asked = not _empty(bdev)
        if asked:
            _ask()
        vfs.VfsLfs2.mkfs(bdev, progsize=256)
        _mount(bdev)
        if asked:
            import display
            display.Display()            # back to the boot splash
    except Exception:
        _screen("File system error", (
            "The flash memory cannot",
            "be formatted.",
            "",
            "Programs cannot be saved",
            "on this device.",
        ))


_boot()
del _boot, _ask, _screen, _empty, _mount, vfs, rp2
