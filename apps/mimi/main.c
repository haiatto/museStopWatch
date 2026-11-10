/*
 * みみ: tries the mic (msw_mic_*). The circle grows with the sound, the
 * band below shows the last 3 seconds. Hold blue to record (up to 3 s),
 * let go to hear it back.
 */
#include "msw_app.h"

#define CX 233
#define CY 233
#define HIST 120                    /* level history: 120 ticks at 40 fps = 3 s */
#define REC_MAX (16000 * 3)

static msw_gfx_t *fb;
static int s_hist[HIST];
static int s_hist_at;
static int16_t s_rec[REC_MAX];
static int s_rec_n, s_play_at;
static bool s_recording, s_playing;
static int16_t s_buf[2048];
static uint32_t s_got, s_log_ms;
static int s_peak, s_amp;

static uint16_t level_color(int v)
{
    return v < 30 ? MSW_RGB(77, 208, 225) : v < 70 ? MSW_RGB(255, 213, 79) : MSW_RGB(255, 112, 67);
}

static void tick(uint32_t dt_ms, void *user)
{
    (void)user;
    (void)dt_ms;
    /* Take everything heard since the last tick. */
    int n;
    while ((n = msw_mic_read(s_buf, 2048)) > 0) {
        s_got += (uint32_t)n;
        for (int i = 0; i < n; i++) {
            int a = s_buf[i] < 0 ? -s_buf[i] : s_buf[i];
            s_amp = a > s_amp ? a : s_amp;
        }
        if (s_recording) {
            int room = REC_MAX - s_rec_n;
            int take = n < room ? n : room;
            for (int i = 0; i < take; i++) {
                s_rec[s_rec_n + i] = s_buf[i];
            }
            s_rec_n += take;
        }
    }
    if (s_playing) {
        if (s_play_at < s_rec_n) {
            s_play_at += msw_pcm_write(s_rec + s_play_at, s_rec_n - s_play_at);
        } else {
            s_playing = false;
        }
    }
    int level = msw_mic_level();
    s_hist[s_hist_at] = level;
    s_hist_at = (s_hist_at + 1) % HIST;
    if (level > s_peak) {
        s_peak = level;
    }
    if (msw_millis() - s_log_ms >= 2000) {
        msw_log("level %d peak %d amp %d, %u samples in %u ms", level, s_peak, s_amp, (unsigned)s_got,
                (unsigned)(msw_millis() - s_log_ms));
        s_log_ms = msw_millis();
        s_got = 0;
        s_peak = 0;
        s_amp = 0;
    }

    msw_gfx_fill(fb, MSW_RGB(0, 0, 0));
    if (level < 0) {
        msw_gfx_circle(fb, CX, 190, 40, MSW_RGB(90, 90, 90));
        msw_gfx_text(fb, 130, 250, "マイクはいま使えません", MSW_RGB(160, 160, 160), 0);
    } else {
        int r = 24 + level * 110 / 100;
        msw_gfx_fill_circle(fb, CX, 190, r, level_color(level));
        msw_gfx_circle(fb, CX, 190, 134, MSW_RGB(60, 60, 60));
    }
    /* The last 3 seconds as bars. */
    for (int i = 0; i < HIST; i++) {
        int v = s_hist[(s_hist_at + i) % HIST];
        int h = v < 0 ? 1 : 1 + v * 50 / 100;
        msw_gfx_fill_rect(fb, 113 + i * 2, 380 - h, 1, h, v < 0 ? MSW_RGB(60, 60, 60) : level_color(v));
    }
    const char *say = s_recording ? "録音中…はなすと再生"
                      : s_playing ? "再生中"
                      : "青ボタンを押す間 録音";
    uint16_t c = s_recording ? MSW_RGB(255, 112, 67) : MSW_RGB(230, 230, 230);
    msw_gfx_text(fb, CX - msw_text_width(say, 0) / 2, 395, say, c, 0);
    if (s_rec_n) {
        char t[24];
        int ds = s_rec_n / 1600;
        t[0] = (char)('0' + ds / 10);
        t[1] = '.';
        t[2] = (char)('0' + ds % 10);
        t[3] = 's';
        t[4] = '\0';
        msw_gfx_text(fb, CX - 16, 60, t, MSW_RGB(200, 200, 200), 16);
    }
    msw_fb_present();
}

static void button(int b, bool down, void *user)
{
    (void)user;
    if (b != MSW_BUTTON_BLUE) {
        return;
    }
    if (down) {
        msw_sound_stop(-1);
        s_playing = false;
        s_rec_n = 0;
        s_recording = true;
    } else if (s_recording) {
        s_recording = false;
        s_play_at = 0;
        s_playing = s_rec_n > 0;
        msw_log("recorded %d samples", s_rec_n);
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
    for (int i = 0; i < HIST; i++) {
        s_hist[i] = -1;
    }
    msw_mic_on(true);
    msw_set_fps(40);
    msw_on_tick(tick, NULL);
    msw_on_button(button, NULL);
    return 0;
}
