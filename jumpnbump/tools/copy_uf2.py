# Legt die fertige Firmware nach jedem Build als jumpnbump.uf2 in die
# Projektwurzel - dort ist sie leichter zu finden als tief in .pio/build/.
import shutil
import os

Import("env")  # noqa: F821  (von PlatformIO bereitgestellt)


def copy_uf2(source, target, env):
    src = os.path.join(env.subst("$BUILD_DIR"), "firmware.uf2")
    dst = os.path.join(env.subst("$PROJECT_DIR"), "jumpnbump.uf2")
    if os.path.exists(src):
        shutil.copyfile(src, dst)
        print("Firmware kopiert nach: %s (%.0f kB)" % (dst, os.path.getsize(dst) / 1024.0))


env.AddPostAction("buildprog", copy_uf2)  # noqa: F821
