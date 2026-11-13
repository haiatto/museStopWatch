/*
 * かみずもう: paper sumo for the museStopWatch, oblique view.
 *
 * A Dual Button unit on PORT.A: the yellow-wire button drives the red
 * rikishi (left), the white-wire button the blue one (right). Each press
 * makes your rikishi lunge; push the other out of the ring to win.
 * Tapping the left / right half of the screen works too (fallback).
 *
 * A button to GND reads 0 while pressed (msw_pin_read: internal pull-up).
 * The voice clips (data/*.pcm) are optional: without them the game has no calls.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "msw_app.h"

#define SCR 466
#define CX 233
#define CY 246            /* dohyo centre */
#define RX 172            /* ring half width on screen */
#define RY 80             /* ring half height (oblique squash) */
#define BASE_Y (CY + 26)  /* where the rikishi stand */
#define POS2PX 148.0f     /* ring units -> pixels */

static msw_gfx_t *fb;

static const uint16_t COL_BG = MSW_RGB(24, 20, 34);
static const uint16_t COL_STRAW = MSW_RGB(198, 164, 105);
static const uint16_t COL_CLAY = MSW_RGB(222, 192, 138);
static const uint16_t COL_RING = MSW_RGB(250, 246, 236);
static const uint16_t COL_SKIN = MSW_RGB(242, 200, 155);
static const uint16_t COL_SKIN2 = MSW_RGB(248, 216, 178);
static const uint16_t COL_HAIR = MSW_RGB(24, 24, 28);
static const uint16_t COL_RED = MSW_RGB(229, 57, 53);
static const uint16_t COL_BLUE = MSW_RGB(30, 136, 229);
static const uint16_t COL_WHITE = MSW_RGB(255, 255, 255);
static const uint16_t COL_DARK = MSW_RGB(16, 14, 22);

/* game state */
static float p1, p2;        /* positions in ring units, 0 = centre */
static float v1, v2;
static int hop1, hop2;      /* hop animation frames left */
static int shake;           /* ring shake frames left */
static int state;           /* 0 playing, 1 finished */
static int fin_timer;       /* frames until reset */
static int loser;           /* 1 or 2 when finished */
static int win1, win2;
static int prev_y, prev_w;  /* pin states last tick (1 = released) */
static int clock_ms;        /* ms since app start, for timing below */
static int hold1_ms, hold2_ms;   /* how long each button has been held */
static int charged1, charged2;   /* hold reached charge time */
static int combo1, combo2;       /* rapid-press combo counters */
static int edge1, edge2;         /* rikishi is at the ring edge now */
static int last_hit_ms;          /* last collision sound, for throttling */
/* the gyoji's recorded calls: 0 hakkeyoi, 1 nokotta, 2 aka wins, 3 ao wins */
static const int16_t *clip_pcm[4];
static int clip_len[4];
static const int16_t *cur_pcm;
static int cur_len, cur_pos, cur_on;
static void play_clip(int i)
{
    if (clip_pcm[i] && !cur_on)
    {
        cur_pcm = clip_pcm[i]; cur_len = clip_len[i];
        cur_pos = 0; cur_on = 1;
    }
}
static int lastpress1_ms, lastpress2_ms;

/* tiny sound-effect sequencer: events fire when clock_ms reaches at_ms */
struct sfx { int at_ms, ch, wave, freq, dur, vol; };
static struct sfx sfxq[12];
static int sfxn;
static void sfx_add(int delay_ms, int ch, int wave, int freq, int dur, int vol)
{
    if (sfxn >= 12) return;
    sfxq[sfxn].at_ms = clock_ms + delay_ms;
    sfxq[sfxn].ch = ch; sfxq[sfxn].wave = wave;
    sfxq[sfxn].freq = freq; sfxq[sfxn].dur = dur; sfxq[sfxn].vol = vol;
    sfxn++;
}
static void sfx_tick(void)
{
    for (int i = 0; i < sfxn; i++)
    {
        if (clock_ms >= sfxq[i].at_ms)
        {
            msw_sound(sfxq[i].ch, sfxq[i].wave, sfxq[i].freq, sfxq[i].dur,
                      sfxq[i].vol, true);
            sfxq[i] = sfxq[--sfxn];
            /* one sound per tick; keep the rest from piling onto it */
            for (int j = 0; j < sfxn; j++)
                if (sfxq[j].at_ms < clock_ms + 90)
                    sfxq[j].at_ms = clock_ms + 90;
            break;
        }
    }
}
static int prev_touch_half; /* 0 none, 1 left, 2 right */
static int snd_step;        /* win jingle progress */

