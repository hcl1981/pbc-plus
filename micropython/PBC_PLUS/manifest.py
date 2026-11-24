# Frozen module manifest for the PicoBoy Color Plus.
#
# The rp2 port defaults (asyncio etc.) plus the board-specific helper
# modules, so `import pbc` and `import turtle` resolve immediately on
# a fresh device, without any user files on the filesystem.

# The same as $(PORT_DIR)/boards/manifest.py, but without the port's
# _boot.py: modules_python/_boot.py takes its place (error screen
# instead of silently formatting a damaged filesystem).
freeze("$(PORT_DIR)/modules", "rp2.py")
include("$(MPY_DIR)/extmod/asyncio")
require("onewire")
require("ds18x20")
require("dht")
require("neopixel")

# Path is relative to this manifest.py.
freeze("modules_python")
