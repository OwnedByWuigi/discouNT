#!/usr/bin/env python3
import os
import shutil
import sys
from PIL import Image

out_dir = sys.argv[1]
for source in sys.argv[2:]:
    if not os.path.isfile(source):
        continue
    name = os.path.basename(source)
    shutil.copyfile(source, os.path.join(out_dir, name))
    root, ext = os.path.splitext(name)
    if ext.lower() in (".jpg", ".jpeg", ".png", ".gif", ".webp"):
        try:
            Image.open(source).convert("RGB").save(os.path.join(out_dir, root + ".bmp"), "BMP")
        except Exception as error:
            print("warning: could not convert wallpaper %s: %s" % (source, error), file=sys.stderr)
with open(os.path.join(out_dir, ".stamp"), "w"):
    pass
