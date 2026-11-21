#!/usr/bin/env python3
"""P3 New Pilot screen check (issue #64): under xvfb-run the game reaches the main interface, then:

1. A click on New Pilot opens the New Pilot screen (NewMenu_Run logs "Interface: new pilot ready").
2. A SIGUSR1 snapshot shows the panel (PICT 300 at 0,104) untouched except for three text lines drawn by the
   Text Engine on 32-bit pixels: the pilot name in FtDf 129 spec 1's colour (250,183,0) centred on x=194 at
   y=161, the difficulty and the mode in spec 2's colour (white) at y=191 and y=221.
3. A click on Cancel (Btns 129, button 2) returns to the main interface; a click on Quit exits cleanly.

The red "name already in use" colour needs saved pilots (folder scan and pilot files, P4/P6): test-text-engine
checks that colour on 32-bit pixels instead.

Writes new-pilot.png to OUTDIR.

Usage: port/check-new-pilot.py BINARY GAME_ROOT [OUTDIR]     (GAME_ROOT from port/stage-data.py)
"""
import importlib.util
import os
import subprocess
import sys
import time

from PIL import Image, ImageChops

spec = importlib.util.spec_from_file_location("menu", os.path.join(os.path.dirname(__file__), "check-main-menu.py"))
menu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(menu)

NEW_PILOT = (0, 100, 205, 155)			# Btns 128, button 0 (kMainButton_NewPilot)
CANCEL = (32, 303, 108, 321)			# Btns 129, button 1 (kNewButton_Back)
QUIT = menu.QUIT
PANEL = (0, 104, 288, 326)				# New menu.cp: PICT 300, kNewButtonTotalWidth x kNewButtonTotalHeight at 0,104
TEXT = [								# New menu.cp: NewMenu_DrawDisplay lines (centred on 194), colour of full ink
	("name", (130, 161, 258, 180), (250, 183, 0)),
	("difficulty", (130, 191, 258, 210), (255, 255, 255)),
	("mode", (130, 221, 258, 240), (255, 255, 255)),
]
MIN_INK = 10							# fully inked pixels per text line


def centre(box):
	return (box[0] + box[2]) // 2, (box[1] + box[3]) // 2


def count(log, text):
	with open(log) as f:
		return f.read().count(text)


def check_screen(shot, panel):
	ok = True
	masked = shot.crop(PANEL)
	for name, box, colour in TEXT:
		local = (box[0] - PANEL[0], box[1] - PANEL[1], box[2] - PANEL[0], box[3] - PANEL[1])
		ink = dict((c, n) for n, c in shot.crop(box).getcolors(1 << 16)).get(colour, 0)
		if ink < MIN_INK:
			print(f"FAIL 640x480: {name} not drawn in {colour} at {box} ({ink} pixels)")
			ok = False
		else:
			xs = [x for y in range(box[1], box[3]) for x in range(box[0], box[2]) if shot.getpixel((x, y)) == colour]
			print(f"ok   640x480: {name} drawn in {colour} ({ink} pixels, x {min(xs)}..{max(xs)})")
		masked.paste(panel.crop(local), local[:2])
	diff = ImageChops.difference(masked, panel.crop((0, 0, PANEL[2] - PANEL[0], PANEL[3] - PANEL[1]))).getbbox()
	if diff is not None:
		print(f"FAIL 640x480: panel differs from PICT 300 outside the text lines, box {diff}")
		ok = False
	else:
		print("ok   640x480: rest of the panel pixel-identical to PICT 300")
	return ok


def main():
	if os.environ.get("CHECK_NEW_PILOT_INNER") is None:
		env = dict(os.environ, CHECK_NEW_PILOT_INNER="1")
		return subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", sys.executable, *sys.argv], env=env).returncode

	binary, root = sys.argv[1:3]
	out = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else ".")
	os.makedirs(out, exist_ok=True)
	panel = Image.open(os.path.join(root, "WaterRace Data", "Interface PNG", "300.png")).convert("RGB")

	def checks(proc, log, window):
		menu.click_quit(window, *centre(NEW_PILOT))
		if not menu.wait_for(proc, log, "Interface: new pilot ready", 60):
			print(f"FAIL 640x480: New Pilot screen not reached (exit {proc.poll()}), see {log}")
			return False
		print("ok   640x480: click on New Pilot opens the New Pilot screen")
		shot = menu.snapshot(proc, log, os.path.join(out, "snapshot.png"))
		if shot is None:
			print("FAIL 640x480: no snapshot")
			return False
		shot.save(os.path.join(out, "new-pilot.png"))
		ok = check_screen(shot, panel)

		menu.click_quit(window, *centre(CANCEL))
		end = time.time() + 60
		while time.time() < end and count(log, "Interface: main menu ready") < 2 and proc.poll() is None:
			time.sleep(0.5)
		if count(log, "Interface: main menu ready") < 2:
			print(f"FAIL 640x480: Cancel does not return to the main interface, see {log}")
			return False
		print("ok   640x480: Cancel returns to the main interface")
		time.sleep(2)
		menu.click_quit(window, *centre(QUIT))
		return ok

	ok = menu.run(binary, root, out, "640x480", checks)
	print(f"     snapshot in {out}")
	print("new pilot check passed" if ok else "new pilot check FAILED")
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
