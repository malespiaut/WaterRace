#!/usr/bin/env python3
"""Decode a raw PICT blob (DirectBits 0x9A/0x9B, PackBits 0x98/0x99, or CompressedQuickTime 0x8200 'jpeg') to PNG.
The mask region of the *Rgn opcodes is skipped (the image is decoded whole).

Usage: pict2png.py FILE.pict OUT.png
       pict2png.py FORK.rsrc_fork ID OUT.png    (PICT resource ID from a raw resource fork)
Unsupported opcodes/codecs are logged to stderr and exit status is 1.
"""
import io
import struct
import sys

from PIL import Image

# fixed-size payloads of the opcodes we skip (v2 PICT)
SKIP = {0x00: 0, 0x02: 8, 0x03: 2, 0x04: 2, 0x05: 2, 0x06: 4, 0x07: 4, 0x08: 2, 0x09: 8,
        0x0A: 8, 0x0B: 4, 0x0C: 4, 0x0D: 2, 0x0E: 4, 0x0F: 4, 0x10: 8, 0x1A: 6,
        0x1B: 6, 0x1C: 6, 0x1E: 0, 0x20: 8, 0x21: 4, 0x22: 6, 0x23: 2, 0x2E: 0, 0x0C00: 24,
        0x00A0: 2}


class PictError(Exception):
    pass


def unpackbits(data, pos, n, unit=1):
    """PackBits-decode (runs of unit-byte items) until n output bytes; returns (bytes, new pos)."""
    out = bytearray()
    while len(out) < n:
        c = data[pos]
        pos += 1
        if c < 128:
            out += data[pos:pos + (c + 1) * unit]
            pos += (c + 1) * unit
        elif c > 128:
            out += data[pos:pos + unit] * (257 - c)
            pos += unit
    return bytes(out[:n]), pos


def direct_bits(d, p, rgn=False):
    p += 4  # baseAddr
    rowbytes, top, left, bottom, right, _ver, packtype, _packsize = struct.unpack(">HhhhhHHI", d[p:p + 18])
    pixsize, cmpcount = struct.unpack(">HH", d[p + 28:p + 32])
    p += 46 + 18  # PixMap, srcRect(8) dstRect(8) mode(2)
    if rgn:
        p += struct.unpack(">H", d[p:p + 2])[0]
    rowbytes &= 0x3FFF
    w, h = right - left, bottom - top
    if pixsize not in (16, 32) or packtype not in (0, 1, 3, 4):
        raise PictError(f"DirectBitsRect pixelSize {pixsize} packType {packtype}")
    img = Image.new("RGBA", (w, h))
    px = []
    for _ in range(h):
        if packtype in (0, 1):
            row = d[p:p + rowbytes]
            p += rowbytes
        else:
            if rowbytes > 250:
                (n,) = struct.unpack(">H", d[p:p + 2])
                p += 2
            else:
                n = d[p]
                p += 1
            row, _ = unpackbits(d, p, w * cmpcount, 1) if packtype == 4 else unpackbits(d, p, w * 2, 2)
            p += n
        if pixsize == 16:  # xRRRRRGGGGGBBBBB
            for x in range(w):
                v = (row[2 * x] << 8) | row[2 * x + 1]
                r, g, b = (v >> 10) & 31, (v >> 5) & 31, v & 31
                px.append(((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2), 255))
        elif packtype in (0, 1):  # raw ARGB
            px += [(row[4 * x + 1], row[4 * x + 2], row[4 * x + 3], 255) for x in range(w)]
        else:  # planar: [A] R G B
            pl = [row[i * w:(i + 1) * w] for i in range(cmpcount)]
            if cmpcount == 4:
                px += list(zip(pl[1], pl[2], pl[3], pl[0]))
            else:
                px += list(zip(pl[0], pl[1], pl[2], b"\xff" * w))
    img.putdata(px)
    return img


