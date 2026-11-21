#!/usr/bin/env python3
"""Convert every PICT in a raw resource fork (Interface.rsrc_fork menu artwork) to PNG (see PORTING.md 5.3).

Usage: convert_interface.py [--argb] FORK.rsrc_fork OUT_DIR [SHEET.png]
Writes OUT_DIR/<id>.png and merges them into OUT_DIR/manifest.json ([{id, name, width, height, png}]); with SHEET.png also a
contact sheet of original | converted, the original rendered without pict2png (ImageMagick/ffmpeg, else Pomme via
$POMME_PICT_REF, see tools/pomme_pict_ref.cpp); cells whose mean RGB difference exceeds TOL are outlined red,
cells no reference decoder reads are labelled grey. Failures and mismatches go to stderr, exit 1.
--argb (app-fork textures: Post FX, World.cp turbo/joker/wake, Texture_NewARGB32TextureFromPictResource): keep the PICT's alpha plane; each sheet image is
shown as RGB | alpha over a checkerboard, and the reference is Pomme's RGBA decode when $POMME_PICT_REF is set.
"""
import json
import os
import struct
import sys

from PIL import Image, ImageChops, ImageDraw, ImageStat

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pict2png  # noqa: E402

TOL = 8  # mean per-channel RGB difference: ImageMagick expands 5-bit channels as v<<3, pict2png as (v<<3)|(v>>2), up to 7 apart


def picts(fork):
    """Yield (id, name, data) for every PICT resource of a raw resource fork."""
    do, mo, _dl, ml = struct.unpack(">4I", fork[:16])
    m = fork[mo:mo + ml]
    to, no = struct.unpack(">HH", m[24:28])
    (n,) = struct.unpack(">H", m[to:to + 2])
    for i in range(n + 1):
        t, k, ro = struct.unpack(">4sHH", m[to + 2 + i * 8:to + 10 + i * 8])
        if t != b"PICT":
            continue
        for j in range(k + 1):
            rid, nameoff, ao = struct.unpack(">hHI", m[to + ro + j * 12:to + ro + j * 12 + 8])
            name = "" if nameoff == 0xFFFF else m[no + nameoff + 1:no + nameoff + 1 + m[no + nameoff]].decode("mac_roman")
            o = do + (ao & 0xFFFFFF)
            (ln,) = struct.unpack(">I", fork[o:o + 4])
            yield rid, name, fork[o + 4:o + 4 + ln]


def main(argv):
    argb = argv[1:2] == ["--argb"]
    if argb:
        argv = argv[:1] + argv[2:]
    if len(argv) not in (3, 4):
        print(__doc__, file=sys.stderr)
        return 2
    fork, out = open(argv[1], "rb").read(), argv[2]
    os.makedirs(out, exist_ok=True)
    manifest, fails, cells, unread = [], 0, [], []
    for rid, name, d in sorted(picts(fork)):
        try:
            img = pict2png.decode(d)
            if not argb:  # DrawPicture ignores the alpha plane of 32-bit PICTs: keep the artwork opaque
                img = img.convert("RGB")
            elif img.getchannel("A").getextrema() == (255, 255):
                # No alpha plane (the QuickTime JPEG lens flares, the 3-component wake 250): the GWorld's x byte stayed 0,
                # kQABlend_PreMultiply (LensFlare.cp, Drawing.cp wake) adds them
                # shortcut: a 4-channel PICT with a fully opaque alpha plane would also get 0; none of the app-fork PICTs has one
                img.putalpha(0)
        except Exception as ex:  # noqa: BLE001 - log every outlier, keep going
            fails += 1
            print(f"FAIL PICT {rid} {name!r}: {ex}", file=sys.stderr)
            continue
        img.save(os.path.join(out, f"{rid}.png"))
        manifest.append(dict(id=rid, name=name, width=img.width, height=img.height, png=f"{rid}.png"))
        if len(argv) == 4:
            import texture_sheets as ts  # noqa: E402 - only for the optional sheet
            # ImageMagick rejects the full-plane clip rect these PICTs carry: clip to the picture frame instead
            orig = argb and ts.pomme_render(d, alpha=True) or ts.im_render(d.replace(b"\x00\x01\x00\x0a\x80\x01\x80\x01\x7f\xff\x7f\xff", b"\x00\x01\x00\x0a" + d[2:10])) \
                or ts.pomme_render(d)
            ref = img if orig is None or orig.mode == img.mode else img.convert(orig.mode)  # a JPEG reference has no alpha to compare
            diff = orig is not None and (orig.size != img.size or max(ImageStat.Stat(ImageChops.difference(orig, ref)).mean) > TOL)
            if orig is None:
                unread.append(rid)
            if diff:
                fails += 1
                print(f"MISMATCH PICT {rid} {name!r}", file=sys.stderr)
            cells.append((f"{rid} {name} {img.width}x{img.height}", orig, img, diff))
    path = os.path.join(out, "manifest.json")
    ids = {e["id"] for e in manifest}  # several forks share one folder: replace this fork's ids, keep the others
    kept = [e for e in json.load(open(path)) if e["id"] not in ids] if os.path.exists(path) else []
    with open(path, "w") as f:
        json.dump(sorted(manifest + kept, key=lambda e: e["id"]), f, indent=1)
    if cells:
        cw, cols = 320, 4
        rows = (len(cells) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * (2 * cw + 8), rows * (cw * 3 // 4 + 14)), (40, 40, 40))
        dr = ImageDraw.Draw(sheet)
        for i, (label, orig, img, diff) in enumerate(cells):
            x, y = (i % cols) * (2 * cw + 8), (i // cols) * (cw * 3 // 4 + 14)
            for k, im in enumerate((orig, img)):
                if im is None:  # no reference decoder reads it: review the converted image by eye
                    continue
                im = im.copy()
                if argb:  # RGB | alpha over a checkerboard, side by side
                    over = ts.checker(im.size)
                    if "A" in im.getbands():
                        over.paste(im.convert("RGB"), mask=im.getchannel("A"))
                    pair = Image.new("RGB", (im.width * 2 + 2, im.height), (40, 40, 40))
                    pair.paste(im.convert("RGB"))
                    pair.paste(over, (im.width + 2, 0))
                    im = pair
                im.thumbnail((cw, cw * 3 // 4))
                sheet.paste(im, (x + k * cw, y + 14))
            dr.text((x + 2, y + 1), label, fill=(255, 80, 80) if diff else (255, 255, 255) if orig else (150, 150, 150))
            if diff:
                dr.rectangle([x, y, x + 2 * cw - 1, y + 14 + cw * 3 // 4 - 1], outline=(255, 0, 0))
        sheet.save(argv[3])
    print(f"{len(manifest)} converted, {fails} failed or mismatched" + (f", unread by reference: {unread}" if cells else ""))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
