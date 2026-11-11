#!/usr/bin/env python3
"""Draws the pictures of the PixooCtl demo (demo/Pictures).

Everything is drawn here, no pixels from elsewhere:

  boing.gif    bouncing, spinning checkered ball, 64x64, animated
  copper.gif   moving rainbow raster bars ("copper bars"), animated
  boing.iff    still ball in 320x256 (IFF ILBM, shrunk by the library)
  brush.iff    ball with a transparent background colour (ILBM brush)
  floppy.iff   a 3.5" disk (ILBM)

The ILBM files are written with an own encoder (ByteRun1), the GIFs
with the encoder of tools/mktestgifs.py, so no Python packages are needed.

Usage: tools/mkdemo.py [outdir]   (default: demo/Pictures)
"""
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mktestgifs import Frame, write_gif  # noqa: E402

# --- ILBM -------------------------------------------------------------------


def byterun1(row):
    """Compress one plane row with ByteRun1 (PackBits)."""
    out = bytearray()
    i, n = 0, len(row)
    while i < n:
        j = i
        while j + 1 < n and row[j + 1] == row[i] and j - i < 127:
            j += 1
        if j - i + 1 >= 3:
            out += bytes([257 - (j - i + 1), row[i]])
            i = j + 1
            continue
        start = i
        while i < n and i - start < 128:
            if i + 2 < n and row[i] == row[i + 1] == row[i + 2]:
                break
            i += 1
        out += bytes([i - start - 1]) + bytes(row[start:i])
    return bytes(out)


def chunk(cid, data):
    return cid + struct.pack(">I", len(data)) + data + (b"\0" if len(data) & 1 else b"")


def write_ilbm(path, w, h, palette, pixels, transparent=None):
    """Write an IFF ILBM picture; transparent: colour index or None."""
    planes = max(1, (len(palette) - 1).bit_length())
    pal = list(palette) + [(0, 0, 0)] * ((1 << planes) - len(palette))
    rowbytes = (w + 15) // 16 * 2
    body = bytearray()
    for y in range(h):
        row = pixels[y * w:(y + 1) * w]
        for p in range(planes):
            plane = bytearray(rowbytes)
            for x, c in enumerate(row):
                if (c >> p) & 1:
                    plane[x >> 3] |= 0x80 >> (x & 7)
            body += byterun1(plane)
    masking = 2 if transparent is not None else 0
    bmhd = struct.pack(">HHhhBBBBHBBhh", w, h, 0, 0, planes, masking, 1, 0,
                       transparent or 0, 10, 11, max(w, 320), max(h, 200))
    cmap = b"".join(bytes(c) for c in pal)
    form = b"ILBM" + chunk(b"BMHD", bmhd) + chunk(b"CMAP", cmap) + chunk(b"BODY", bytes(body))
    with open(path, "wb") as f:
        f.write(chunk(b"FORM", form))


# --- the ball ---------------------------------------------------------------

BG, GRID, BG_SH, GRID_SH = 0, 1, 2, 3       # background colours
RED0, WHITE0, SHADES = 4, 12, 8             # 8 shades of red and of white


def ball_palette():
    pal = [(170, 170, 170), (160, 32, 160), (96, 96, 96), (90, 18, 90)]
    for i in range(SHADES):
        f = 0.35 + 0.65 * i / (SHADES - 1)
        pal.append((int(235 * f), int(20 * f), int(20 * f)))
    for i in range(SHADES):
        f = 0.35 + 0.65 * i / (SHADES - 1)
        pal.append((int(255 * f), int(255 * f), int(255 * f)))
    return pal


def grid_background(w, h, step, offset=0):
    return [GRID if (x - offset) % step == 0 or (y - offset) % step == 0 else BG
            for y in range(h) for x in range(w)]


def draw_shadow(img, w, h, cx, cy, r):
    for y in range(max(0, int(cy - r)), min(h, int(cy + r) + 2)):
        for x in range(max(0, int(cx - r)), min(w, int(cx + r) + 2)):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                c = img[y * w + x]
                if c == BG:
                    img[y * w + x] = BG_SH
                elif c == GRID:
                    img[y * w + x] = GRID_SH


