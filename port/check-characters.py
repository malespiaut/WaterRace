#!/usr/bin/env python3
"""P3 character-select check (issue #49): under xvfb-run the game reaches the main interface, then:

1. New Pilot, a typed name and Return open the character-select screen (CharacterMenu_Run logs
   "Interface: character menu ready").
2. A SIGUSR1 snapshot shows PICT 4000 untouched outside the pilot panel and the character's context; the
   panel shows the current character's 'Enbl' preview and its neighbours' 'Dsbl' previews (decoded from the
   Characters/ MetaFiles with tools/pict2png.py); the character is drawn in its context, and a second
   snapshot shows it moved (intro animation playing).
3. A click on the right arrow loads the next character (log "Interface: character 1 loaded"), the left
   arrow key the first one again; each time the previews follow and the new character is drawn.
4. Escape returns to the main interface; a click on Quit exits cleanly (no link stub, exit 0).

Writes characters-0.png, characters-1.png and characters-back.png to OUTDIR.

Usage: port/check-characters.py BINARY GAME_ROOT [OUTDIR]     (GAME_ROOT from port/stage-data.py)
"""
import importlib.util
import os
import struct
import subprocess
import sys
import time

from PIL import Image, ImageChops


def load(name, path):
	spec = importlib.util.spec_from_file_location(name, os.path.join(os.path.dirname(__file__), path))
	module = importlib.util.module_from_spec(spec)
	spec.loader.exec_module(module)
	return module


menu = load("menu", "check-main-menu.py")
pict = load("pict2png", "../tools/pict2png.py")

NEW_PILOT = (0, 100, 205, 155)			# Btns 128, button 0
RIGHT = (256, 177, 281, 200)			# Btns 160, button 3 (kCharacterButton_Right)
QUIT = menu.QUIT
DISPLAY = (10, 80, 288, 366)			# Characters Menu.cp: kDisplay_*, redrawn per character
CHARACTER = (288, 50, 640, 480)			# kCharacterPosH, kCharacterPosV to the interface's corner
PREVIEWS = [("left", (44, 116), "Dsbl", -1), ("central", (117, 117), "Enbl", 0), ("right", (222, 116), "Dsbl", 1)]
MIN_LIT = 0.05							# fraction of the character's context that differs from PICT 4000
MAX_PREVIEW_DIFF = 8					# per channel: Pomme and pict2png may expand 16-bit pixels differently


def centre(box):
	return (box[0] + box[2]) // 2, (box[1] + box[3]) // 2


def count(log, text):
	with open(log) as f:
		return f.read().count(text)


def wait_count(proc, log, text, n, timeout):
	end = time.time() + timeout
	while time.time() < end and count(log, text) < n and proc.poll() is None:
		time.sleep(0.5)
	return count(log, text) >= n


def previews(folder):
	"""Enbl/Dsbl preview images of each character file, in the game's (case-insensitive) catalog order."""
	result = []
	for name in sorted(os.listdir(folder), key=str.lower):
		d = open(os.path.join(folder, name), "rb").read()
		pos = struct.unpack(">I", d[8:12])[0]
		images = {}
		for i in range(struct.unpack(">I", d[pos:pos + 4])[0]):
			_, _, _, tag, sub, tid, size, at = struct.unpack(">BBh4s4s4sII", d[pos + 4 + 24 * i:pos + 28 + 24 * i])
			if tag == b"data" and sub == b"imag":
				images[tid.decode()] = pict.decode(d[at:at + size]).convert("RGB")
		result.append((name, images))
	return result


