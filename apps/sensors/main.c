/*
 * センサー: shows what the firmware gives an app besides touch and buttons:
 * the date and time, the battery, the step count and PORT.A (I2C devices
 * found on the Grove port). Blue scans PORT.A again. Muse can read the same
 * with app.call ("read", "scan").
 */
#include <stdio.h>
#include <string.h>

#include "msw_app.h"

#define CX 233

static msw_gfx_t *fb;
static uint8_t s_found[16];
static int s_nfound = -1;          /* -1: the bus did not open */
static uint32_t s_log_ms;
static const char *WDAY[7] = { "日", "月", "火", "水", "木", "金", "土" };

static void scan(void)
{
    s_nfound = msw_i2c_begin(100) ? msw_i2c_scan(s_found, 16) : -1;
    msw_log("PORT.A: %d found", s_nfound);
}

static void centre(int y, const char *s, uint16_t c, int size)
{
    msw_gfx_text(fb, CX - msw_text_width(s, size) / 2, y, s, c, size);
}

static void tick(uint32_t dt_ms, void *user)
{
    (void)dt_ms;
    (void)user;
    char line[64];
    msw_time_t t = { 0 };
    msw_battery_t b = { 0 };
    bool has_t = msw_time(&t);
    bool has_b = msw_battery(&b);
    int steps = msw_steps();

    msw_gfx_fill(fb, MSW_RGB(0, 0, 0));
    if (has_t) {
        snprintf(line, sizeof(line), "%02d:%02d:%02d", t.hour, t.minute, t.second);
        centre(80, line, MSW_RGB(255, 255, 255), 28);
        snprintf(line, sizeof(line), "%d年%d月%d日（%s）", t.year, t.month, t.day, WDAY[t.weekday % 7]);
        centre(120, line, MSW_RGB(200, 200, 200), 0);
    } else {
        centre(90, "時刻はまだ合っていません", MSW_RGB(160, 160, 160), 0);
    }
    if (has_b) {
        snprintf(line, sizeof(line), "電池 %d%%  %d.%02d V", b.percent, b.mv / 1000, b.mv % 1000 / 10);
        centre(175, line, MSW_RGB(129, 199, 132), 0);
        centre(200, b.charging ? "充電中" : b.usb ? "USB 給電" : "電池で動作中", MSW_RGB(160, 160, 160), 0);
    }
    snprintf(line, sizeof(line), "歩数 %d", steps);
    centre(250, line, MSW_RGB(255, 213, 79), 0);
    if (s_nfound < 0) {
        centre(300, "PORT.A を開けません", MSW_RGB(255, 112, 67), 0);
    } else if (s_nfound == 0) {
        centre(300, "PORT.A: 何もつながっていません", MSW_RGB(79, 195, 247), 0);
    } else {
        int o = snprintf(line, sizeof(line), "PORT.A:");
        for (int i = 0; i < s_nfound && o < (int)sizeof(line) - 6; i++) {
            o += snprintf(line + o, sizeof(line) - o, " %02X", s_found[i]);
        }
        centre(300, line, MSW_RGB(79, 195, 247), 0);
    }
    centre(370, "青ボタンで PORT.A を調べ直す", MSW_RGB(120, 120, 120), 0);
    msw_fb_present();

    if (msw_millis() - s_log_ms >= 2000) {
        s_log_ms = msw_millis();
        msw_log("time %s %04d-%02d-%02d %02d:%02d:%02d, battery %d%% %d mV%s%s, steps %d, port %d",
                has_t ? "ok" : "unset", t.year, t.month, t.day, t.hour, t.minute, t.second,
                has_b ? b.percent : -1, has_b ? b.mv : 0, has_b && b.charging ? " charging" : "",
                has_b && b.usb ? " usb" : "", steps, s_nfound);
    }
}

/* PORT.A's addresses as text: "3C 68", "none" or "closed". */
static int port_text(char *out, int cap)
{
    if (s_nfound < 0) {
        return snprintf(out, cap, "closed");
    }
    if (s_nfound == 0) {
        return snprintf(out, cap, "none");
    }
    int o = 0;
    for (int i = 0; i < s_nfound && o < cap - 4; i++) {
        o += snprintf(out + o, cap - o, i ? " %02X" : "%02X", s_found[i]);
    }
    return o;
}

/* Muse's app.call (app.json "commands"). */
static const char *on_call(const char *name, const msw_args_t *args, void *user)
{
    (void)args;
    (void)user;
    static char reply[200];
    char port[64];
    if (!strcmp(name, "scan")) {
        scan();
        port_text(port, sizeof(port));
        snprintf(reply, sizeof(reply), "port %s", port);
        return reply;
    }
    if (!strcmp(name, "read")) {
        msw_time_t t = { 0 };
        msw_battery_t b = { 0 };
        bool has_t = msw_time(&t);
        bool has_b = msw_battery(&b);
        port_text(port, sizeof(port));
        snprintf(reply, sizeof(reply), "time %s %04d-%02d-%02d %02d:%02d:%02d, battery %d%% %d mV%s, steps %d, port %s",
                 has_t ? "ok" : "unset", t.year, t.month, t.day, t.hour, t.minute, t.second,
                 has_b ? b.percent : -1, has_b ? b.mv : 0, has_b && b.charging ? " charging" : has_b && b.usb ? " usb" : "",
                 msw_steps(), port);
        return reply;
    }
    return NULL;
}

static void button(int b, bool down, void *user)
{
    (void)user;
    if (b == MSW_BUTTON_BLUE && down) {
        scan();
    }
}

int app_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fb = msw_fb();
    if (!fb) {
        return 1;
    }
    scan();
    msw_set_fps(5);
    msw_on_tick(tick, NULL);
    msw_on_button(button, NULL);
    msw_on_muse_call(on_call, NULL);
    return 0;
}
