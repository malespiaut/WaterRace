#!/usr/bin/env python3
"""P3 main interface check (issue #48): under xvfb-run the game plays the logo intro and reaches the main
interface (MainMenu_Run logs "Interface: main menu ready"). Then:

1. 640x480 window: a SIGUSR1 snapshot (WATERRACE_SNAPSHOT, glReadPixels) must show the background
   (PICT 200) around Leah's context and the slid-in buttons (PICT 260 at 0,100) pixel for pixel, and Leah
   (lit pixels in her context). The mouse then hovers Quit: a second snapshot must show its "over"
   picture (PICT 211) at the Btns 128 position. A click on Quit must exit cleanly (exit 0, no link stub).
2. 1280x720 window (letterboxed): a click on Quit, in window coordinates, must exit cleanly too.

Writes menu.png and menu-quit-over.png to OUTDIR.

Usage: port/check-main-menu.py BINARY GAME_ROOT [OUTDIR]     (GAME_ROOT from port/stage-data.py)
"""
import os
import signal
import subprocess
import sys
import time

from PIL import Image, ImageChops

LEAH = (288, 50, 640, 480)				# Main Menu.cp: kLeahPosH, kLeahPosV, to the interface's corner
BUTTONS = (0, 100, 205, 433)			# PICT 260 (205x333) at kMainButtonOffsetH, kMainButtonOffsetV
QUIT = (0, 403, 158, 433)				# Btns 128, button 5 (kMainButton_Quit): hotZone = where + PICT 211
MIN_LIT = 0.05							# fraction of Leah's context that differs from the background


def wait_for(proc, log, text, timeout):
	end = time.time() + timeout
	while time.time() < end and proc.poll() is None:
		with open(log) as f:
			if text in f.read():
				return True
		time.sleep(0.5)
	return False


def snapshot(proc, log, path):
	if os.path.exists(path):
		os.remove(path)
	with open(log) as f:
		count = f.read().count("Boot: snapshot")
	proc.send_signal(signal.SIGUSR1)
	end = time.time() + 30
	while time.time() < end:
		with open(log) as f:
			if f.read().count("Boot: snapshot") > count:
				return Image.open(path).convert("RGB")
		time.sleep(0.2)
	return None


def same(shot, picture, box):
	return ImageChops.difference(shot.crop(box), picture.crop((0, 0, box[2] - box[0], box[3] - box[1]))).getbbox() is None


def xdotool(*args):
	subprocess.run(["xdotool", *args], check=True, env=os.environ)


def click_quit(window, x, y):
	# Pressed for a while: the menu polls the mouse once per presented frame
	xdotool("mousemove", "--window", window, str(x), str(y))
	time.sleep(1)
	xdotool("mousedown", "1")
	time.sleep(0.5)
	xdotool("mouseup", "1")


def run(binary, root, out, size, checks):
	log = os.path.join(out, f"menu-{size}.log")
	env = dict(os.environ, WATERRACE_WINDOW_SIZE=size, WATERRACE_SNAPSHOT=os.path.join(out, "snapshot.png"))
	with open(log, "w") as f:
		proc = subprocess.Popen([os.path.abspath(binary)], cwd=root, env=env, stdout=f, stderr=subprocess.STDOUT)
	ok = True
	try:
		if not wait_for(proc, log, "Interface: main menu ready", 240):
			print(f"FAIL {size}: main menu not reached (exit {proc.poll()}), see {log}")
			return False
		print(f"ok   {size}: logo intro -> main menu")
		time.sleep(3)	# Leah's first loop
		window = subprocess.run(["xdotool", "search", "--sync", "--name", "WaterRace"], capture_output=True,
								text=True, check=True).stdout.split()[0]
		ok = checks(proc, log, window)
		code = proc.wait(timeout=120)
		with open(log) as f:
			stub = "WaterRace port: unimplemented" in f.read()
		if code != 0 or stub:
			print(f"FAIL {size}: Quit exit {code}, link stub hit {stub}, see {log}")
			ok = False
		else:
			print(f"ok   {size}: click on Quit exits cleanly")
	finally:
		if proc.poll() is None:
			proc.kill()
			ok = False
			print(f"FAIL {size}: still running after Quit, see {log}")
	return ok


def main():
	if os.environ.get("CHECK_MAIN_MENU_INNER") is None:
		env = dict(os.environ, CHECK_MAIN_MENU_INNER="1")
		return subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", sys.executable, *sys.argv], env=env).returncode

	binary, root = sys.argv[1:3]
	out = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else ".")
	os.makedirs(out, exist_ok=True)
	pictures = os.path.join(root, "WaterRace Data", "Interface PNG")
	background = Image.open(os.path.join(pictures, "200.png")).convert("RGB")
	buttons = Image.open(os.path.join(pictures, "260.png")).convert("RGB")
	quit_over = Image.open(os.path.join(pictures, "211.png")).convert("RGB")

	def checks_640(proc, log, window):
		ok = True
		shot = snapshot(proc, log, os.path.join(out, "snapshot.png"))
		if shot is None:
			print("FAIL 640x480: no snapshot")
			return False
		shot.save(os.path.join(out, "menu.png"))
		masked = background.copy()
		masked.paste(shot.crop(LEAH), LEAH[:2])
		masked.paste(buttons, BUTTONS[:2])
		if shot.size != (640, 480) or not same(shot, masked, (0, 0, 640, 480)):
			print(f"FAIL 640x480: artwork differs from PICT 200 + 260, box {ImageChops.difference(shot, masked).getbbox()}")
			ok = False
		else:
			print("ok   640x480: background (PICT 200) and buttons (PICT 260) pixel-identical")
		diff = ImageChops.difference(shot.crop(LEAH), background.crop(LEAH)).convert("L")
		lit = sum(diff.histogram()[16:]) / ((LEAH[2] - LEAH[0]) * (LEAH[3] - LEAH[1]))
		if lit < MIN_LIT:
			print(f"FAIL 640x480: Leah not drawn ({lit:.3f} of her context)")
			ok = False
		else:
			print(f"ok   640x480: Leah drawn over the background ({lit:.3f} of her context)")

		xdotool("mousemove", "--window", window, str((QUIT[0] + QUIT[2]) // 2), str((QUIT[1] + QUIT[3]) // 2))
		time.sleep(2)
		shot = snapshot(proc, log, os.path.join(out, "snapshot.png"))
		if shot is None or not same(shot, quit_over, QUIT):
			print("FAIL 640x480: hovering Quit does not show PICT 211")
			ok = False
		else:
			shot.save(os.path.join(out, "menu-quit-over.png"))
			print("ok   640x480: hovering Quit shows its over picture (PICT 211)")
		click_quit(window, (QUIT[0] + QUIT[2]) // 2, (QUIT[1] + QUIT[3]) // 2)
		return ok

	def checks_720(proc, log, window):
		# 1280x720: scale 1.5, 160 px bars left and right
		click_quit(window, 160 + (QUIT[0] + QUIT[2]) * 3 // 4, (QUIT[1] + QUIT[3]) * 3 // 4)
		return True

	ok = run(binary, root, out, "640x480", checks_640)
	ok = run(binary, root, out, "1280x720", checks_720) and ok
	print(f"     snapshots in {out}")
	print("main menu check passed" if ok else "main menu check FAILED")
	return 0 if ok else 1


if __name__ == "__main__":
	sys.exit(main())