def check_screen(shot, background, chars, num, label):
	ok = True
	masked = background.copy()
	masked.paste(shot.crop(DISPLAY), DISPLAY[:2])
	masked.paste(shot.crop(CHARACTER), CHARACTER[:2])
	diff = ImageChops.difference(shot, masked).getbbox()
	if diff is not None:
		print(f"FAIL {label}: artwork differs from PICT 4000 outside the panel and the character, box {diff}")
		ok = False
	else:
		print(f"ok   {label}: artwork pixel-identical to PICT 4000 outside the panel and the character")
	for where, (x, y), kind, offset in PREVIEWS:
		name, images = chars[(num + offset) % len(chars)]
		image = images[kind]
		box = (x, y, x + image.width, y + image.height)
		worst = max(high for _, high in ImageChops.difference(shot.crop(box), image).getextrema())
		if worst > MAX_PREVIEW_DIFF:
			print(f"FAIL {label}: {where} preview is not {name}'s {kind} (max channel difference {worst})")
			ok = False
		else:
			print(f"ok   {label}: {where} preview is {name}'s {kind}")
	lit = sum(ImageChops.difference(shot.crop(CHARACTER), background.crop(CHARACTER)).convert("L").histogram()[16:])
	lit /= (CHARACTER[2] - CHARACTER[0]) * (CHARACTER[3] - CHARACTER[1])
	if lit < MIN_LIT:
		print(f"FAIL {label}: character not drawn ({lit:.3f} of its context)")
		ok = False
	else:
		print(f"ok   {label}: {chars[num][0]} drawn in its context ({lit:.3f} of it)")
	return ok


def main():
	if os.environ.get("CHECK_CHARACTERS_INNER") is None:
		env = dict(os.environ, CHECK_CHARACTERS_INNER="1")
		return subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", sys.executable, *sys.argv], env=env).returncode

	binary, root = sys.argv[1:3]
	out = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else ".")
	os.makedirs(out, exist_ok=True)
	background = Image.open(os.path.join(root, "WaterRace Data", "Interface PNG", "4000.png")).convert("RGB")
	chars = previews(os.path.join(root, "WaterRace Data", "Characters"))
	snapshot = os.path.join(out, "snapshot.png")

	def checks(proc, log, window):
		menu.click_quit(window, *centre(NEW_PILOT))
		if not menu.wait_for(proc, log, "Interface: new pilot ready", 60):
			print(f"FAIL: New Pilot screen not reached (exit {proc.poll()}), see {log}")
			return False
		time.sleep(1)
		for key in ["t", "e", "s", "t", "Return"]:
			menu.xdotool("key", "--window", window, key)
			time.sleep(0.3)
		if not menu.wait_for(proc, log, "Interface: character menu ready", 120):
			print(f"FAIL: character-select screen not reached (exit {proc.poll()}), see {log}")
			return False
		print("ok   New Pilot -> character-select screen")
		time.sleep(3)
		first = menu.snapshot(proc, log, snapshot)
		time.sleep(1)
		second = menu.snapshot(proc, log, snapshot)
		if first is None or second is None:
			print("FAIL: no snapshot")
			return False
		first.save(os.path.join(out, "characters-0.png"))
		ok = check_screen(first, background, chars, 0, "character 0")
		if ImageChops.difference(first.crop(CHARACTER), second.crop(CHARACTER)).getbbox() is None:
			print("FAIL character 0: character not animated (same pixels 1 s apart)")
			ok = False
		else:
			print("ok   character 0: animated (pixels change 1 s apart)")

		menu.click_quit(window, *centre(RIGHT))
		if not wait_count(proc, log, "Interface: character 1 loaded", 1, 120):
			print(f"FAIL: right arrow does not load character 1 (exit {proc.poll()}), see {log}")
			return False
		time.sleep(3)
		shot = menu.snapshot(proc, log, snapshot)
		shot.save(os.path.join(out, "characters-1.png"))
		ok = check_screen(shot, background, chars, 1, "character 1 (right arrow)") and ok

		menu.xdotool("mousemove", "--window", window, *map(str, centre(DISPLAY)))	# off the buttons
		menu.xdotool("key", "--window", window, "Left")
		if not wait_count(proc, log, "Interface: character 0 loaded", 1, 120):
			print(f"FAIL: left arrow key does not load character 0 (exit {proc.poll()}), see {log}")
			return False
		time.sleep(3)
		shot = menu.snapshot(proc, log, snapshot)
		shot.save(os.path.join(out, "characters-back.png"))
		ok = check_screen(shot, background, chars, 0, "character 0 (left arrow key)") and ok

		menu.xdotool("key", "--window", window, "Escape")
		if not wait_count(proc, log, "Interface: main menu ready", 2, 120):
			print(f"FAIL: Escape does not return to the main interface, see {log}")
			return False
		print("ok   Escape returns to the main interface")
		time.sleep(2)
		menu.click_quit(window, *centre(QUIT))
		return ok

	ok = menu.run(binary, root, out, "640x480", checks)
	print(f"     snapshots in {out}")
	print("character-select check passed" if ok else "character-select check FAILED")
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
