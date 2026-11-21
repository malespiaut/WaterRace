#!/usr/bin/env python3
"""P3 Practice Zone fly-through check (issue #47): runs the game under xvfb-run with WATERRACE_FLYTHROUGH=1 and
checks that Demo_PracticeFlyThrough flies the whole 'flth' camera path (30 s) without error. Then captures the path
at fixed times (WATERRACE_SCREENSHOT_TIME, 1/100 s) in a 640x480 and a 1280x720 window and checks each frame is
textured (enough distinct colours, little black) and shows the sea. The widescreen frame must be the 4:3 frame
with more on the sides (4:3 vertical FOV kept, PORTING.md 6.3): its centre 4:3 crop must match the 4:3 frame.
Writes the frames and a contact sheet (flythrough-sheet.png) to OUTDIR, to compare by eye with the references of #34.

Usage: port/check-flythrough.py BINARY GAME_ROOT [OUTDIR]     (GAME_ROOT from port/stage-data.py)
"""
import os
import subprocess
import sys

from PIL import Image, ImageChops, ImageDraw, ImageStat

TIMES = [100, 500, 900, 1300, 1700, 2100, 2500, 2850]	# the path is 3000 ticks long
MIN_COLOURS = 2000	# distinct colours in a frame: textured, not flat-shaded
MAX_BLACK = 0.1	# fraction of near-black pixels (the cave at 21 s is dark)
MIN_SEA = 0.03	# fraction of sea- or sky-coloured (blue/cyan) pixels
MAX_WIDE_DIFF = 12	# mean abs difference (0-255) between the 4:3 frame and the widescreen frame's centre


def run(binary, root, size, env, timeout=300):
    env = dict(os.environ, WATERRACE_FLYTHROUGH="1", WATERRACE_WINDOW_SIZE=size, **env)
    return subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", os.path.abspath(binary)],
                          cwd=root, env=env, capture_output=True, text=True, timeout=timeout)


def shot(binary, root, size, t, path):
    r = run(binary, root, size, {"WATERRACE_SCREENSHOT": path, "WATERRACE_SCREENSHOT_TIME": str(t)})
    if r.returncode != 0 or not os.path.exists(path):
        sys.stderr.write(r.stderr)
        return None
    return Image.open(path).convert("RGB")


def frame_ok(img):
    data = img.tobytes()
    pixels = list(zip(data[0::3], data[1::3], data[2::3]))
    colours = len(set(pixels))
    black = sum(1 for r, g, b in pixels if r + g + b < 30) / len(pixels)
    sea = sum(1 for r, g, b in pixels if b > r + 20 and g > r) / len(pixels)
    return colours >= MIN_COLOURS and black <= MAX_BLACK and sea >= MIN_SEA, f"{colours} colours, black {black:.3f}, sea {sea:.3f}"


def main():
    binary, root = sys.argv[1:3]
    out = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else ".")
    os.makedirs(out, exist_ok=True)
    ok = True

    fly = run(binary, root, "640x480", {})
    if "Boot: Demo_PracticeFlyThrough done (0)" not in fly.stderr or "WaterRace port: unimplemented" in fly.stderr or fly.returncode != 0:
        sys.stderr.write(fly.stderr)
        print(f"FAIL fly-through did not run to its end (exit {fly.returncode})")
        ok = False
    else:
        print("ok   fly-through ran to its end")

    frames = []
    for t in TIMES:
        narrow = shot(binary, root, "640x480", t, os.path.join(out, f"fly-{t:04d}-640x480.png"))
        wide = shot(binary, root, "1280x720", t, os.path.join(out, f"fly-{t:04d}-1280x720.png"))
        if narrow is None or wide is None:
            print(f"FAIL t={t}: no screenshot")
            ok = False
            continue
        frames.append((t, narrow, wide))
        for name, img in (("640x480", narrow), ("1280x720", wide)):
            good, info = frame_ok(img)
            print(f"{'ok  ' if good else 'FAIL'} t={t} {name}: {info}")
            ok &= good
        centre = wide.crop((160, 0, 1120, 720)).resize((640, 480), Image.BILINEAR)
        diff = sum(ImageStat.Stat(ImageChops.difference(centre, narrow)).mean) / 3
        print(f"{'ok  ' if diff <= MAX_WIDE_DIFF else 'FAIL'} t={t} widescreen centre vs 4:3: mean diff {diff:.1f}")
        ok &= diff <= MAX_WIDE_DIFF

    if frames:
        sheet = Image.new("RGB", (320 + 427, len(frames) * 250), "white")
        draw = ImageDraw.Draw(sheet)
        for i, (t, narrow, wide) in enumerate(frames):
            sheet.paste(narrow.resize((320, 240)), (0, i * 250))
            sheet.paste(wide.resize((427, 240)), (320, i * 250))
            draw.text((4, i * 250 + 239), f"t = {t / 100:.2f} s", fill="black")
        sheet.save(os.path.join(out, "flythrough-sheet.png"))
        print(f"     contact sheet {os.path.join(out, 'flythrough-sheet.png')}")

    print("fly-through check passed" if ok else "fly-through check FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