static void reset_round(void)
{
    p1 = -0.42f; p2 = 0.42f;
    v1 = v2 = 0;
    hop1 = hop2 = 0;
    state = 0;
    loser = 0;
    snd_step = 0;
    charged1 = charged2 = 0;
    edge1 = edge2 = 0;
    hold1_ms = hold2_ms = 0;
    combo1 = combo2 = 0;
    /* the gyoji opens the bout */
    play_clip(0);
}

static void lunge_p(int who, float power, int big)
{
    if (state != 0) return;
    if (who == 1) { v1 += power; hop1 = big ? 11 : 7; }
    else          { v2 -= power; hop2 = big ? 11 : 7; }
    msw_sound(1, MSW_WAVE_NOISE, big ? 90 : 160, big ? 120 : 55,
              big ? 38 : 22, true);
    shake = big ? 6 : 3;
}
static void lunge(int who) { lunge_p(who, 1.55f, 0); }

/* a fresh press: rapid presses build a combo that hits a little harder */
static void press(int who)
{
    int *combo = (who == 1) ? &combo1 : &combo2;
    int *last = (who == 1) ? &lastpress1_ms : &lastpress2_ms;
    if (clock_ms - *last <= 300) { if (*combo < 5) (*combo)++; }
    else *combo = 0;
    *last = clock_ms;
    lunge_p(who, 1.55f * (1.0f + 0.22f * (float) *combo), 0);
}

static void physics(float dt)
{
    if (state != 0) return;
    /* friction */
    float f = 1.0f - 3.0f * dt;
    if (f < 0) f = 0;
    v1 *= f; v2 *= f;
    p1 += v1 * dt;
    p2 += v2 * dt;
    /* contact: bodies are about 0.35 ring units wide */
    float gap = p2 - p1;
    if (gap < 0.35f)
    {
        float mid = (p1 + p2) * 0.5f;
        p1 = mid - 0.175f;
        p2 = mid + 0.175f;
        /* compare push strengths as magnitudes: v1 rightward, v2 leftward.
         * (Comparing v1 > v2 directly made red win almost every contact.) */
        float s1 = v1, s2 = -v2;
        if (s1 > s2 + 0.05f)
        {
            v2 = v1 * 0.92f;
            v1 *= 0.35f;
            if (clock_ms - last_hit_ms > 180)
            {
                msw_sound(2, MSW_WAVE_NOISE, 320, 110, 58, true);
                msw_sound(3, MSW_WAVE_SQUARE, 160, 80, 34, true);
                last_hit_ms = clock_ms;
            }
            shake = 4;
        }
        else if (s2 > s1 + 0.05f)
        {
            v1 = v2 * 0.92f;
            v2 *= 0.35f;
            if (clock_ms - last_hit_ms > 180)
            {
                msw_sound(2, MSW_WAVE_NOISE, 320, 110, 58, true);
                msw_sound(3, MSW_WAVE_SQUARE, 160, 80, 34, true);
                last_hit_ms = clock_ms;
            }
            shake = 4;
        }
        else
        {
            v1 *= 0.5f;
            v2 *= 0.5f;
        }
    }
    /* at the edge but still in: the referee's "nokotta" call */
    if ((p1 < -0.85f || p1 > 0.85f) && !edge1)
    {
        edge1 = 1;
        play_clip(1);
    }
    if (p1 > -0.6f && p1 < 0.6f) edge1 = 0;
    if ((p2 < -0.85f || p2 > 0.85f) && !edge2)
    {
        edge2 = 1;
        play_clip(1);
    }
    if (p2 > -0.6f && p2 < 0.6f) edge2 = 0;
    /* ring out */
    if (p1 < -1.04f || p1 > 1.04f || p2 < -1.04f || p2 > 1.04f)
    {
        loser = (p1 < -1.04f || p1 > 1.04f) ? 1 : 2;
        if (loser == 1) win2++; else win1++;
        state = 1;
        fin_timer = 170;
        snd_step = 0;
        /* drum thumps and a little fanfare for the finish */
        sfx_add(0, 1, MSW_WAVE_NOISE, 70, 200, 40);
        sfx_add(260, 1, MSW_WAVE_NOISE, 58, 280, 44);
        play_clip(loser == 1 ? 3 : 2);   /* "ao no kachi" / "aka no kachi" */
    }
}

