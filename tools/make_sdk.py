"""Copy the headers a mini app compiles against into sdk/include.

    .venv\\Scripts\\python.exe tools\\make_sdk.py

Run after a firmware build (it needs the build's sdkconfig.h and the
downloaded lvgl). Writes
  sdk/include/msw_app.h      the API (from components/muse_diy_stopwatch/miniapp/include)
  sdk/include/muse_speaking.h  the official speaking gestures, for avatars
  sdk/include/sdkconfig.h    the firmware's configuration (lvgl reads it)
  sdk/include/lvgl/...       lvgl's headers (lvgl.h and src/**/*.h)
so sdk/ is self-contained: sdk/build.py, the stub and these headers are all
an app build needs besides the Xtensa gcc.
"""
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FW = os.path.join(ROOT, "muse-gadget-sdk", "esp32")
LVGL = os.path.join(FW, "managed_components", "lvgl__lvgl")
CONFIG = os.path.join(FW, "build-muse-m5stack-stopwatch", "config", "sdkconfig.h")
HEADER = os.path.join(FW, "components", "muse_diy_stopwatch", "miniapp", "include", "msw_app.h")
# The official speaking gestures (header only, Apache 2.0): avatars use them as avatar/muse_pixel.c does.
SPEAKING = os.path.join(FW, "components", "muse", "muse_speaking.h")
OUT = os.path.join(ROOT, "sdk", "include")


def main():
    if os.path.isdir(os.path.join(OUT, "lvgl")):
        shutil.rmtree(os.path.join(OUT, "lvgl"))
    os.makedirs(OUT, exist_ok=True)
    shutil.copy2(HEADER, OUT)
    shutil.copy2(SPEAKING, OUT)
    shutil.copy2(CONFIG, OUT)
    n = 0
    # LICENCE.txt: lvgl is MIT, and its notice must go with the copied headers.
    for base in ("lvgl.h", "lv_version.h", "LICENCE.txt"):
        src = os.path.join(LVGL, base)
        if os.path.exists(src):
            os.makedirs(os.path.join(OUT, "lvgl"), exist_ok=True)
            shutil.copy2(src, os.path.join(OUT, "lvgl", base))
            n += 1
    for dirpath, _, files in os.walk(os.path.join(LVGL, "src")):
        for f in files:
            if f.endswith(".h"):
                rel = os.path.relpath(os.path.join(dirpath, f), LVGL)
                dst = os.path.join(OUT, "lvgl", rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                shutil.copy2(os.path.join(dirpath, f), dst)
                n += 1
    # lvgl's Kconfig glue includes esp_attr.h; an app needs none of it.
    with open(os.path.join(OUT, "esp_attr.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("/* museStopWatch SDK: stands in for ESP-IDF's esp_attr.h (tools/make_sdk.py). */\n"
                "#pragma once\n#define IRAM_ATTR\n#define DRAM_ATTR\n#define EXT_RAM_BSS_ATTR\n")
    print(f"sdk/include: msw_app.h, muse_speaking.h, sdkconfig.h and {n} lvgl headers")


if __name__ == "__main__":
    main()
