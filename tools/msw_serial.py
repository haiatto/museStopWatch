"""Send console commands to the StopWatch over USB serial and save what it prints.

    .venv\\Scripts\\python.exe tools\\msw_serial.py COM5 --cmd ">msw.mem" --until "@msw.mem done" --secs 20 --out log.txt
    .venv\\Scripts\\python.exe tools\\msw_serial.py COM5 --secs 30            # just listen

Opens the port without touching DTR/RTS (no reset). Each --cmd is sent as a
line; '>' starts a console command (muse_input.c). --until stops early when
that text appears after the last command. --grep keeps only matching lines
on stdout (the --out file gets everything).
"""
import argparse
import re
import sys
import time

import serial


def open_port(name, wait):
    t = time.time()
    while True:
        try:
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = name, 115200, 0.2
            s.dtr = False
            s.rts = False
            s.open()
            return s
        except Exception:
            if time.time() - t > wait:
                raise
            time.sleep(0.3)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("--cmd", action="append", default=[])
    ap.add_argument("--gap", type=float, default=0.5, help="seconds between commands")
    ap.add_argument("--until", default=None)
    ap.add_argument("--secs", type=float, default=20)
    ap.add_argument("--wait-port", type=float, default=20)
    ap.add_argument("--grep", default=None)
    ap.add_argument("--out", default=None)
    a = ap.parse_args()

    s = open_port(a.port, a.wait_port)
    out = open(a.out, "wb") if a.out else None
    pat = re.compile(a.grep) if a.grep else None
    buf = b""
    seen_after_cmds = b""

    def pump(dur):
        nonlocal buf, seen_after_cmds
        end = time.time() + dur
        while time.time() < end:
            try:
                d = s.read(4096)
            except Exception:
                time.sleep(0.3)
                try:
                    s.close()
                    s.open()
                except Exception:
                    pass
                continue
            if not d:
                continue
            if out:
                out.write(d)
                out.flush()
            seen_after_cmds += d
            buf += d
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").rstrip("\r")
                if not pat or pat.search(text):
                    print(text, flush=True)
            if a.until and a.until.encode() in seen_after_cmds:
                return True
        return False

    pump(0.3)
    for c in a.cmd:
        seen_after_cmds = b""
        s.write((c + "\n").encode())
        s.flush()
        pump(a.gap)
    pump(a.secs)
    s.close()


if __name__ == "__main__":
    sys.exit(main())
