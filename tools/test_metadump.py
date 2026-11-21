#!/usr/bin/env python3
"""Self-check for metadump.py. Run: python3 tools/test_metadump.py"""
import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import metadump as M

LOGO = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "WaterRace Data", "Core", "Logo Intro")

# the values port/test-metafile.cpp and port/test-textures.cpp (manifest.json) assert for Core/Logo Intro
lines, fails = M.dump(LOGO, "Logo Intro")
assert fails == 0, [l for l in lines if l.startswith("FAIL")]
assert lines[0] == "file Logo Intro version=0x0104 creator=MFEd tags=86", lines[0]
assert "count Logo Intro shap 16" in lines
shapes = [l for l in lines if l.startswith("shape ")]
assert len(shapes) == 16 and sum(int(l.split("shading=")[1].split()[0]) < M.FX for l in shapes) == 11
assert [l for l in lines if l.startswith("cptx ")] == ["cptx Logo Intro %s" % t for t in
    ("Wrlg 512 128", "Flr1 256 256", "Flr2 64 64", "Flr3 16 16", "Flr5 64 64", "Flr4 64 64")]
assert lines == M.dump(LOGO, "Logo Intro")[0]

# a shape whose pointCount no longer matches its pnts subtag fails the size check
b = bytearray(open(LOGO, "rb").read())
tpos = M.u32(b, 8)
for i in range(M.u32(b, tpos)):
    e = tpos + 4 + 24 * i
    if b[e + 4:e + 8] == b"shap":
        pos = M.i32(b, e + 20)
        struct.pack_into(">i", b, pos, M.i32(b, pos) + 1)
        break
with tempfile.NamedTemporaryFile() as f:
    f.write(b)
    f.flush()
    lines, fails = M.dump(f.name, "bad")
assert fails == 1 and lines[-1].startswith("FAIL bad #") and "<-->/pnts size 8064, expected 8088" in lines[-1], lines[-1]
print("ok")
