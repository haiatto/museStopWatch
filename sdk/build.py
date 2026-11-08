"""Build a museStopWatch mini app into a .mswapp package.

    python sdk/build.py apps/<name>            -> apps/<name>/build/<id>.mswapp

An app is a folder with app.json and its .c files:

    {
      "id": "blocks",             a-z, 0-9 and _, up to 23 letters; the store key
      "name": "ブロック",          shown under the icon (UTF-8, up to 31 bytes)
      "version": 1,
      "icon": ["....RR....", ...],   text art, up to 32 x 32 ('.' transparent;
                                     16 x 16 or smaller is drawn twice the size)
      "palette": {"R": "#e53935", ...}
    }

The compiler: $MSW_GCC, or xtensa-esp32s3-elf-gcc on PATH, or the one ESP-IDF
installed under .tools/ in this project. Everything else comes from sdk/
(include/ from tools/make_sdk.py, stub/ from tools/gen_msw_exports.py).

Two links: the app itself (a shared object, entry app_main, no C library),
and a check that links it against stub/msw_stub.c with --no-undefined, so a
call the firmware does not give fails here and not on the watch.
"""
import glob
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

SDK = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(SDK)
API_VERSION = 1
ICON = 32
ICON_BG = (0x1A, 0x1F, 0x26)   # the launcher's icon ring

CFLAGS = [
    "-mlongcalls", "-Os", "-fPIC", "-ffunction-sections", "-fdata-sections",
    "-fno-common", "-fvisibility=hidden", "-fno-jump-tables", "-fno-tree-switch-conversion",
    "-std=gnu17", "-Wall", "-Wno-unused-function",
    "-DESP_PLATFORM", "-DLV_CONF_INCLUDE_SIMPLE", "-DMSW_APP",
]
LDFLAGS = ["-shared", "-nostdlib", "-nostartfiles", "-e", "app_main",
           "-Wl,--gc-sections", "-Wl,--strip-all"]


def find_gcc():
    if os.environ.get("MSW_GCC"):
        return os.environ["MSW_GCC"]
    name = "xtensa-esp32s3-elf-gcc"
    found = shutil.which(name)
    if found:
        return found
    pat = os.path.join(ROOT, ".tools", "espressif", "tools", "xtensa-esp-elf", "*", "xtensa-esp-elf", "bin", name + "*")
    for p in sorted(glob.glob(pat), reverse=True):
        if os.path.basename(p) in (name, name + ".exe"):
            return p
    sys.exit("no " + name + ": set MSW_GCC or put it on PATH")


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    out = (r.stdout + r.stderr).strip()
    if out:
        print(out)
    if r.returncode:
        sys.exit("failed: " + os.path.basename(cmd[0]) + " " + " ".join(cmd[1:4]) + " ...")


def rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def color(text):
    m = re.fullmatch(r"#?([0-9a-fA-F]{6})", text.strip())
    if not m:
        sys.exit(f"bad colour {text!r}: use #rrggbb")
    v = int(m.group(1), 16)
    return (v >> 16) & 255, (v >> 8) & 255, v & 255


def make_icon(meta):
    art = meta.get("icon")
    if not art:
        return b""
    pal = {k: color(v) for k, v in meta.get("palette", {}).items()}
    h, w = len(art), max(len(r) for r in art)
    if h > ICON or w > ICON:
        sys.exit("icon art is larger than 32 x 32")
    scale = 2 if h <= 16 and w <= 16 else 1
    ox, oy = (ICON - w * scale) // 2, (ICON - h * scale) // 2
    px = [rgb565(*ICON_BG)] * (ICON * ICON)
    for y, row in enumerate(art):
        for x, ch in enumerate(row):
            if ch in ". ":
                continue
            if ch not in pal:
                sys.exit(f"icon: {ch!r} is not in the palette")
            c = rgb565(*pal[ch])
            for ky in range(scale):
                for kx in range(scale):
                    px[(oy + y * scale + ky) * ICON + ox + x * scale + kx] = c
    return struct.pack("<%dH" % len(px), *px)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    app = os.path.abspath(sys.argv[1])
    meta = json.load(open(os.path.join(app, "app.json"), encoding="utf-8"))
    app_id = meta["id"]
    if not re.fullmatch(r"[a-z0-9_]{1,23}", app_id):
        sys.exit("id: 1 to 23 of a-z, 0-9 and _")
    name = meta.get("name", app_id).encode("utf-8")
    if len(name) > 31:
        sys.exit("name: 31 bytes at most (a Japanese letter takes 3)")
    version = int(meta.get("version", 1))
    srcs = sorted(glob.glob(os.path.join(app, "*.c")))
    if not srcs:
        sys.exit("no .c files in " + app)

    gcc = find_gcc()
    inc = os.path.join(SDK, "include")
    if not os.path.exists(os.path.join(inc, "lvgl", "lvgl.h")):
        sys.exit("sdk/include has no lvgl: run tools/make_sdk.py")
    out = os.path.join(app, "build")
    os.makedirs(out, exist_ok=True)
    incs = ["-I" + inc, "-I" + os.path.join(inc, "lvgl"), "-I" + app]
    objs = []
    for src in srcs:
        obj = os.path.join(out, os.path.splitext(os.path.basename(src))[0] + ".o")
        run([gcc, *CFLAGS, *incs, "-c", src, "-o", obj])
        objs.append(obj)

    so = os.path.join(out, app_id + ".so")
    run([gcc, *LDFLAGS, *objs, "-o", so])

    # The check: every symbol the app needs must be one the firmware gives.
    stub_o = os.path.join(out, "msw_stub.o")
    run([gcc, "-mlongcalls", "-fPIC", "-fno-builtin", "-w", "-c",
         os.path.join(SDK, "stub", "msw_stub.c"), "-o", stub_o])
    check = os.path.join(out, "check.so")
    r = subprocess.run([gcc, "-shared", "-nostdlib", "-nostartfiles", "-e", "app_main", "-Wl,--no-undefined",
                        *objs, stub_o, "-o", check], capture_output=True, text=True, errors="replace")
    if r.returncode:
        missing = sorted(set(re.findall(r"undefined reference to `([^']+)'", r.stdout + r.stderr)))
        print((r.stdout + r.stderr).strip())
        sys.exit("the firmware does not give: " + ", ".join(missing) if missing else "link check failed")
    os.remove(check)

    elf = open(so, "rb").read()
    icon = make_icon(meta)
    body = icon + elf
    head = struct.pack("<4sHH24s32sHHIII", b"MSWA", API_VERSION, version, app_id.encode(),
                       name, ICON if icon else 0, ICON if icon else 0, len(icon), len(elf),
                       zlib.crc32(body) & 0xFFFFFFFF)
    pkg = os.path.join(out, app_id + ".mswapp")
    with open(pkg, "wb") as f:
        f.write(head + body)
    print(f"{os.path.relpath(pkg)}: {len(head) + len(body)} bytes (code {len(elf)}, icon {len(icon)})")


if __name__ == "__main__":
    main()
