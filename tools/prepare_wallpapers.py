#!/usr/bin/env python3
import os
import shutil
import sys

out_dir = sys.argv[1]
for source in sys.argv[2:]:
    if not os.path.isfile(source):
        continue
    name = os.path.basename(source)
    shutil.copyfile(source, os.path.join(out_dir, name))
    # Keep the source image intact.  The framebuffer/GDI wallpaper loaders
    # decode JPEG directly at runtime; no generated BMP companion is needed.
    root, ext = os.path.splitext(name)
    if ext.lower() in (".jpg", ".jpeg"):
        stale_bmp = os.path.join(out_dir, root + ".bmp")
        if os.path.exists(stale_bmp):
            os.remove(stale_bmp)
with open(os.path.join(out_dir, ".stamp"), "w"):
    pass
