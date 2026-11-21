#!/usr/bin/env python3
"""Self-check for pict2png.py. Run: python3 tools/test_pict2png.py"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pict2png as P

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "WaterRace Data", "Interface.rsrc_fork")


def synth(packtype, pixsize, cmpcount, rows, w, h):
    """Minimal v2 PICT holding one DirectBitsRect with PackBits-literal rows."""
    rowbytes = w * pixsize // 8
    pm = struct.pack(">HhhhhHHIIIHHHHIII", rowbytes | 0x8000, 0, 0, h, w, 0, packtype, 0, 0, 0,
                     0, pixsize, cmpcount, 8 if pixsize == 32 else 5, 0, 0, 0)
    body = b"\x00\x00\x00\xff" + pm[:46] + struct.pack(">hhhhhhhhH", 0, 0, h, w, 0, 0, h, w, 0)
    for r in rows:
        lit = bytes([len(r) // (2 if packtype == 3 else 1) - 1]) + r
        body += (bytes([len(lit)]) if rowbytes <= 250 else struct.pack(">H", len(lit))) + lit
    return (b"\0\0" + struct.pack(">hhhh", 0, 0, h, w) + b"\x00\x11\x02\xff" + b"\x0c\x00" + bytes(24)
            + b"\x00\x9a" + body + b"\x00\xff")


# packType 4, ARGB planes: pixel (x=0) = A10 R20 G30 B40, (x=1) = A11 R21 G31 B41
img = P.decode(synth(4, 32, 4, [bytes([10, 11, 20, 21, 30, 31, 40, 41])], 2, 1))
assert [img.getpixel((x, 0)) for x in range(2)] == [(20, 30, 40, 10), (21, 31, 41, 11)], [img.getpixel((x, 0)) for x in range(2)]
# packType 3: 0x7C00 = pure red, 0x001F = pure blue
img = P.decode(synth(3, 16, 3, [bytes([0x7C, 0x00, 0x00, 0x1F])], 2, 1))
assert [img.getpixel((x, 0)) for x in range(2)] == [(255, 0, 0, 255), (0, 0, 255, 255)], [img.getpixel((x, 0)) for x in range(2)]
# unknown opcode fails cleanly
try:
    P.decode(b"\0\0" + bytes(8) + b"\x00\x11\x02\xff\x12\x34")
    raise SystemExit("expected PictError")
except P.PictError:
    pass

if os.path.exists(DATA):  # real data: one 0x9A (16-bit) and one 0x8200 jpeg
    fork = open(DATA, "rb").read()
    assert P.decode(P.resource(fork, b"PICT", 6000)).size == (640, 480)
    assert P.decode(P.resource(fork, b"PICT", 5020)).size == (640, 424)
print("ok")
