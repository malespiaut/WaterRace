#!/usr/bin/env python3
"""P3 FT logo check (issue #38): runs the game under xvfb-run and screenshots the last frame of
Demo_Play's fade-in over the FT logo (PICT 128, drawn from "Interface PNG/128.png"). In a 640x480
window the logo's rect must be pixel-identical to the PNG.

Frame 62: Screen_Init's fade to black and Demo_Play's fade back each present 2 * kGammaSpeed + 1 = 31 frames.

Usage: port/check-ft-logo.py BINARY GAME_ROOT     (GAME_ROOT from port/stage-data.py)
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageChops

FRAME = 62


def main():
    binary, root = sys.argv[1:3]
    logo = Image.open(os.path.join(root, "WaterRace Data", "Interface PNG", "128.png")).convert("RGB")
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "logo.png")
        env = dict(os.environ, WATERRACE_SCREENSHOT=out, WATERRACE_SCREENSHOT_FRAME=str(FRAME),
                   WATERRACE_WINDOW_SIZE="640x480")
        subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", os.path.abspath(binary)],
                       cwd=root, env=env, check=True, timeout=120)
        shot = Image.open(out).convert("RGB")

    # Demo_Play centres the picture on the 640x480 interface.
    left, top = (640 - logo.width) // 2, (480 - logo.height) // 2
    diff = ImageChops.difference(shot.crop((left, top, left + logo.width, top + logo.height)), logo).getbbox()
    if shot.size != (640, 480) or diff is not None:
        print(f"FAIL FT logo: screenshot {shot.size}, differing box {diff}")
        return 1
    print("ok   FT logo pixel-identical to Interface PNG/128.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
