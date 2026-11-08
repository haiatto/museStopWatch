---
name: msw-app
description: Make, build, install and measure a museStopWatch mini app (ELF app run from PSRAM on the M5Stack StopWatch) from this workspace. Use when asked to write or fix an app under apps/, or to change the mini-app runtime (components/miniapp) and its SDK.
---

# museStopWatch mini apps (this workspace)

Muse gets the same knowledge from the firmware's built-in guide
(`muse-gadget-sdk/esp32/components/miniapp/guide.md`, served by `app.doc`).
Read it first: API summary, patterns, pitfalls, rules. Keep it and this
skill in step: a lesson learned here goes into the guide too.

## Where things are

- API header: `muse-gadget-sdk/esp32/components/miniapp/include/msw_app.h`
- Runtime: `components/miniapp/msw_app_*.c` (`msw_app_priv.h` lists them)
- What apps may call besides msw_*: `sdk/exports.list`
- Apps: `apps/<name>/app.json` + `*.c` (blocks, pyoko, aquarium are examples)

## Commands (workspace root, Git Bash or PowerShell)

```
.venv/Scripts/python.exe tools/gen_msw_exports.py      # after changing msw_app.h or exports.list
.\dev.ps1 build                                        # firmware (needed after the line above)
.venv/Scripts/python.exe tools/make_sdk.py             # after a firmware build: sdk/include
.venv/Scripts/python.exe sdk/build.py apps/<name>      # -> apps/<name>/build/<id>.mswapp
.venv/Scripts/python.exe tools/msw_app_install.py COM5 apps/<name>/build/<id>.mswapp --run
.venv/Scripts/python.exe tools/msw_app_install.py COM5 --cmd "msw.app.log"
```

Other console commands: `msw.app.list`, `msw.app.run <id>`, `msw.app.stop`,
`msw.app.rm <id>`, `msw.imu` (raw and mapped IMU values).

## Steps

1. Write the app. Original art, names and music only.
2. Build it. A "the firmware does not give: X" error means: use something
   else, or add X to `sdk/exports.list` (inline lvgl functions cannot be
   exported: export what they call), regenerate, rebuild the firmware.
3. Check the COM port (`.\dev.ps1 ports`), tell the user, install and run.
4. Run it ~10 s, `msw.app.stop`, then read `msw.app.log`: ticks a second,
   tick time, present time, leaks freed. Crashes restart the watch and the
   log names the app on the next boot.
5. Ask the user what the screen shows and how it sounds. A clean log is not
   a device test.

## Measured (2026-11-08)

- Full-frame present 26.7 ms (0.12 µs/px, QSPI 40 MHz); drawing ~25 ns per
  pixel written in PSRAM. Full redraw with layers: ~18-20 fps.
- App load: 1-4 ms. PSRAM free with an app and its frame: ~2 MB.

## Lessons

- Touch is one finger (rule, user 2026-11-08). The CST820 does report two
  (count at 0x02, 6 bytes a finger from 0x03; `msw.tp <s>` dumps the raw
  registers), and msw_touches was tried, but it is self-capacitive: with
  an arrow held and a jump tapped it often went 2 -> 0 -> 1 and lost the
  held finger for good (work/logs-2026-11-07/tpdump3.log). msw_touches was
  removed; apps must not need two touches. Put the second action on the
  blue button (apps/pyoko: arrows on the screen, jump on blue, a blue mark
  at the upper-right rim). A slide-up-to-jump gesture was also tried and
  removed: a held finger drifts, so it fired by mistake.
- Buttons: yellow = upper left (the speaker button outside apps), blue =
  upper right (talk outside apps). In apps yellow opens the runtime's app
  menu (つづける / おわる, msw_app_menu.c) and pauses ticks; blue is the
  app's (MSW_BUTTON_BLUE), not sent to Muse. User rule (2026-11-08): this is
  the standard; an app that does otherwise (msw_menu_own) must tell the
  user how to quit.
- On-screen pads: trial (2026-11-08) along the rim at the lower left and
  lower right (the left/right rim at mid height hid too much of the
  screen under the thumbs): see apps/blocks and apps/pyoko.
