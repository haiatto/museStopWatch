"""Save the device's screen as an image, over the serial console.

    python tools/msw_shot.py COM5 out.png [--round]

Sends `>msw.app.shot full` (the 466 x 466 screen as JPEG, the same as Muse's
app.screenshot with size "full"), decodes the base64 lines and saves the
picture. --round makes the corners outside the round screen transparent
(PNG), as the screen looks on the device.
"""
import argparse
import base64
import io
import re
import sys
import time

import serial
from PIL import Image, ImageDraw


def shoot(port, secs=20):
    # No DTR/RTS (no reset). '>' starts a console command; without it each
    # letter is a key of its own (muse_input.c).
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.2
    s.dtr = False
    s.rts = False
    s.open()
    with s:
        s.reset_input_buffer()
        s.write(b">msw.app.shot full\n")
        buf = b""
        end = time.time() + secs
        while time.time() < end:
            buf += s.read(65536)
            if b"@msw.app.shot done" in buf or b"@msw.app.shot error" in buf:
                break
    text = buf.decode("utf-8", "replace")
    m = re.search(r"@msw\.app\.shot error[^\n]*", text)
    if m:
        sys.exit(m.group(0))
    a = text.find("@msw.app.shot ")
    b = text.find("@msw.app.shot done")
    if a < 0 or b < 0:
        sys.exit("no screenshot in the reply")
    lines = text[a:b].splitlines()[1:]
    data = "".join(l.strip() for l in lines if re.fullmatch(r"[A-Za-z0-9+/=]+", l.strip()))
    return base64.b64decode(data)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("out")
    ap.add_argument("--round", action="store_true")
    a = ap.parse_args()
    img = Image.open(io.BytesIO(shoot(a.port))).convert("RGB")
    if a.round:
        mask = Image.new("L", (img.width * 4, img.height * 4), 0)
        ImageDraw.Draw(mask).ellipse((0, 0, mask.width - 1, mask.height - 1), fill=255)
        img.putalpha(mask.resize(img.size, Image.LANCZOS))
    img.save(a.out)
    print(a.out, img.size)


if __name__ == "__main__":
    main()
