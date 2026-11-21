#!/usr/bin/env python3
"""P3 boot check (issue #37): runs the game under xvfb-run with WATERRACE_BOOT_ONLY=1, which quits
when main() reaches Demo_Play, and checks that every Boot_Sequence step ran in order, with no
link-stub (port_todo_fatal) abort and a clean exit.

Usage: port/check-boot.py BINARY GAME_ROOT     (GAME_ROOT from port/stage-data.py)
"""
import os
import subprocess
import sys

STEPS = ["Boot_Sequence", "Preference_Read", "Resolve_DataFolders", "Preload_DataFolders",
         "Open_DataFiles", "Screen_Init", "TextEngine_Init", "InfinityPlayer_Init",
         "Boot_Sequence done", "Demo_Play"]


def main():
    binary, root = sys.argv[1:3]
    env = dict(os.environ, WATERRACE_BOOT_ONLY="1")
    run = subprocess.run(["xvfb-run", "-a", "-s", "-screen 0 1920x1080x24", os.path.abspath(binary)],
                         cwd=root, env=env, capture_output=True, text=True, timeout=120)
    log = run.stderr
    sys.stderr.write(log)
    steps = [line[len("Boot: "):] for line in log.splitlines() if line.startswith("Boot: ")]
    ok = True
    if [s for s in steps if s in STEPS] != STEPS:
        print(f"FAIL boot steps: got {steps}, expected {STEPS} in order")
        ok = False
    if "WaterRace port: unimplemented" in log:
        print("FAIL port_todo_fatal abort")
        ok = False
    if run.returncode != 0:
        print(f"FAIL exit code {run.returncode}")
        ok = False
    print("ok   boot reached Demo_Play" if ok else "boot check FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
