#!/usr/bin/env python3
"""Contact sheets of converted textures vs. the original PICT rendered independently by ImageMagick.

Usage: texture_sheets.py DATA_DIR SHEET_DIR
Textures ImageMagick/ffmpeg cannot read (32-bit planar RGB, packType 4) fall back to Pomme's PICT decoder when the
tools/pomme_pict_ref.cpp binary is named in $POMME_PICT_REF (build line in that file); Pomme is independent of pict2png.py.
Per MetaFile writes SHEET_DIR/<name>_<n>.png: each cell is original (ImageMagick) | converted PNG on a
checkerboard, labelled by name; cells whose mean RGB difference exceeds TOL from ImageMagick's are outlined red. Prints a
summary of RGB mismatches and of textures no independent decoder can read (the original has no alpha, so only RGB is compared) and exits 1 if any.
"""
import io
import os
import struct
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops, ImageDraw, ImageStat

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import convert_textures as ct  # noqa: E402

TOL = 5  # mean per-channel RGB difference tolerated (JPEG decoders/chroma upsampling and 5->8 bit expansion differ at edges)
CELL, COLS, ROWS = 128, 6, 8  # each cell shows two CELL-wide thumbnails


def im_render(pict):
    """Render a raw PICT blob without pict2png: ImageMagick, or ffmpeg on the embedded JPEG (IM lacks 0x8200)."""
    with tempfile.NamedTemporaryFile(suffix=".pct") as f:
        i = pict.find(b"\xff\xd8\xff")
        if i >= 0 and b"jpeg" in pict[:i]:
            f.write(pict[i:])
            cmd = ["ffmpeg", "-v", "error", "-f", "image2pipe", "-vcodec", "mjpeg", "-i", f.name,
                   "-f", "image2pipe", "-vcodec", "png", "-"]
        else:
            q = 40  # past header + 0x0C00; ImageMagick chokes on the 0xA1 long comments (ICC profile) that follow
            while pict[q:q + 2] == b"\x00\xa1":
                q += 6 + struct.unpack(">H", pict[q + 4:q + 6])[0]
            f.write(b"\0" * 512 + pict[:40] + pict[q:])
            cmd = ["magick", "pict:" + f.name, "png:-"]
        f.flush()
        r = subprocess.run(cmd, capture_output=True)
    return Image.open(io.BytesIO(r.stdout)).convert("RGB") if r.returncode == 0 else None


def pomme_render(pict):
    """Pomme's ReadPICT via tools/pomme_pict_ref.cpp ($POMME_PICT_REF): ARGB bytes, opaque here (the alpha plane is a separate cptx)."""
    exe = os.environ.get("POMME_PICT_REF")
    if not exe:
        return None
    # Pomme rejects the full-plane clip rect (-32767..32767) these PICTs carry: clip to the picture frame instead
    pict = pict.replace(b"\x00\x01\x00\x0a\x80\x01\x80\x01\x7f\xff\x7f\xff", b"\x00\x01\x00\x0a" + pict[2:10])
    r = subprocess.run([exe], input=pict, capture_output=True)
    if r.returncode:
        return None
    hdr, _, px = r.stdout.partition(b"\n")
    w, h = map(int, hdr.split())
    return Image.frombytes("RGBA", (w, h), px, "raw", "ARGB").convert("RGB")


def checker(size):
    bg = Image.new("RGB", size, (200, 200, 200))
    d = ImageDraw.Draw(bg)
    for y in range(0, size[1], 8):
        for x in range(0, size[0], 8):
            if (x // 8 + y // 8) % 2:
                d.rectangle([x, y, x + 7, y + 7], fill=(120, 120, 120))
    return bg


def thumb(img):
    img = img.copy()
    img.thumbnail((CELL, CELL - 14))
    return img


def main(argv):
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    data, out = argv[1:]
    os.makedirs(out, exist_ok=True)
    bad, unread, total = [], [], 0
    for root, _dirs, files in sorted(os.walk(data)):
        for fn in sorted(files):
            if fn.endswith(".rsrc_fork"):
                continue
            with open(os.path.join(root, fn), "rb") as f:
                d = f.read()
            try:
                entries = ct.metafile_entries(d)
            except Exception:  # noqa: BLE001 - not a MetaFile
                entries = None
            if entries is None:
                continue
            cells = []
            for e, ie in ct.textures(entries):
                name, _fl, w, h, _dp, ad, img = ct.convert(d, e, ie)
                pict = d[ie[7]:ie[7] + ie[6]]
                orig = im_render(pict) or pomme_render(pict)
                total += 1
                rel = os.path.relpath(os.path.join(root, fn), data)
                if orig is None:  # neither ImageMagick nor ffmpeg reads it: shown unchecked (grey label), review by eye only
                    unread.append(f"{rel}/{name}")
                    orig = img.convert("RGB")
                diff = orig.size != img.size or max(ImageStat.Stat(ImageChops.difference(orig, img.convert("RGB"))).mean) > TOL
                if diff:
                    bad.append(f"{rel}/{name}")
                bg = checker(img.size)
                bg.paste(img.convert("RGB"), mask=img.getchannel("A") if "A" in img.getbands() else None)
                cells.append((f"{name} a{ad} {w}x{h}", thumb(orig), thumb(bg), bool(diff)))
            per = COLS * ROWS
            for n in range(0, len(cells), per):
                sheet = Image.new("RGB", (COLS * (2 * CELL + 6), ROWS * CELL), (40, 40, 40))
                dr = ImageDraw.Draw(sheet)
                for i, (label, a, b, diff) in enumerate(cells[n:n + per]):
                    x, y = (i % COLS) * (2 * CELL + 6), (i // COLS) * CELL
                    sheet.paste(a, (x, y + 14))
                    sheet.paste(b, (x + CELL, y + 14))
                    dr.text((x + 2, y + 1), label, fill=(255, 80, 80) if diff else (255, 255, 255))
                    if diff:
                        dr.rectangle([x, y, x + 2 * CELL - 1, y + CELL - 1], outline=(255, 0, 0))
                sheet.save(os.path.join(out, f"{fn.replace(os.sep, '_')}_{n // per}.png"))
    print(f"{total} textures, {len(bad)} RGB mismatches vs independent decoder, {len(unread)} unreadable by it")
    for u in unread:
        print("UNREAD", u)
    for b in bad:
        print("MISMATCH", b)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
