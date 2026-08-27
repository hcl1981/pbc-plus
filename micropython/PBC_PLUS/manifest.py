# Frozen module manifest for the PicoBoy Color Plus.
#
# Inherits the rp2 port defaults (asyncio, _boot, etc.) and adds the
# board-specific helper modules so `import pbc` and `import turtle`
# resolve immediately on a fresh device, without any user files on
# the filesystem.

include("$(PORT_DIR)/boards/manifest.py")

# Path is relative to this manifest.py.
freeze("modules_python")
