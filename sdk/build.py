"""Build a museStopWatch mini app into a .mswapp package.

    python sdk/build.py apps/<name>            -> apps/<name>/build/<id>.mswapp
    python sdk/build.py apps/<name> --no-source   the same without the app's files

An app is a folder with app.json and its .c files:

    {
      "id": "blocks",             a-z, 0-9 and _, up to 23 letters; the store key
      "name": "ブロック",          shown under the icon (UTF-8, up to 31 bytes)
      "version": 1,
      "icon": ["....RR....", ...],   text art, up to 32 x 32 ('.' transparent;
                                     16 x 16 or smaller is drawn twice the size)
      "palette": {"R": "#e53935", ...},
      "port": {"connect": "M5Stack の Dual Button ユニット",
               "about": "2つのボタンで、画面のキャラを左右に動かします"}
    }

"port" is needed when the app uses PORT.A (msw_port_power, msw_i2c_begin,
msw_pin_read, msw_uart_begin), in words for the user: "connect", what to plug into PORT.A
(up to 120 bytes), and "about", what the app does with it (up to 240 bytes).
The StopWatch shows both before the app starts, and the app runs only after
the user taps "はじめる" holding the blue button. They go in the package as
the data file ".port" (connect, a newline, about).

"commands" lists what the app takes from Muse's app.call (msw_on_muse_call):
[{"name": "servo", "about": "what it does", "args": {"ch": "0 to 7"}}, ...].
It goes in the package as the data file ".commands"; app.commands returns it.

An avatar (Muse's look on the face screen, msw_avatar_define) is built the
same way with "kind": "avatar" in app.json: its package has the magic "MSWV"
and no icon, and it may call no msw_ function but msw_avatar_define.

The compiler: $MSW_GCC, or xtensa-esp32s3-elf-gcc on PATH, or the one ESP-IDF
installed under .tools/ in this project. Everything else comes from sdk/
(include/ from tools/make_sdk.py, stub/ from tools/gen_msw_exports.py).

Three links: the app itself (a shared object, entry app_main, no C library,
stripped: <id>.so goes in the package), the same with its symbols and debug
lines (<id>.elf, kept in build/ for addr2line on the ELF addresses in a
crash report), and a check that links it against stub/msw_stub.c with
--no-undefined, so a call the firmware does not give fails here and not on
the watch.

Files in the app's folder data/ go in the package as they are; the app
reads them in place with msw_app_data("<name>", &size) (no copy in RAM).

The package also keeps the app's files (app.json, .c, .h) as one text in
zlib, which app.source returns. The package can be as large as the
StopWatch's free package space; app.list says how much that is.
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
API_VERSION = 5                # 2: msw_app_data and the data/ folder (2026-11-11); 3: msw_pin_read, "port" (2026-11-13);
                               # 4: msw_uart_*, msw_jpeg_*, msw_nn_* (2026-11-14);
                               # 5: msw_on_muse_call, msw_arg_*, "commands" (2026-11-14)
COMMAND_NAME = r"[a-z0-9_]{1,31}"
COMMANDS_MAX = 4096            # bytes of the ".commands" JSON
PORT_FNS = ("msw_port_power", "msw_i2c_begin", "msw_pin_read", "msw_uart_begin")   # msw_app_core.c is_port_fn
PORT_CONNECT_MAX = 120
PORT_ABOUT_MAX = 240
ICON = 32
ICON_BG = (0x1A, 0x1F, 0x26)   # the launcher's icon ring
HEAD_FMT = "<4sHH24s32sHHIII"
FILE_NAME_MAX = 47             # msw_pkg_file_t's name, less its NUL

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
    """The app's own files as one UTF-8 text in zlib, after "MSWZ", the zlib
    length and the text length. The watch keeps it with the app; app.source
    returns the text. (Firmware before 2026-11-11 reads "MSWS" only: there
    app.source says the app has no source.)"""
    names = sorted(n for n in os.listdir(app)
                   if os.path.isfile(os.path.join(app, n))
                   and (n == "app.json" or n.endswith((".c", ".h"))))
    parts = []
    for n in names:
        text = open(os.path.join(app, n), encoding="utf-8").read()
        parts.append(f"--- {n} ---\n{text}" + ("" if text.endswith("\n") else "\n"))
    data = "".join(parts).encode("utf-8")
    z = zlib.compress(data, 9)
    return b"MSWZ" + struct.pack("<II", 4 + len(z), len(data)) + z, len(data)


def make_commands(cmds):
    """app.json "commands" as the data file ".commands" (compact JSON), which
    app.commands returns to Muse:
        [{"name": "servo", "about": "what it does",
          "args": {"ch": "0 to 7", "angle": "0 to 180"}}, ...]
    "args" is optional; each value says in words what the argument is."""
    if not isinstance(cmds, list):
        sys.exit('app.json: "commands" is a list of {"name", "about", "args"}')
    out, seen = [], set()
    for c in cmds:
        if not isinstance(c, dict):
            sys.exit('app.json: each of "commands" is {"name", "about", "args"}')
        name = c.get("name")
        if not isinstance(name, str) or not re.fullmatch(COMMAND_NAME, name):
            sys.exit(f'app.json: command name {name!r}: 1 to 31 of a-z, 0-9 and _')
        if name in seen:
            sys.exit(f'app.json: command "{name}" is there twice')
        seen.add(name)
        about = c.get("about")
        if not isinstance(about, str) or not about.strip():
            sys.exit(f'app.json: command "{name}" needs "about": what it does')
        args = c.get("args", {})
        if not isinstance(args, dict) or not all(isinstance(k, str) and isinstance(v, str) for k, v in args.items()):
            sys.exit(f'app.json: command "{name}": "args" is {{"key": "what it is", ...}}')
        bad = set(c) - {"name", "about", "args"}
        if bad:
            sys.exit(f'app.json: command "{name}": unknown keys {", ".join(sorted(bad))}')
        item = {"name": name, "about": about.strip()}
        if args:
            item["args"] = args
        out.append(item)
    data = json.dumps(out, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(data) > COMMANDS_MAX:
        sys.exit(f'app.json: "commands" is {len(data)} bytes as JSON; {COMMANDS_MAX} at most')
    return data


def make_data(app, at, extra=None):
    """The files in data/ as the "MSWD" section starting at offset at of the
    package: count, then (name[48], offset, size) each, then the files,
    16-aligned from the package's start (msw_app_priv.h)."""
    d = os.path.join(app, "data")
    extra = extra or {}
    names = sorted(n for n in os.listdir(d) if os.path.isfile(os.path.join(d, n))) if os.path.isdir(d) else []
    for n in extra:
        if n in names:
            sys.exit(f"data/{n}: the name is the firmware's (app.json)")
    names = sorted(names + list(extra))
    if not names:
        return b""
    for n in names:
        if len(n.encode("utf-8")) > FILE_NAME_MAX:
            sys.exit(f"data/{n}: names up to {FILE_NAME_MAX} bytes")
    table_end = at + 8 + 4 + 56 * len(names)
    pos, blobs, table = table_end, b"", b""
    for n in names:
        blob = extra[n] if n in extra else open(os.path.join(d, n), "rb").read()
        pad = (-pos) % 16
        blobs += b"\0" * pad
        pos += pad
        table += struct.pack("<48sII", n.encode("utf-8"), pos, len(blob))
        blobs += blob
        pos += len(blob)
    body = struct.pack("<I", len(names)) + table + blobs
    print(f"data: {len(names)} files, {pos - table_end} bytes")
    return b"MSWD" + struct.pack("<I", len(body)) + body


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


