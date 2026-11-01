#!/usr/bin/env python3
"""Stage the game's data for the Linux port (P1, PORTING.md §5.4).

Pomme reads resource forks only from AppleDouble files named NAME.rsrc (magic 0x00051607).
The repo keeps the same forks as raw NAME.rsrc_fork files, so this script wraps each one
in a minimal AppleDouble header and writes the result next to the data fork.

The English app's resource fork (STR# 128-131, 200-304, ...) is not in the repo. Pass the
AppleDouble copy from the 1.0.1 CD as --app-rsrc.

Usage: port/stage-data.py OUTDIR [--app-rsrc "PATH/WaterRace 1.0.1 [English].rsrc"]

OUTDIR becomes the game's root (Pomme's volume 0). It contains "WaterRace Data/" and the app fork.
Run the binary from OUTDIR.
"""
import argparse
import os
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_DATA = os.path.join(ROOT, "WaterRace Data")
APP_NAME = "WaterRace 1.0.1 [English]"
ADF_MAGIC = 0x00051607
ADF_VERSION = 0x00020000
RSRC_ENTRY = 2


def appledouble(resource_fork: bytes) -> bytes:
    """AppleDouble v2 with one entry (resource fork). Pomme reads only this entry."""
    header_len = 26 + 12  # fixed header + one descriptor
    hdr = struct.pack(">II", ADF_MAGIC, ADF_VERSION) + b"\0" * 16
    hdr += struct.pack(">H", 1)
    hdr += struct.pack(">III", RSRC_ENTRY, header_len, len(resource_fork))
    assert len(hdr) == header_len
    return hdr + resource_fork


def read_appledouble_fork(path: str) -> bytes:
    """Return the resource fork stored in an AppleDouble file (entry 2)."""
    b = open(path, "rb").read()
    magic = struct.unpack(">I", b[:4])[0]
    if magic != ADF_MAGIC:
        raise ValueError(f"{path}: not an AppleDouble file")
    n = struct.unpack(">H", b[24:26])[0]
    for i in range(n):
        eid, off, ln = struct.unpack(">III", b[26 + 12 * i:38 + 12 * i])
        if eid == RSRC_ENTRY:
            return b[off:off + ln]
    raise ValueError(f"{path}: no resource fork entry")


def stage(outdir: str, app_rsrc: str | None) -> int:
    if not os.path.isdir(REPO_DATA):
        sys.exit(f"missing {REPO_DATA}")
    dest_data = os.path.join(outdir, "WaterRace Data")
    if os.path.exists(dest_data):
        shutil.rmtree(dest_data)
    shutil.copytree(REPO_DATA, dest_data)

    wrapped = 0
    for dirpath, _, files in os.walk(dest_data):
        for name in files:
            if not name.endswith(".rsrc_fork"):
                continue
            raw_path = os.path.join(dirpath, name)
            base = raw_path[: -len(".rsrc_fork")]
            fork = open(raw_path, "rb").read()
            with open(base + ".rsrc", "wb") as f:
                f.write(appledouble(fork))
            os.remove(raw_path)
            wrapped += 1

    if app_rsrc:
        fork = read_appledouble_fork(app_rsrc)  # validates the input
        with open(os.path.join(outdir, APP_NAME + ".rsrc"), "wb") as f:
            f.write(appledouble(fork))
    print(f"staged {dest_data} ({wrapped} resource forks wrapped)")
    if not app_rsrc:
        print(f"warning: no --app-rsrc given; {APP_NAME}.rsrc is missing", file=sys.stderr)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("outdir")
    ap.add_argument("--app-rsrc", help="AppleDouble copy of the English app's resource fork")
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)
    return stage(args.outdir, args.app_rsrc)


if __name__ == "__main__":
    sys.exit(main())