def draw_ball(img, w, h, cx, cy, r, phase, tilt=0.35, seg=math.pi / 8):
    """Checkered ball (fields of seg radians, 8 x 8 per hemisphere by default),
    lit from the top left."""
    lx, ly, lz = -0.45, -0.55, 0.70
    ln = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ln, ly / ln, lz / ln
    ct, st = math.cos(tilt), math.sin(tilt)
    for y in range(max(0, int(cy - r) - 1), min(h, int(cy + r) + 2)):
        for x in range(max(0, int(cx - r) - 1), min(w, int(cx + r) + 2)):
            nx, ny = (x + 0.5 - cx) / r, (y + 0.5 - cy) / r
            d = nx * nx + ny * ny
            if d > 1:
                continue
            nz = math.sqrt(1 - d)
            # undo the tilt of the axis, then longitude and latitude
            tx, ty = nx * ct + ny * st, -nx * st + ny * ct
            lon = math.atan2(tx, nz) + phase
            lat = math.asin(max(-1.0, min(1.0, ty)))
            red = (math.floor(lon / seg) + math.floor(lat / seg)) % 2 == 0
            light = max(0.0, nx * lx + ny * ly + nz * lz)
            shade = min(SHADES - 1, int(1 + light * (SHADES - 1)))
            img[y * w + x] = (RED0 if red else WHITE0) + shade


def boing_anim(path):
    w = h = 64
    r = 13
    frames = []
    n = 32
    for k in range(n):
        img = grid_background(w, h, 8, 4)
        cy = 49 - 30 * abs(math.sin(math.pi * k / 16))
        cx = 32 + 15 * math.sin(2 * math.pi * k / n)
        draw_shadow(img, w, h, cx + 5, cy + 3, r)
        # larger fields: 6 x 6 per hemisphere read better at this size
        draw_ball(img, w, h, cx, cy, r, -2 * math.pi * k / n, seg=math.pi / 6)
        frames.append(Frame(0, 0, w, h, img, delay=4))
    write_gif(path, w, h, ball_palette(), frames)


def boing_still(path):
    w, h, r = 320, 256, 92
    img = grid_background(w, h, 32, 16)
    draw_shadow(img, w, h, 186, 140, r)
    draw_ball(img, w, h, 160, 124, r, 0.2)
    write_ilbm(path, w, h, ball_palette(), img)


def boing_brush(path):
    w = h = 48
    img = [BG] * (w * h)
    draw_ball(img, w, h, 24, 24, 22, 0.5)
    write_ilbm(path, w, h, ball_palette(), img, transparent=BG)


# --- copper bars ------------------------------------------------------------

def copper_anim(path):
    w = h = 64
    hues = [(255, 40, 40), (255, 150, 0), (255, 240, 0), (40, 220, 40),
            (0, 220, 230), (60, 90, 255), (220, 60, 255)]
    pal = [(0, 0, 16)]
    for col in hues:                        # 8 shades per bar, dark to bright
        for i in range(8):
            f = 0.2 + 0.8 * i / 7
            pal.append(tuple(int(c * f) for c in col))
    frames = []
    n = 32
    for k in range(n):
        rows = [0] * h
        for b in range(len(hues)):
            centre = 32 + 24 * math.sin(2 * math.pi * k / n + b * 0.55)
            for dy in range(-4, 5):
                y = int(round(centre)) + dy
                if 0 <= y < h:
                    rows[y] = 1 + b * 8 + (7 - abs(dy) * 7 // 4 if abs(dy) < 4 else 0)
        img = [rows[y] for y in range(h) for _ in range(w)]
        frames.append(Frame(0, 0, w, h, img, delay=5))
    write_gif(path, w, h, pal, frames)


# --- floppy disk -------------------------------------------------------------

def floppy(path):
    w = h = 64
    pal = [(0, 0, 0), (40, 50, 90), (25, 30, 60), (190, 195, 205), (120, 125, 140),
           (245, 245, 235), (230, 60, 50), (60, 60, 70)]
    img = [0] * (w * h)

    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                img[y * w + x] = c

    rect(4, 4, 59, 59, 1)                   # body
    for i in range(5):                      # cut corner at the top right
        rect(59 - i, 4, 59, 4 + 4 - i, 0)
    rect(5, 58, 58, 59, 2)                  # lower edge
    rect(18, 4, 45, 22, 3)                  # metal shutter
    rect(36, 7, 41, 19, 7)                  # its window
    rect(18, 21, 45, 22, 4)
    rect(10, 30, 53, 57, 5)                 # label
    for y in (36, 42, 48):                  # lines on the label
        rect(14, y, 49, y, 4)
    rect(10, 30, 53, 32, 6)                 # red stripe
    rect(6, 52, 8, 55, 7)                   # write protect hole
    write_ilbm(path, w, h, pal, img)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join("demo", "Pictures")
    os.makedirs(out, exist_ok=True)
    boing_anim(os.path.join(out, "boing.gif"))
    copper_anim(os.path.join(out, "copper.gif"))
    boing_still(os.path.join(out, "boing.iff"))
    boing_brush(os.path.join(out, "brush.iff"))
    floppy(os.path.join(out, "floppy.iff"))


if __name__ == "__main__":
    main()
