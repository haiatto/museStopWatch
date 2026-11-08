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

Three links: the app itself (a shared object, entry app_main, no C library,
stripped: <id>.so goes in the package), the same with its symbols and debug
lines (<id>.elf, kept in build/ for addr2line on the ELF addresses in a
crash report), and a check that links it against stub/msw_stub.c with
--no-undefined, so a call the firmware does not give fails here and not on
the watch.
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
SOURCE_MAX = 256 * 1024        # the app's files kept in the package (app.source)

CFLAGS = [
    "-mlongcalls", "-Os", "-g", "-fPIC", "-ffunction-sections", "-fdata-sections",
    "-fno-common", "-fvisibility=hidden", "-fno-jump-tables", "-fno-tree-switch-conversion",
    "-std=gnu17", "-Wall", "-Wno-unused-function",
    "-DESP_PLATFORM", "-DLV_CONF_INCLUDE_SIMPLE", "-DMSW_APP",
]
LDFLAGS = ["-shared", "-nostdlib", "-nostartfiles", "-e", "app_main",
           "-Wl,--gc-sections"]


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


def make_source(app):
    """The app's own files as one UTF-8 text, after "MSWS" and its length.
    The watch keeps it with the app; app.source returns the text."""
    names = sorted(n for n in os.listdir(app)
                   if os.path.isfile(os.path.join(app, n))
                   and (n == "app.json" or n.endswith((".c", ".h"))))
    parts = []
    for n in names:
        text = open(os.path.join(app, n), encoding="utf-8").read()
        parts.append(f"--- {n} ---\n{text}" + ("" if text.endswith("\n") else "\n"))
    data = "".join(parts).encode("utf-8")
    if len(data) > SOURCE_MAX:
        print(f"note: source is {len(data)} bytes, over {SOURCE_MAX}: not kept in the package")
        return b""
    return b"MSWS" + struct.pack("<I", len(data)) + data


def check_layout(elf):
    """.rodata must not start inside .text's last word (see the align above)."""
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x2E)
    secs = [struct.unpack_from("<IIIIIIIIII", elf, shoff + i * shentsize) for i in range(shnum)]
    strtab = secs[shstrndx][4]
    def name(sh):
        return elf[strtab + sh[0]:elf.index(b"\0", strtab + sh[0])].decode()
    by = {name(sh): sh for sh in secs}
    if ".text" in by and ".rodata" in by:
        text_end = by[".text"][3] + by[".text"][5]
        if by[".rodata"][3] < (text_end + 3) & ~3:
            sys.exit("layout: .rodata starts inside .text's last word; the loader would break its first string")


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

    # elf_loader rounds .text up to 4 bytes when it maps the app, so a
    # .rodata starting at an odd address right after .text (only strings,
    # 1-aligned) would have its first string relocated into the code, where
    # the firmware can't read it byte by byte. A 4-aligned constant kept in
    # .rodata (-u keeps it from --gc-sections) aligns the whole section.
    align_c = os.path.join(out, "msw_align.c")
    with open(align_c, "w", encoding="utf-8") as f:
        f.write("const int msw_rodata_align[1] __attribute__((aligned(4))) = { 0 };\n")
    align_o = os.path.join(out, "msw_align.o")
    run([gcc, *CFLAGS, "-c", align_c, "-o", align_o])
    objs.append(align_o)
    LDFLAGS.append("-Wl,-u,msw_rodata_align")

    so = os.path.join(out, app_id + ".so")
    run([gcc, *LDFLAGS, "-Wl,--strip-all", *objs, "-o", so])
    debug_elf = os.path.join(out, app_id + ".elf")      # same addresses, with symbols
    run([gcc, *LDFLAGS, *objs, "-o", debug_elf])

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
    check_layout(elf)
    icon = make_icon(meta)
    source = make_source(app)
    body = icon + elf + source
    head = struct.pack("<4sHH24s32sHHIII", b"MSWA", API_VERSION, version, app_id.encode(),
                       name, ICON if icon else 0, ICON if icon else 0, len(icon), len(elf),
                       zlib.crc32(body) & 0xFFFFFFFF)
    pkg = os.path.join(out, app_id + ".mswapp")
    with open(pkg, "wb") as f:
        f.write(head + body)
    print(f"{os.path.relpath(pkg)}: {len(head) + len(body)} bytes (code {len(elf)}, icon {len(icon)}, source {len(source)})")
    print(f"{os.path.relpath(debug_elf)}: for addr2line -pfiaC -e {os.path.relpath(debug_elf)} <ELF address from app.log>")


if __name__ == "__main__":
    main()