/* ---- drawing ------------------------------------------------------------- */

static void draw_ellipse(int cx, int cy, int rx, int ry, uint16_t col)
{
    for (int dy = -ry; dy <= ry; dy++)
    {
        float t = (float) dy / (float) ry;
        int hw = (int) (rx * sqrtf(1.0f - t * t));
        msw_gfx_fill_rect(fb, cx - hw, cy + dy, hw * 2, 1, col);
    }
}

static void draw_ellipse_outline(int cx, int cy, int rx, int ry, uint16_t col)
{
    for (int i = 0; i < 128; i++)
    {
        float a = (float) i / 128.0f * 6.28318f;
        int x = cx + (int) (rx * cosf(a));
        int y = cy + (int) (ry * sinf(a));
        msw_gfx_fill_rect(fb, x - 2, y - 2, 4, 4, col);
    }
}

static void draw_rikishi(int sx, int base_y, int face, uint16_t mawashi,
                         int hop, int fallen, int crouch)
{
    int lift = 0;
    if (hop > 0) lift = (hop > 3) ? (7 - hop) * 2 : hop * 2;
    int y = base_y - lift;
    if (fallen) y += 16;
    if (crouch > 0) y += 5;                       /* sinking to charge */
    if (crouch > 1 && (clock_ms / 150) % 2 == 0) mawashi = COL_WHITE;
    int lean = face * (hop > 0 ? 4 : 0) + (fallen ? face * -10 : 0);
    /* legs */
    msw_gfx_fill_rect(fb, sx - 16, y - 8, 12, 10, COL_SKIN);
    msw_gfx_fill_rect(fb, sx + 5, y - 8, 12, 10, COL_SKIN);
    /* body */
    msw_gfx_fill_circle(fb, sx + lean / 2, y - 30, 25, COL_SKIN);
    msw_gfx_fill_circle(fb, sx + lean / 2 + face * 3, y - 24, 16, COL_SKIN2);
    /* mawashi */
    msw_gfx_fill_rect(fb, sx - 24 + lean / 2, y - 26, 48, 11, mawashi);
    /* arms */
    msw_gfx_fill_circle(fb, sx - 26 + lean / 2, y - 32, 8, COL_SKIN);
    msw_gfx_fill_circle(fb, sx + 26 + lean / 2, y - 32, 8, COL_SKIN);
    /* head */
    int hx = sx + lean, hy = y - 62;
    msw_gfx_fill_circle(fb, hx, hy, 13, COL_SKIN);
    msw_gfx_fill_rect(fb, hx - 13, hy - 13, 26, 8, COL_HAIR);
    msw_gfx_fill_rect(fb, hx - 3, hy - 20, 6, 8, COL_HAIR);   /* topknot */
    /* eyes, looking toward the opponent (or X X when fallen) */
    if (fallen)
    {
        msw_gfx_pixel(fb, hx + face * 4 - 1, hy, COL_DARK);
        msw_gfx_pixel(fb, hx + face * 4 + 1, hy, COL_DARK);
        msw_gfx_pixel(fb, hx + face * 4, hy - 1, COL_DARK);
        msw_gfx_pixel(fb, hx + face * 4, hy + 1, COL_DARK);
    }
    else
    {
        msw_gfx_fill_rect(fb, hx + face * 5 - 2, hy - 2, 3, 4, COL_DARK);
        msw_gfx_fill_rect(fb, hx + face * 5 + 3, hy - 2, 3, 4, COL_DARK);
    }
}

