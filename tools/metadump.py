#!/usr/bin/env python3
"""Independent dump and size check of every MetaFile (PORTING.md 5.2, P4). Shares no code with the C++ loaders.

Usage: metadump.py "WaterRace Data"
Prints one record per line, in a fixed order, so a C++ test can print the same lines and diff them:
  file <path> version=0x0104 creator=MFEd tags=<n>      (or "file <path> empty" for a resource-fork-only file)
  count <path> <tag>[/<subtag>] <n>
  shape <path> <id> shading=<s> points=<p> triangles=<t> normals=<n> bbox=<x0>,<y0>,<z0>,<x1>,<y1>,<z1> name=<name>
  mesh <path> <tmsh|smsh> <id> points=<p> triangles=<t> bbox=... name=<name>
  cptx <path> <name> <width> <height>
  FAIL <path> #<entry> <tag>[/<subtag>] <reason>
Exit status is 1 if any FAIL line was printed.
"""
import collections
import os
import struct
import sys

SKIP = (".rsrc_fork", ".png", ".json")
NODATA = -1

# on-disk sizes (MetaFile Read.cp); None = variable, checked below or not at all
FIXED = {"shap": 300, "txst": 1680, "scrp": 192, "cmra": 136, "terr": 5636, "tmsh": 12792, "smsh": 12796,
         "cmly": 300, "cmps": 56, "view": 120,
         "data/chds": 1136, "data/shds": 1068, "data/shpp": 704, "data/lcpy": 256, "data/vers": 84}
MEGA = {0x0300: 3004, 0x0310: 3932}  # by tag version
TRDS = (1392, 1396)
SHDT = {16: 56, 17: 76, 19: 28, 18: 104}  # by shape shading: sprite, light, lens, particle (audio is variable)
FX = 16  # kShader_FXOffset: shading >= 16 has no 3D geometry
PARTICLE = 18


def u32(b, p):
    return struct.unpack_from(">I", b, p)[0]


def i32(b, p):
    return struct.unpack_from(">i", b, p)[0]


def bbox(b, p):
    """Min and max over the 8 corner Vectors at p."""
    v = struct.unpack_from(">24f", b, p)
    return ",".join("%g" % f for f in [min(v[i::3]) for i in range(3)] + [max(v[i::3]) for i in range(3)])


def pstr(b, p):
    return b[p + 1:p + 1 + min(b[p], 31)].decode("latin-1")


def fourcc(b):
    return b.decode("latin-1")


def var_size(key, pay, owner):
    """Expected size of a variable payload from its own header or its owner tag, or None if unchecked."""
    if key == "<-->/anim":
        return 24 + 32 * u32(pay, 20) if len(pay) >= 24 else 24
    if key == "sklt":
        return None if len(pay) >= 25632 and (len(pay) - 25632) % 24 == 0 else -1
    if key == "mdan":
        return 96 + 32 * u32(pay, 92) if len(pay) >= 96 else 96
    if key == "trck":
        return 316 + 68 * u32(pay, 312) if len(pay) >= 316 else 316
    if key == "encl":
        return 44 + 92 * i32(pay, 40) if len(pay) >= 44 else 44
    if key == "item":
        return None if len(pay) % 28 == 0 else -1
    if key == "cptx":
        w, h, alpha = struct.unpack_from(">HH", pay, 8) + struct.unpack_from(">h", pay, 24) if len(pay) >= 28 else (0, 0, 0)
        return 28 + (w * h if alpha else 0)
    if owner is None:
        return None
    otag, opay = owner
    if otag == "shap":
        shading = struct.unpack_from(">H", opay, 226)[0]
        if key == "<-->/pnts":
            if shading < FX:
                return 24 * i32(opay, 0)
            if shading == PARTICLE:
                return None if len(pay) % 80 == 0 else -1
        if key == "<-->/trig":
            return 12 * i32(opay, 8)
        if key == "<-->/nrls":
            return 12 * i32(opay, 24)
        if key == "<-->/shdt":
            return SHDT.get(shading)
    if otag in ("tmsh", "smsh"):
        if key in ("<-->/pnts", "<-->/ligt"):
            return 24 * i32(opay, 112)
        if key == "<-->/trig":
            return 12 * i32(opay, 124)
    return None


