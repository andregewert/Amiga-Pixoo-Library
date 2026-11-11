#!/usr/bin/env python3
"""Writes the GIF files of the host test and their expected frames.

The GIFs are encoded here (own LZW encoder) so that every feature of the
decoder in src/library/pixoo_image.c is exercised on purpose: global and local
colour tables, transparency, the disposal methods 1, 2 and 3, interlacing,
frames smaller than the screen, code sizes up to 12 bits with clear codes,
and many frames. A simple compositor computes how each frame must look
(ARGB canvas, alpha 0 = transparent); test/gifs.expected gets one line per
frame: file, frame number, delay in ms, CRC-32 of the canvas.

If Pillow is installed, its decoding of the frames is compared with the
expected result as an independent check of the encoder.

Usage: tools/mktestgifs.py [outdir]   (default: test)
"""
import os
import struct
import sys
import zlib


def lzw_encode(indices, minsize):
    """LZW-compress colour indices as GIF wants it (LSB first bit packing)."""
    clear, eoi = 1 << minsize, (1 << minsize) + 1
    out = bytearray()
    bits = nbits = 0

    def put(code, size):
        nonlocal bits, nbits
        bits |= code << nbits
        nbits += size
        while nbits >= 8:
            out.append(bits & 255)
            bits >>= 8
            nbits -= 8

    def reset():
        return {(i,): i for i in range(clear)}, eoi + 1, minsize + 1

    table, nxt, size = reset()
    put(clear, size)
    cur = ()
    for px in indices:
        if cur + (px,) in table:
            cur += (px,)
            continue
        put(table[cur], size)
        if nxt < 4096:
            table[cur + (px,)] = nxt
            nxt += 1
            if nxt > (1 << size) and size < 12:
                size += 1
        else:
            put(clear, size)
            table, nxt, size = reset()
        cur = (px,)
    if cur:
        put(table[cur], size)
    put(eoi, size)
    if nbits:
        out.append(bits & 255)
    blocks = bytearray([minsize])
    for i in range(0, len(out), 255):
        part = out[i:i + 255]
        blocks += bytes([len(part)]) + part
    return bytes(blocks + b"\0")


def table_bytes(colours):
    """Colour table padded to a power of two; returns (bytes, size code)."""
    n, code = 2, 0
    while n < len(colours):
        n *= 2
        code += 1
    data = b"".join(bytes(c) for c in colours) + b"\0\0\0" * (n - len(colours))
    return data, code


class Frame:
    """One image of a GIF: rectangle, pixels (indices), options."""

    def __init__(self, x, y, w, h, pixels, delay=10, transp=None, disposal=0,
                 lct=None, interlace=False):
        self.x, self.y, self.w, self.h = x, y, w, h
        self.pixels = pixels
        self.delay, self.transp, self.disposal = delay, transp, disposal
        self.lct, self.interlace = lct, interlace


def write_gif(path, w, h, gct, frames, gce=True):
    out = bytearray(b"GIF89a")
    flags = 0
    if gct:
        gdata, gcode = table_bytes(gct)
        flags = 0x80 | gcode
    out += struct.pack("<HHBBB", w, h, flags, 0, 0)
    if gct:
        out += gdata
    if len(frames) > 1:  # NETSCAPE loop extension, ignored by the decoder
        out += b"\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00"
    for f in frames:
        if gce:
            pf = (f.disposal << 2) | (1 if f.transp is not None else 0)
            out += struct.pack("<BBBBHBB", 0x21, 0xF9, 4, pf, f.delay, f.transp or 0, 0)
        flags = 0x40 if f.interlace else 0
        if f.lct:
            ldata, lcode = table_bytes(f.lct)
            flags |= 0x80 | lcode
        out += struct.pack("<BHHHHB", 0x2C, f.x, f.y, f.w, f.h, flags)
        if f.lct:
            out += ldata
        rows = list(range(f.h))
        if f.interlace:
            rows = (list(range(0, f.h, 8)) + list(range(4, f.h, 8)) +
                    list(range(2, f.h, 4)) + list(range(1, f.h, 2)))
        data = [f.pixels[r * f.w + c] for r in rows for c in range(f.w)]
        ncol = len(f.lct or gct)
        minsize = max(2, (ncol - 1).bit_length())
        out += lzw_encode(data, minsize)
    out += b"\x3b"
    with open(path, "wb") as fh:
        fh.write(out)


