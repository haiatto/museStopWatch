"""Install a mini app on the StopWatch over USB serial (no Muse needed).

    .venv\\Scripts\\python.exe tools\\msw_app_install.py COM5 apps\\blocks\\build\\blocks.mswapp [--run]
    .venv\\Scripts\\python.exe tools\\msw_app_install.py COM5 --cmd "msw.app.log"

Sends the package as base64 lines (>msw.app.put / data / end, see
components/muse_diy_stopwatch/miniapp/msw_app_core.c), each answered before the next goes.
Opens the port without touching DTR/RTS (no reset). --cmd sends one other
console command (without '>') and prints the reply.
"""
import argparse
import base64
import sys
import time

import serial

CHUNK = 600   # bytes a line: 800 base64 letters, under the 1024 the console takes


def open_port(name):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = name, 115200, 0.2
    s.dtr = False
    s.rts = False
    s.open()
    return s


class Port:
    def __init__(self, name):
        self.s = open_port(name)
        self.buf = b""

    def send(self, line):
        self.s.write((">" + line + "\n").encode())

    def wait(self, prefix, secs=10.0):
        """The first line starting with prefix; prints other @ lines."""
        end = time.time() + secs
        while time.time() < end:
            d = self.s.read(self.s.in_waiting or 1)   # what has come: read(4096) waits the whole timeout
            if d:
                self.buf += d
            while b"\n" in self.buf:
                raw, self.buf = self.buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").rstrip("\r")
                if line.startswith(prefix):
                    return line
        return None

    def lines(self, secs):
        end = time.time() + secs
        while time.time() < end:
            d = self.s.read(4096)
            if d:
                sys.stdout.write(d.decode("utf-8", "replace"))
                sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("package", nargs="?")
    ap.add_argument("--run", action="store_true")
    ap.add_argument("--cmd")
    ap.add_argument("--secs", type=float, default=3)
    a = ap.parse_args()
    p = Port(a.port)
    if a.cmd:
        p.send(a.cmd)
        p.lines(a.secs)
        return
    data = open(a.package, "rb").read()
    p.send(f"msw.app.put {len(data)}")
    r = p.wait("@msw.app.put", 300)   # making room (compacting) can take a while
    if r != "@msw.app.put ready":
        sys.exit(f"put: {r}")
    t0 = time.time()
    off = 0
    while off < len(data):
        p.send("msw.app.data " + base64.b64encode(data[off:off + CHUNK]).decode())
        r = p.wait("@msw.app.data")
        if r is None:
            # A line or its answer gets lost now and then: ask where it is.
            p.send("msw.app.putpos")
            pos = p.wait("@msw.app.putpos")
            if not pos or "failed" in pos:
                sys.exit(f"data at {off}: no answer ({pos})")
            off = int(pos.split()[1])
            print(f"\nno answer: going on from {off}")
            continue
        if "failed" in r:
            sys.exit(f"data at {off}: {r}")
        off = min(off + CHUNK, len(data))
        print(f"\r{off}/{len(data)} bytes", end="", flush=True)
    print(f" in {time.time() - t0:.1f} s")
    p.send("msw.app.end")
    r = p.wait("@msw.app.end", 20)
    print(r)
    if not r or " ok " not in r + " ":
        sys.exit(1)
    if a.run:
        app_id = r.split()[2]
        p.send(f"msw.app.run {app_id}")
        print(p.wait("@msw.app.run", 20))


if __name__ == "__main__":
    main()
