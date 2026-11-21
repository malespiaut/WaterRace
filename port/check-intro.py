#!/usr/bin/env python3
"""P3 logo intro check (issue #44): runs the game under xvfb-run with WATERRACE_INTRO_ONLY=1 and checks that
Demo_Play plays the Logo Intro to its end (no error, no link-stub abort, clean exit). Then captures the
intro at fixed script times (WATERRACE_SCREENSHOT_TIME, 1/100 s) and checks each frame: the 640x360
letterboxed context shows the model (enough lit, coloured pixels), the bars above and below stay black.
Writes the frames and a contact sheet (intro-sheet.png) to OUTDIR, to compare by eye with the
reference frames of #34.

Usage: port/check-intro.py BINARY GAME_ROOT [OUTDIR]     (GAME_ROOT from port/stage-data.py)
"""
import os
import subprocess
import sys

from PIL import Image, ImageDraw

TIMES = [50, 150, 250, 350, 450, 550, 650, 750, 849]	# the script is 850 ticks long
TOP, BOTTOM = 60, 420	# Demo.cp: kMovieVOffset, kMovieHeight
MIN_LIT = 0.004	# fraction of the context with a pixel brighter than 48


def run(binary, root, env, timeout=300):
    env = dict(os.environ, WATERRACE_INTRO_ONLY="1", WATERRACE_WINDOW_SIZE="640x480", **env)
    return subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", os.path.abspath(binary)],
                          cwd=root, env=env, capture_output=True, text=True, timeout=timeout)


def main():
    binary, root = sys.argv[1:3]
    out = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else ".")
    os.makedirs(out, exist_ok=True)
    ok = True

    play = run(binary, root, {})
    if "Boot: Demo_Play done (0)" not in play.stderr or "WaterRace port: unimplemented" in play.stderr or play.returncode != 0:
        sys.stderr.write(play.stderr)
        print(f"FAIL intro did not play to its end (exit {play.returncode})")
        ok = False
    else:
        print("ok   intro played to its end")

    frames = []
    for t in TIMES:
        path = os.path.join(out, f"intro-{t:03d}.png")
        shot = run(binary, root, {"WATERRACE_SCREENSHOT": path, "WATERRACE_SCREENSHOT_TIME": str(t)})
        if shot.returncode != 0 or not os.path.exists(path):
            sys.stderr.write(shot.stderr)
            print(f"FAIL t={t}: no screenshot (exit {shot.returncode})")
            ok = False
            continue
        img = Image.open(path).convert("RGB")
        frames.append((t, img))
        gray = img.convert("L")
        context = gray.crop((0, TOP, 640, BOTTOM))
        lit = sum(context.histogram()[49:]) / (640 * (BOTTOM - TOP))
        bars = max(gray.crop((0, 0, 640, TOP)).getextrema()[1], gray.crop((0, BOTTOM, 640, 480)).getextrema()[1])
        if img.size != (640, 480) or lit < MIN_LIT or bars > 0:
            print(f"FAIL t={t}: size {img.size}, lit {lit:.3f}, bar max {bars}")
            ok = False
        else:
            print(f"ok   t={t}: lit {lit:.3f}")

    if frames:
        sheet = Image.new("RGB", (3 * 320, ((len(frames) + 2) // 3) * 250), "white")
        draw = ImageDraw.Draw(sheet)
        for i, (t, img) in enumerate(frames):
            x, y = (i % 3) * 320, (i // 3) * 250
            sheet.paste(img.resize((320, 240)), (x, y))
            draw.text((x + 4, y + 239), f"t = {t / 100:.2f} s", fill="black")
        sheet.save(os.path.join(out, "intro-sheet.png"))
        print(f"     contact sheet {os.path.join(out, 'intro-sheet.png')}")

    print("intro check passed" if ok else "intro check FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
