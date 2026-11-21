#!/usr/bin/env python3
"""Convert every cptx texture in the MetaFiles under a data dir to RGBA PNG (see PORTING.md 5.2, 5.3).

Usage: convert_textures.py DATA_DIR OUT_DIR
Writes OUT_DIR/<metafile path>/<name>.png and OUT_DIR/manifest.json; failures go to stderr, exit 1.
"""
import json
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pict2png  # noqa: E402

HDR = 28  # CompressedTexture header incl. padding; the alpha plane (w*h bytes) follows it


def metafile_entries(d):
    """Big-endian MetaFile: header {ver,flags,creator,tablePos,tableSize}, table {count, 24-byte entries}."""
    if len(d) < 16 or d[:2] != b"\x01\x04":
        return None
    pos, _size = struct.unpack(">II", d[8:16])
    (n,) = struct.unpack(">I", d[pos:pos + 4])
    return [struct.unpack(">BBh4s4s4sII", d[pos + 4 + i * 24:pos + 28 + i * 24]) for i in range(n)]


def textures(entries):
    """Yield (cptx entry, next entry); the PICT is the next entry (MetaFile Read.cp:227)."""
    for i, e in enumerate(entries):
        if e[3] == b"cptx":
            yield e, entries[i + 1] if i + 1 < len(entries) else None


def convert(d, e, img_e):
    p = e[7]
    name = d[p:p + 4].decode("mac_roman")
    flags, w, h, depth = struct.unpack(">IHHH", d[p + 4:p + 14])
    (alpha_depth,) = struct.unpack(">h", d[p + 24:p + 26])
    if alpha_depth not in (0, 1, 8):
        raise ValueError(f"alphaDepth {alpha_depth}")
    if img_e is None or img_e[4] != b"txim":
        raise ValueError("no txim entry after cptx")
    img = pict2png.decode(d[img_e[7]:img_e[7] + img_e[6]])
    if img.size != (w, h):
        raise ValueError(f"PICT size {img.size} != header {(w, h)}")
    if alpha_depth:
        if e[6] != HDR + w * h:
            raise ValueError(f"payload {e[6]} != {HDR + w * h}")
        a = d[p + HDR:p + HDR + w * h]
        if alpha_depth == 1:  # 1-bit: stored as bytes, thresholded like Textures.cp (kMaskLimit)
            a = bytes(255 if v > 127 else 0 for v in a)
        img.putalpha(Image.frombytes("L", (w, h), a))
    return name, flags, w, h, depth, alpha_depth, img


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    data, out = argv[1:]
    manifest, fails, used = [], 0, set()
    for root, _dirs, files in sorted(os.walk(data)):
        for fn in sorted(files):
            if fn.endswith(".rsrc_fork"):
                continue
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, data)
            with open(path, "rb") as f:
                d = f.read()
            try:
                entries = metafile_entries(d)
            except struct.error:
                entries = None
            if entries is None:
                continue
            for e, img_e in textures(entries):
                try:
                    name, flags, w, h, depth, ad, img = convert(d, e, img_e)
                    odir = os.path.join(out, rel)
                    os.makedirs(odir, exist_ok=True)
                    png = f"{rel}/{name.replace('/', '_').replace(chr(0), '')}.png"  # NUL pads short names
                    while png in used:  # repeated names inside one MetaFile: keep each, suffix the later ones
                        png = png[:-4] + "_dup.png"
                    used.add(png)
                    img.save(os.path.join(out, png))
                    manifest.append(dict(name=name, flags=flags, width=w, height=h, depth=depth,
                                         alphaDepth=ad, source=rel, png=png))
                except Exception as ex:  # noqa: BLE001 - log every outlier, keep going
                    fails += 1
                    print(f"FAIL {rel} {e[5]!r}: {ex}", file=sys.stderr)
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
    cnt = {k: sum(m["alphaDepth"] == k for m in manifest) for k in (0, 1, 8)}
    print(f"{len(manifest)} converted, {fails} failed, alphaDepth counts {cnt}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