static void draw(void)
{
    msw_gfx_fill(fb, COL_BG);
    int shx = 0;
    if (shake > 0) shx = (shake % 2) ? 2 : -2;
    /* dohyo: straw base, clay top, white ring line */
    draw_ellipse(CX + shx, CY + 10, RX + 14, RY + 10, COL_STRAW);
    draw_ellipse(CX + shx, CY, RX + 8, RY + 4, COL_CLAY);
    draw_ellipse_outline(CX + shx, CY, RX, RY, COL_RING);
    draw_ellipse_outline(CX + shx, CY, RX - 10, RY - 6, COL_CLAY);

    /* the gyoji, fan raised while he is calling */
    {
        int gx2 = CX + shx, gy = CY - RY + 18;
        int fanup = (cur_on || state == 1);
        msw_gfx_fill_rect(fb, gx2 - 10, gy - 26, 20, 24, MSW_RGB(72, 62, 148));
        msw_gfx_fill_rect(fb, gx2 - 10, gy - 14, 20, 3, COL_WHITE);
        msw_gfx_fill_circle(fb, gx2, gy - 34, 9, COL_SKIN);
        msw_gfx_fill_rect(fb, gx2 - 5, gy - 47, 10, 6, COL_HAIR);
        if (fanup)
        {
            msw_gfx_fill_rect(fb, gx2 + 12, gy - 46, 5, 14, MSW_RGB(150, 110, 60));
            msw_gfx_fill_circle(fb, gx2 + 14, gy - 49, 7, MSW_RGB(150, 110, 60));
        }
        else
        {
            msw_gfx_fill_rect(fb, gx2 + 12, gy - 28, 5, 12, MSW_RGB(150, 110, 60));
            msw_gfx_fill_circle(fb, gx2 + 14, gy - 30, 7, MSW_RGB(150, 110, 60));
        }
    }
    int x1 = CX + (int) (p1 * POS2PX) + shx;
    int x2 = CX + (int) (p2 * POS2PX) + shx;
    int h1 = hop1, h2 = hop2;
    if (state == 1)
    {
        int el = 170 - fin_timer;
        int bounce = (el % 20 < 10) ? 6 : 2;
        if (loser == 2) h1 = bounce; else h2 = bounce;
        static const uint16_t conf[4] = { COL_RED, COL_BLUE, COL_WHITE,
                                          MSW_RGB(244, 180, 60) };
        for (int i = 0; i < 24; i++)
        {
            int cxp = (i * 67 + 23) % SCR;
            int cyp = (el * 3 + i * 37) % 320;
            msw_gfx_fill_rect(fb, cxp, cyp, 5, 5, conf[i % 4]);
        }
    }
    draw_rikishi(x1, BASE_Y, 1, COL_RED, h1, state == 1 && loser == 1,
                 charged1 ? 2 : (hold1_ms > 60 ? 1 : 0));
    draw_rikishi(x2, BASE_Y, -1, COL_BLUE, h2, state == 1 && loser == 2,
                 charged2 ? 2 : (hold2_ms > 60 ? 1 : 0));

    /* charge gauge and combo pips, so the techniques are visible */
    for (int s = 0; s < 2; s++)
    {
        int gx = (s == 0) ? x1 : x2;
        int hold = (s == 0) ? hold1_ms : hold2_ms;
        int chg = (s == 0) ? charged1 : charged2;
        int cmb = (s == 0) ? combo1 : combo2;
        int lastp = (s == 0) ? lastpress1_ms : lastpress2_ms;
        uint16_t gc = (s == 0) ? COL_RED : COL_BLUE;
        if (hold > 60 || chg)
        {
            int fill = chg ? 56 : (hold * 56) / 400;
            if (fill > 56) fill = 56;
            msw_gfx_fill_rect(fb, gx - 28, BASE_Y + 16, 56, 7, COL_DARK);
            msw_gfx_fill_rect(fb, gx - 28, BASE_Y + 16, fill, 7,
                              chg ? COL_WHITE : gc);
        }
        if (chg)
        {
            const char *t = "たまった";
            int tw2 = msw_text_width(t, 0);
            msw_gfx_text(fb, gx - tw2 / 2, BASE_Y - 122, t, COL_WHITE, 0);
        }
        else if (cmb > 0 && clock_ms - lastp <= 600)
        {
            for (int i = 0; i < cmb; i++)
                msw_gfx_fill_rect(fb, gx - cmb * 6 + i * 12,
                                  BASE_Y - 112, 8, 8, gc);
        }
    }

    /* title and score */
    msw_gfx_text(fb, CX - 40, 26, "かみずもう", COL_WHITE, 0);
    char buf[32];
    snprintf(buf, sizeof buf, "%d - %d", win1, win2);
    int w = msw_text_width(buf, 20);
    msw_gfx_text(fb, CX - w / 2, 58, buf, COL_WHITE, 20);
    msw_gfx_text(fb, 62, 58, "あか", COL_RED, 0);
    msw_gfx_text(fb, CX + 118, 58, "あお", COL_BLUE, 0);

    if (state == 1)
    {
        const char *msg = (loser == 1) ? "あお の かち" : "あか の かち";
        int tw = msw_text_width(msg, 0);
        msw_gfx_text(fb, CX - tw / 2, 108, msg, COL_WHITE, 0);
    }
    msw_fb_present();
}