STATES = ["boot", "idle", "listening", "thinking", "working", "speaking", "happy", "error", "off"]
HOST_FRAMES = 4


def png(path, w, h, rgb):
    """rgb: bytes, 3 a pixel, rows top to bottom."""
    rows = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))


def frame_rgb(grid, pal, scale):
    """One 64 x 64 frame blown up scale times: rows of RGB bytes."""
    lut = []
    for c in struct.unpack("<256H", pal):
        r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
        lut.append(bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31)))
    out = []
    for y in range(64):
        row = b"".join(lut[i] * scale for i in grid[y * 64:(y + 1) * 64])
        out.extend([row] * scale)
    return out


def previews(dump, out):
    """build/preview/<state>.png (the frames left to right) and all.png (a row a state)."""
    size = 64 * 64 + 512
    data = open(dump, "rb").read()
    if len(data) != size * HOST_FRAMES * len(STATES):
        return
    pdir = os.path.join(out, "preview")
    os.makedirs(pdir, exist_ok=True)
    all_rows = []
    for si, name in enumerate(STATES):
        frames3, frames2 = [], []
        for k in range(HOST_FRAMES):
            o = (si * HOST_FRAMES + k) * size
            grid, pal = data[o:o + 4096], data[o + 4096:o + size]
            frames3.append(frame_rgb(grid, pal, 3))
            frames2.append(frame_rgb(grid, pal, 2))
        rows = [b"".join(f[y] for f in frames3) for y in range(64 * 3)]
        png(os.path.join(pdir, name + ".png"), 64 * 3 * HOST_FRAMES, 64 * 3, b"".join(rows))
        all_rows.extend(b"".join(f[y] for f in frames2) for y in range(64 * 2))
    png(os.path.join(pdir, "all.png"), 64 * 2 * HOST_FRAMES, 64 * 2 * len(STATES), b"".join(all_rows))
    print(f"{os.path.relpath(pdir)}: all.png (a row a state: {', '.join(STATES)}) and one PNG a state")