def dump(path, rel):
    """Returns (lines, failure count) for one MetaFile."""
    b = open(path, "rb").read()
    if not b:
        return ["file %s empty" % rel], 0
    out, fails = [], []
    version, flags, creator, tpos, tsize = struct.unpack_from(">hH4sii", b, 0)
    n = u32(b, tpos)
    out.append("file %s version=0x%04x creator=%s tags=%d" % (rel, version, fourcc(creator), n))
    if version != 0x0104:
        fails.append("FAIL %s header version 0x%04x" % (rel, version))
    if tsize != 4 + 24 * n or tpos + tsize > len(b):
        fails.append("FAIL %s header tag table %d+%d for %d entries" % (rel, tpos, tsize, n))
        return out + fails, len(fails)

    counts = collections.Counter()
    records = []
    owner = None  # last master tag, for its <--> subtags
    prev_depth = -1
    for i in range(n):
        depth, eflags, ever, tag, sub, eid, size, pos = struct.unpack_from(">BBh4s4s4sii", b, tpos + 4 + 24 * i)
        tag, sub, eid = fourcc(tag), fourcc(sub), fourcc(eid)
        key = tag + ("/" + sub if tag in ("data", "<-->") else "")
        counts[key] += 1
        where = "FAIL %s #%d %s" % (rel, i, key)
        if depth > prev_depth + 1:
            fails.append("%s depth %d after %d" % (where, depth, prev_depth))
        prev_depth = depth
        if size == NODATA:
            if pos != NODATA:
                fails.append("%s no data but position %d" % (where, pos))
            continue
        if size < 0 or pos < 16 or pos + size > len(b):
            fails.append("%s payload %d+%d outside file of %d" % (where, pos, size, len(b)))
            continue
        pay = b[pos:pos + size]

        if tag == "mega":
            want = MEGA.get(ever & 0xFFFF, -1)
        elif key == "data/trds":
            want = size if size in TRDS else TRDS[0]
        else:
            want = FIXED.get(key, var_size(key, pay, owner))
        if want is not None and size != want:
            fails.append("%s size %d, expected %d" % (where, size, want))
            continue
        if tag != "<-->":
            owner = (tag, pay)

        if tag == "shap":
            pts, tris, normals, shading = i32(pay, 0), i32(pay, 8), i32(pay, 24), struct.unpack_from(">H", pay, 226)[0]
            records.append("shape %s %s shading=%d points=%d triangles=%d normals=%d bbox=%s name=%s"
                           % (rel, fourcc(pay[296:300]), shading, pts, tris, normals, bbox(pay, 104), pstr(pay, 230)))
        elif tag in ("tmsh", "smsh"):
            records.append("mesh %s %s %s points=%d triangles=%d bbox=%s name=%s"
                           % (rel, tag, fourcc(pay[32:36]), i32(pay, 112), i32(pay, 124), bbox(pay, 132), pstr(pay, 0)))
        elif tag == "cptx":
            w, h = struct.unpack_from(">HH", pay, 8)
            records.append("cptx %s %s %d %d" % (rel, fourcc(pay[0:4]), w, h))

    out += ["count %s %s %d" % (rel, k, counts[k]) for k in sorted(counts)]
    return out + records + fails, len(fails)


def files(root):
    for d, dirs, names in os.walk(root):
        dirs.sort()
        for name in sorted(names):
            if not name.endswith(SKIP):
                p = os.path.join(d, name)
                yield p, os.path.relpath(p, root)


def main(argv):
    if len(argv) != 2:
        sys.exit(__doc__)
    sys.stdout.reconfigure(encoding="latin-1", newline="\n")  # names are raw Mac bytes, printed back as-is
    total = 0
    for path, rel in files(argv[1]):
        lines, fails = dump(path, rel)
        print("\n".join(lines))
        total += fails
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
