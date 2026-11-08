#!/usr/bin/env python3
"""Boot StreakOS headless in QEMU and drive it via the monitor for screenshots.

usage: qemu_test.py ISO OUTDIR script.txt
script lines:  wait <ms> | shot <name> | key <qemu-key> | type <text> |
               move <dx> <dy> | click | down | up | wheel <n>
"""
import os, socket, subprocess, sys, time
from PIL import Image

iso, outdir, script = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(outdir, exist_ok=True)
sock = os.path.join(outdir, "mon.sock")
if os.path.exists(sock):
    os.remove(sock)
q = subprocess.Popen(["qemu-system-x86_64", "-cdrom", iso, "-m", "256M", "-vga", "std",
                      "-display", "none", "-serial", "file:" + os.path.join(outdir, "serial.log"),
                      "-monitor", "unix:%s,server,nowait" % sock, *(["-machine", "vmport=off"] if os.environ.get("VMPORT") != "1" else []), "-debugcon", "file:" + os.path.join(outdir, "debug.log")])
for _ in range(100):
    if os.path.exists(sock):
        break
    time.sleep(0.05)
s = socket.socket(socket.AF_UNIX)
s.connect(sock)
s.settimeout(0.3)

def mon(cmd, delay=0.05):
    s.sendall((cmd + "\n").encode())
    time.sleep(delay)
    try:
        while s.recv(65536):
            pass
    except socket.timeout:
        pass

KEYS = {" ": "spc", "\n": "ret", "-": "minus", "=": "equal", ".": "dot", "/": "slash", "*": "asterisk",
        "+": "shift-equal", "(": "shift-9", ")": "shift-0", "_": "shift-minus", ":": "shift-semicolon",
        ",": "comma", "'": "apostrophe", ";": "semicolon", "<": "shift-comma", ">": "shift-dot"}
mon("info status")
for line in open(script):
    p = line.split()
    if not p or p[0].startswith("#"):
        continue
    c = p[0]
    if c == "wait":
        time.sleep(int(p[1]) / 1000)
    elif c == "shot":
        ppm = os.path.join(outdir, p[1] + ".ppm")
        mon("screendump " + ppm)
        time.sleep(0.4)
        Image.open(ppm).save(os.path.join(outdir, p[1] + ".png"))
        os.remove(ppm)
    elif c == "key":
        mon("sendkey " + p[1])
    elif c == "type":
        for ch in line.split(" ", 1)[1].rstrip("\n"):
            k = KEYS.get(ch, ch.lower() if ch.isalnum() else None)
            if ch.isupper():
                k = "shift-" + ch.lower()
            if k:
                mon("sendkey " + k)
    elif c == "move":
        # small steps keep pointer acceleration out of the way (1 px = 1 px)
        dx, dy = int(p[1]), int(p[2])
        while dx or dy:
            sx = max(-3, min(3, dx)); sy = max(-3, min(3, dy))
            mon("mouse_move %d %d" % (sx, sy), 0.004)
            dx -= sx; dy -= sy
        time.sleep(0.05)
    elif c == "click":
        mon("mouse_button 1"); time.sleep(0.08); mon("mouse_button 0")
    elif c == "down":
        mon("mouse_button 1")
    elif c == "up":
        mon("mouse_button 0")
    elif c == "wheel":
        mon("mouse_move 0 0 %s" % p[1])
mon("quit")
q.wait(timeout=10)