def compose(w, h, gct, frames):
    """Expected canvases (ARGB bytes) and delays in ms, like a browser."""
    canvas = bytearray(w * h * 4)
    result = []
    prev_disp, prev_rect, saved = 0, None, None
    for f in frames:
        if prev_disp == 2:
            x0, y0, pw, ph = prev_rect
            for y in range(y0, min(y0 + ph, h)):
                for x in range(x0, min(x0 + pw, w)):
                    canvas[(y * w + x) * 4:(y * w + x) * 4 + 4] = b"\0\0\0\0"
        elif prev_disp == 3 and saved is not None:
            canvas[:] = saved
        if f.disposal == 3:
            saved = bytes(canvas)
        ct = f.lct or gct
        for r in range(f.h):
            for c in range(f.w):
                i = f.pixels[r * f.w + c]
                x, y = f.x + c, f.y + r
                if i == f.transp or i >= len(ct) or x >= w or y >= h:
                    continue
                canvas[(y * w + x) * 4:(y * w + x) * 4 + 4] = bytes([255]) + bytes(ct[i])
        delay = 100 if f.delay < 2 else f.delay * 10
        result.append((bytes(canvas), delay))
        prev_disp, prev_rect = f.disposal, (f.x, f.y, f.w, f.h)
    return result


def lcg(seed):
    """Deterministic pseudo-random numbers (same on every machine)."""
    while True:
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF
        yield seed >> 16


def palette(n, seed):
    r = lcg(seed)
    return [(next(r) & 255, next(r) & 255, next(r) & 255) for _ in range(n)]


def make_set():
    """All test files: name -> (w, h, gct, frames)."""
    files = {}

    # still picture, 16 colours, wider than high
    gct = palette(16, 1)
    px = [((x // 3) + (y // 2)) % 16 for y in range(10) for x in range(20)]
    files["still.gif"] = (20, 10, gct, [Frame(0, 0, 20, 10, px, delay=0)])

    # animation: background, transparent overlay kept, a frame restored to
    # background (transparent), one restored to previous with a local table
    gct = palette(8, 2)
    bg = [(x // 4 + y // 4) % 8 for y in range(32) for x in range(32)]
    over = [7 if (x + y) % 3 else 0 for y in range(10) for x in range(12)]
    box = [3] * (8 * 8)
    lct = palette(4, 3)
    loc = [(x * y) % 4 for y in range(6) for x in range(20)]
    files["anim.gif"] = (32, 32, gct, [
        Frame(0, 0, 32, 32, bg, delay=5),
        Frame(4, 6, 12, 10, over, delay=1, transp=0, disposal=1),
        Frame(20, 20, 8, 8, box, delay=7, disposal=2),
        Frame(10, 24, 20, 6, loc, delay=12, disposal=3, lct=lct, transp=1),
        Frame(0, 0, 4, 4, [5] * 16, delay=100),
    ])

    # interlaced noise in 256 colours: code sizes up to 12, clear codes
    gct = palette(256, 4)
    r = lcg(5)
    px = [next(r) & 255 for _ in range(100 * 77)]
    files["interlace.gif"] = (100, 77, gct, [Frame(0, 0, 100, 77, px, transp=17, interlace=True)])

    # many frames (thinned out when sent)
    gct = palette(4, 6)
    # (a diagonal moving one step per frame, so thinned-out frames differ)
    frames = [Frame(0, 0, 8, 8, [3 if x + y == i % 15 else i // 40 for y in range(8) for x in range(8)],
                    delay=4)
              for i in range(120)]
    files["many.gif"] = (8, 8, gct, frames)
    return files


def pillow_check(path, expected, w, h):
    try:
        from PIL import Image
    except ImportError:
        return
    im = Image.open(path)
    for n, (canvas, _) in enumerate(expected):
        im.seek(n)
        got = im.convert("RGBA").tobytes()
        for i in range(w * h):
            a = canvas[i * 4]
            if got[i * 4 + 3] != a or (a and got[i * 4:i * 4 + 3] != canvas[i * 4 + 1:i * 4 + 4]):
                # Pillow fills disposal 2 with its background colour: only
                # the frames before such a disposal are compared
                raise SystemExit("%s frame %d pixel %d differs from Pillow" % (path, n, i))
        if n + 1 < len(expected) and im.disposal_method in (2, 3):
            break


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "test"
    lines = []
    for name, (w, h, gct, frames) in make_set().items():
        path = os.path.join(outdir, name)
        write_gif(path, w, h, gct, frames)
        expected = compose(w, h, gct, frames)
        pillow_check(path, expected, w, h)
        for n, (canvas, delay) in enumerate(expected):
            lines.append("%s %d %d %08x" % (name, n, delay, zlib.crc32(canvas)))
    with open(os.path.join(outdir, "gifs.expected"), "w", encoding="latin-1") as fh:
        fh.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