def packbits_rect(d, p, rgn=False):
    """Indexed PixMap (or 1-bit BitMap) + colour table, PackBits rows."""
    rowbytes, top, left, bottom, right = struct.unpack(">Hhhhh", d[p:p + 10])
    w, h = right - left, bottom - top
    if rowbytes & 0x8000:
        (pixsize,) = struct.unpack(">H", d[p + 28:p + 30])
        p += 46
        _seed, flags, n = struct.unpack(">IHH", d[p:p + 8])
        p += 8
        clut = {}
        for i in range(n + 1):
            v, r, g, b = struct.unpack(">4H", d[p:p + 8])
            clut[i if flags & 0x8000 else v] = (r >> 8, g >> 8, b >> 8)
            p += 8
    else:  # BitMap: 1-bit, 0 = white
        pixsize, clut = 1, {0: (255, 255, 255), 1: (0, 0, 0)}
        p += 10
    if pixsize not in (1, 2, 4, 8):
        raise PictError(f"PackBitsRect pixelSize {pixsize}")
    p += 18  # srcRect dstRect mode
    if rgn:
        p += struct.unpack(">H", d[p:p + 2])[0]
    rowbytes &= 0x3FFF
    px = []
    for _ in range(h):
        if rowbytes < 8:
            row = d[p:p + rowbytes]
            p += rowbytes
        else:
            if rowbytes > 250:
                (n,) = struct.unpack(">H", d[p:p + 2])
                p += 2
            else:
                n = d[p]
                p += 1
            row, _ = unpackbits(d, p, rowbytes)
            p += n
        per, mask = 8 // pixsize, (1 << pixsize) - 1
        px += [clut.get((row[x // per] >> (8 - pixsize * (x % per + 1))) & mask, (0, 0, 0)) for x in range(w)]
    img = Image.new("RGB", (w, h))
    img.putdata(px)
    return img.convert("RGBA")


def quicktime(d, p):
    q = p + 4 + 68  # version, matrix, matte size/rect, mode, srcRect, accuracy, maskSize
    (matte,) = struct.unpack(">I", d[p + 4 + 38:p + 4 + 42])
    (mask,) = struct.unpack(">I", d[p + 4 + 64:p + 4 + 68])
    q += matte + mask
    dsize, codec = struct.unpack(">I4s", d[q:q + 8])
    if codec != b"jpeg":
        raise PictError(f"QuickTime codec {codec!r}")
    (datalen,) = struct.unpack(">I", d[q + 44:q + 48])
    return Image.open(io.BytesIO(d[q + dsize:q + dsize + datalen])).convert("RGBA")


def decode(d):
    """Raw PICT bytes (no 512-byte file header) -> RGBA PIL image. Raises PictError."""
    p = 10
    if d[p:p + 2] != b"\x00\x11":
        raise PictError("not a version 2 PICT")
    p += 4  # version op + 02FF
    while p + 2 <= len(d):
        (op,) = struct.unpack(">H", d[p:p + 2])
        p += 2
        if op in (0x009A, 0x009B):
            return direct_bits(d, p, op == 0x009B)
        if op in (0x0098, 0x0099):
            return packbits_rect(d, p, op == 0x0099)
        if op == 0x8200:
            return quicktime(d, p)
        if op == 0x00FF:
            raise PictError("no image opcode found")
        if op == 0x0001:  # clip region
            p += struct.unpack(">H", d[p:p + 2])[0]
        elif op == 0x00A1:
            p += 4 + struct.unpack(">H", d[p + 2:p + 4])[0]
        elif op in SKIP:
            p += SKIP[op]
        else:
            raise PictError(f"unsupported opcode 0x{op:04X}")
    raise PictError("truncated PICT")


def resource(fork, rtype, rid):
    do, mo, _dl, ml = struct.unpack(">4I", fork[:16])
    m = fork[mo:mo + ml]
    (to,) = struct.unpack(">H", m[24:26])
    (n,) = struct.unpack(">H", m[to:to + 2])
    for i in range(n + 1):
        t, k, ro = struct.unpack(">4sHH", m[to + 2 + i * 8:to + 10 + i * 8])
        if t != rtype:
            continue
        for j in range(k + 1):
            rid_, _no, ao = struct.unpack(">hHI", m[to + ro + j * 12:to + ro + j * 12 + 8])
            if rid_ == rid:
                o = do + (ao & 0xFFFFFF)
                (ln,) = struct.unpack(">I", fork[o:o + 4])
                return fork[o + 4:o + 4 + ln]
    raise KeyError(f"{rtype.decode()} {rid}")


def main(argv):
    try:
        if len(argv) == 3:
            blob = open(argv[1], "rb").read()
        elif len(argv) == 4:
            blob = resource(open(argv[1], "rb").read(), b"PICT", int(argv[2]))
        else:
            print(__doc__, file=sys.stderr)
            return 2
        decode(blob).save(argv[-1])
    except (PictError, KeyError, IndexError, struct.error, OSError) as e:
        print(f"pict2png: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