/* ---- input + tick --------------------------------------------------------- */

static void tick(uint32_t dt_ms_u, void *user)
{
    (void) user;
    int dt_ms = (int) dt_ms_u;
    float dt = dt_ms / 1000.0f;
    if (dt > 0.06f) dt = 0.06f;

    int y = msw_pin_read(MSW_PIN_YELLOW);
    int w = msw_pin_read(MSW_PIN_WHITE);
    static int ly = -99, lw = -99;
    if (y != ly || w != lw)
    {
        msw_log("kamisumo: pins yellow=%d white=%d", y, w);
        ly = y; lw = w;
    }
    clock_ms += dt_ms;
    sfx_tick();
    if (y == 0 && prev_y != 0) { press(1); hold1_ms = 0; }
    else if (y == 0)
    {
        hold1_ms += dt_ms;
        if (hold1_ms >= 400) charged1 = 1;
    }
    else if (y == 1 && prev_y == 0)
    {
        if (charged1) lunge_p(1, 5.4f, 1);   /* charged dash on release */
        charged1 = 0; hold1_ms = 0;
    }
    if (w == 0 && prev_w != 0) { press(2); hold2_ms = 0; }
    else if (w == 0)
    {
        hold2_ms += dt_ms;
        if (hold2_ms >= 400) charged2 = 1;
    }
    else if (w == 1 && prev_w == 0)
    {
        if (charged2) lunge_p(2, 5.4f, 1);
        charged2 = 0; hold2_ms = 0;
    }
    if (y >= 0) prev_y = y;
    if (w >= 0) prev_w = w;

    /* touch fallback: a fresh touch on a half counts as that side's press */
    int tx, ty, half = 0;
    if (msw_touch(&tx, &ty)) half = (tx < CX) ? 1 : 2;
    if (half != 0 && prev_touch_half == 0) lunge(half);
    prev_touch_half = half;

    /* feed the gyoji voice clip to the PCM buffer */
    if (cur_on && cur_pcm && cur_pos < cur_len)
    {
        int wpcm = msw_pcm_write(cur_pcm + cur_pos, cur_len - cur_pos);
        if (wpcm > 0) cur_pos += wpcm;
        if (cur_pos >= cur_len) cur_on = 0;
    }
    physics(dt);
    if (hop1 > 0) hop1--;
    if (hop2 > 0) hop2--;
    if (shake > 0) shake--;

    if (state == 1)
    {
        if (snd_step > 0 && snd_step <= 3 && (fin_timer % 14) == 0)
        {
            static const int notes[3] = { 523, 659, 784 };
            msw_sound(0, MSW_WAVE_SQUARE, notes[snd_step - 1], 130, 24, true);
            snd_step++;
        }
        if (--fin_timer <= 0) reset_round();
    }
    draw();
}

int app_main(int argc, char **argv)
{
    (void) argc; (void) argv;
    fb = msw_fb();
    if (!fb) return 1;
    /* Most PORT.A units run on the 5V pin; without it the button lines
     * stay low and read as pressed forever (SDK header, 2026-11-14). */
    bool pw = msw_port_power(true);
    msw_log("kamisumo: port power %s", pw ? "on" : "FAILED");
    {
        static const char *names[4] = { "hakkeyoi.pcm", "nokotta.pcm",
                                        "akakachi.pcm", "aokakachi.pcm" };
        for (int i = 0; i < 4; i++)
        {
            size_t vsz = 0;
            const void *vd = msw_app_data(names[i], &vsz);
            if (vd) { clip_pcm[i] = (const int16_t *) vd; clip_len[i] = (int) (vsz / 2); }
        }
        msw_log("kamisumo: voice clips %d %d %d %d", clip_len[0], clip_len[1],
                clip_len[2], clip_len[3]);
    }
    prev_y = prev_w = 1;
    prev_touch_half = 0;
    reset_round();
    msw_set_fps(30);
    msw_on_tick(tick, NULL);
    msw_log("kamisumo: ready");
    return 0;
}
