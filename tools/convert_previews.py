#!/usr/bin/env python3
"""Convert the ship and location previews ('imag' Enbl/Dsbl next to a 'shds' or 'trds' data subtag) to PNG (PORTING.md 5.3).

Usage: convert_previews.py DATA_DIR OUT_DIR
Writes OUT_DIR/<metafile path>/Enbl.png and Dsbl.png (opaque RGB: DrawPicture ignores the alpha plane), as
Data Files.cp loads them. Some previews are QuickTime JPEG, which Pomme can't decode. Failures go to stderr, exit 1.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_textures  # noqa: E402
import pict2png  # noqa: E402

IDS = (b"Enbl", b"Dsbl")  # kEnablePreviewID, kDisablePreviewID


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    data, out = argv[1:]
    done, fails = 0, 0
    for root, _dirs, files in sorted(os.walk(data)):
        for fn in sorted(files):
            if fn.endswith(".rsrc_fork"):
                continue
            path = os.path.join(root, fn)
            rel = os.path.relpath(path, data)
            with open(path, "rb") as f:
                d = f.read()
            try:
                entries = convert_textures.metafile_entries(d)
            except struct.error:
                entries = None
            if entries is None:
                continue
            data_subs = {(e[4], e[5]): e for e in entries if e[3] == b"data"}
            if not any(sub in (b"shds", b"trds") for sub, _id in data_subs):
                continue
            for pid in IDS:
                e = data_subs.get((b"imag", pid))
                try:
                    if e is None:
                        raise ValueError("no imag tag")
                    pict = d[e[7]:e[7] + e[6]]
                    img = pict2png.decode(pict).convert("RGB")
                    top, left, bottom, right = struct.unpack(">hhhh", pict[2:10])
                    if img.size != (right - left, bottom - top):
                        raise ValueError(f"size {img.size} != picFrame {(right - left, bottom - top)}")
                    os.makedirs(os.path.join(out, rel), exist_ok=True)
                    img.save(os.path.join(out, rel, pid.decode() + ".png"))
                    done += 1
                except Exception as ex:  # noqa: BLE001 - log every outlier, keep going
                    fails += 1
                    print(f"FAIL {rel} {pid.decode()}: {ex}", file=sys.stderr)
    print(f"{done} previews converted, {fails} failed")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