def host_check(app, srcs, incs, out):
    """Runs the avatar on this machine, as tools/muse/avatar.py does for the
    official one: every state, with the sanitizers. Stops the build on a
    fault. MSW_NO_HOST_CHECK=1 skips it."""
    if os.environ.get("MSW_NO_HOST_CHECK"):
        print("host check: skipped (MSW_NO_HOST_CHECK)")
        return
    cc = os.environ.get("MSW_HOST_CC") or shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        print("host check: skipped, no C compiler for this machine (install gcc, or set MSW_HOST_CC)")
        return
    exe = os.path.join(out, "avatar_host")
    cmd = [cc, "-std=gnu17", "-O1", "-g", "-Wall", "-Wno-unused-function", "-DMSW_APP", "-DMSW_HOST",
           "-DLV_CONF_INCLUDE_SIMPLE", *incs, *srcs, os.path.join(SDK, "avatar_host.c"), "-o", exe, "-lm"]
    san = ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-fno-omit-frame-pointer"]
    r = subprocess.run(cmd[:1] + san + cmd[1:], capture_output=True, text=True, errors="replace")
    sanitized = r.returncode == 0
    if not sanitized:
        r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    if r.returncode:
        print((r.stdout + r.stderr).strip()[-4000:])
        sys.exit("host check: the avatar does not compile with " + os.path.basename(cc) +
                 " (fix it, or MSW_NO_HOST_CHECK=1 to skip)")
    warn = (r.stdout + r.stderr).strip()
    if warn:
        print(warn[-4000:])
    dump = os.path.join(out, "avatar_host.bin")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="print_stacktrace=1")
    try:
        r = subprocess.run([exe, dump, str(HOST_FRAMES)], capture_output=True, text=True, errors="replace",
                           timeout=120, env=env)
    except subprocess.TimeoutExpired:
        sys.exit("host check: the avatar did not finish in 120 s (a loop that never ends?)")
    text = (r.stdout + r.stderr).strip()
    if r.returncode:
        print("\n".join(text.splitlines()[-60:]))
        sys.exit("host check failed: the avatar crashed or broke a rule above (every state ran: "
                 + ", ".join(STATES) + ")")
    print("host check: OK, every state ran" + (" with the address and UB sanitizers" if sanitized
                                                else " (no sanitizers on this compiler)"))
    previews(dump, out)


