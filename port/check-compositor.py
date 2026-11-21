#!/usr/bin/env python3
"""P3 compositor check (issue #36): runs the game's placeholder screen under xvfb-run and checks
screenshots. Boot.cp paints a QuickDraw pattern into Pomme's 640x480 port, then fades it to black
and back at frame 10 (fade frames 11-26 to black, 27-42 back in).

Usage: port/check-compositor.py BINARY GAME_ROOT     (GAME_ROOT from port/stage-data.py)
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image

BACKGROUND = (32, 64, 128)
RED, GREEN, WHITE, BLACK = (255, 0, 0), (0, 255, 0), (255, 255, 255), (0, 0, 0)


def shot(binary, root, frame, size, out):
    env = dict(os.environ, WATERRACE_SCREENSHOT=out, WATERRACE_SCREENSHOT_FRAME=str(frame),
               WATERRACE_WINDOW_SIZE=size)
    subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", os.path.abspath(binary)],
                   cwd=root, env=env, check=True, timeout=120)
    return Image.open(out).convert("RGB")


def expect(img, name, points):
    bad = [(p, c, img.getpixel(p)) for p, c in points if img.getpixel(p) != c]
    for p, want, got in bad:
        print(f"FAIL {name}: pixel {p} is {got}, expected {want}")
    if not bad:
        print(f"ok   {name}")
    return not bad


def pattern_points(w, h):
    """Pixel probes (window coords, y down) for the pattern letterboxed into a w x h window."""
    s = min(w / 640, h / 480)
    ox, oy = (w - 640 * s) / 2, (h - 480 * s) / 2

    def at(x, y):
        return (int(ox + x * s), int(oy + y * s))

    points = [(at(20, 20), RED), (at(620, 460), GREEN), (at(320, 240), WHITE),
              (at(2, 2), RED), (at(637, 477), GREEN), (at(42, 20), BACKGROUND),
              (at(320, 100), BACKGROUND), (at(100, 470), BACKGROUND)]
    if ox >= 1:  # pillarbox bars
        points += [((0, h // 2), BLACK), ((int(ox) - 1, 10), BLACK), ((w - 1, h - 1), BLACK)]
    return points


def main():
    binary, root = sys.argv[1:3]
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        for size in ("640x480", "1280x720"):
            w, h = map(int, size.split("x"))
            img = shot(binary, root, 5, size, os.path.join(tmp, "pattern.png"))
            if img.size != (w, h):
                print(f"FAIL window is {img.size}, expected {size}")
                ok = False
                continue
            ok &= expect(img, f"pattern at {size}", pattern_points(w, h))

        img = shot(binary, root, 26, "1280x720", os.path.join(tmp, "black.png"))
        colors = img.getcolors(1 << 24)
        black = colors == [(1280 * 720, BLACK)]
        print(("ok  " if black else "FAIL") + f" end of fade to black: {len(colors)} distinct colours")
        ok &= black

        img = shot(binary, root, 18, "640x480", os.path.join(tmp, "mid.png"))
        r, g, b = img.getpixel((320, 100))
        mid = 0 < b < 128 and (r, g, b) != BACKGROUND
        print(("ok  " if mid else "FAIL") + f" mid-fade background is darker: {(r, g, b)}")
        ok &= mid

        img = shot(binary, root, 50, "640x480", os.path.join(tmp, "back.png"))
        ok &= expect(img, "pattern after fade back in", pattern_points(640, 480))

    print("PASS" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