def main():
    args = sys.argv[1:]
    keep_source = "--no-source" not in args
    args = [a for a in args if a != "--no-source"]
    if len(args) != 1 or args[0].startswith("-"):
        sys.exit(__doc__)
    app = os.path.abspath(args[0])
    meta = json.load(open(os.path.join(app, "app.json"), encoding="utf-8"))
    app_id = meta["id"]
    if not re.fullmatch(r"[a-z0-9_]{1,23}", app_id):
        sys.exit("id: 1 to 23 of a-z, 0-9 and _")
    name = meta.get("name", app_id).encode("utf-8")
    if len(name) > 31:
        sys.exit("name: 31 bytes at most (a Japanese letter takes 3)")
    version = int(meta.get("version", 1))
    kind = meta.get("kind", "app")
    if kind not in ("app", "avatar"):
        sys.exit('kind: "app" (the default) or "avatar"')
    avatar = kind == "avatar"
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
    nm = re.sub(r"gcc(\.exe)?$", r"nm\1", gcc)
    r = subprocess.run([nm, "-D", "-u", so], capture_output=True, text=True, errors="replace")
    imports = set(re.findall(r"\bU (\w+)", r.stdout))
    extra = {}
    uses_port = sorted(imports & set(PORT_FNS))
    if uses_port:
        port = meta.get("port")
        need = ('app.json: "port" is needed: the app uses PORT.A (' + ", ".join(uses_port) + '). '
                '{"connect": what to plug in, "about": what the app does with it}, for the user; '
                "both are shown before the app starts")
        if not isinstance(port, dict):
            sys.exit(need)
        connect = str(port.get("connect", "")).strip()
        about = str(port.get("about", "")).strip()
        if not connect or not about:
            sys.exit(need)
        if "\n" in connect:
            sys.exit('app.json: "port"."connect" is one line')
        if len(connect.encode("utf-8")) > PORT_CONNECT_MAX:
            sys.exit(f'app.json: "port"."connect" up to {PORT_CONNECT_MAX} bytes (a Japanese letter takes 3)')
        if len(about.encode("utf-8")) > PORT_ABOUT_MAX:
            sys.exit(f'app.json: "port"."about" up to {PORT_ABOUT_MAX} bytes (a Japanese letter takes 3)')
        extra[".port"] = (connect + "\n" + about).encode("utf-8")
    elif meta.get("port"):
        print('note: "port" in app.json, but the app does not use PORT.A: no start screen')
    cmds = meta.get("commands")
    if cmds is not None:
        if avatar:
            sys.exit('app.json: an avatar takes no "commands"')
        extra[".commands"] = make_commands(cmds)
        if "msw_on_muse_call" not in imports:
            print('note: "commands" in app.json, but the app does not call msw_on_muse_call: app.call gets no_commands')
    elif "msw_on_muse_call" in imports:
        print('note: the app calls msw_on_muse_call, but app.json has no "commands": Muse cannot read what it takes')
    if avatar:
        bad = sorted(set(n for n in imports
                         if n.startswith("msw_") and n != "msw_avatar_define"))
        if bad:
            sys.exit("an avatar can call no msw_ function but msw_avatar_define: " + ", ".join(bad))
        host_check(app, srcs, incs, out)

    elf = open(so, "rb").read()
    check_layout(elf)
    icon = b"" if avatar else make_icon(meta)
    head_size = struct.calcsize(HEAD_FMT)
    data = make_data(app, head_size + len(icon) + len(elf), extra)
    source, raw = make_source(app) if keep_source else (b"", 0)
    body = icon + elf + data + source
    head = struct.pack(HEAD_FMT, b"MSWV" if avatar else b"MSWA", API_VERSION, version, app_id.encode(),
                       name, ICON if icon else 0, ICON if icon else 0, len(icon), len(elf),
                       zlib.crc32(body) & 0xFFFFFFFF)
    pkg = os.path.join(out, app_id + ".mswapp")
    with open(pkg, "wb") as f:
        f.write(head + body)
    print(f"{os.path.relpath(pkg)}: {len(head) + len(body)} bytes (code {len(elf)}, icon {len(icon)}, data {len(data)}, source {len(source)}"
          + (f" in zlib, {raw} as text" if source else "") + ")")
    print(f"{os.path.relpath(debug_elf)}: for addr2line -pfiaC -e {os.path.relpath(debug_elf)} <ELF address from app.log>")


if __name__ == "__main__":
    main()
