/*
 * おちものブロック: a falling-block puzzle for the museStopWatch.
 *
 * Test app 1 of the mini-app runtime (frame drawing, touch buttons, sound,
 * storage). Controls on the screen: the pad on the left moves (left, right,
 * down = soft drop, up = rotate left), A rotates right, B drops at once.
 * Yellow button: the app menu (go on / quit).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "msw_app.h"

#define W 10
#define H 20
#define CELL 16
#define WX 153                     /* the well's top left */
#define WY 58

enum { TITLE, PLAY, PAUSE, OVER };
enum { B_LEFT, B_RIGHT, B_UP, B_DOWN, B_A, B_B, NBTN };

typedef struct {
    int x, y, r;
    const char *label;
} button_t;

/* Buttons along the rim, lower left and lower right (trial, 2026-11-08):
 * the thumbs cover the least of the play area there. Each sits 192 px
 * from the centre at the angle noted (0 = right, 90 = down). */
static const button_t s_btn[NBTN] = {
    { 59, 314, 23, LV_SYMBOL_LEFT },     /* 155 */
    { 152, 407, 23, LV_SYMBOL_RIGHT },   /* 115 */
    { 407, 314, 23, LV_SYMBOL_REFRESH }, /*  25: turn the other way */
    { 97, 369, 23, LV_SYMBOL_DOWN },     /* 135 */
    { 369, 369, 26, "A" },               /*  45: turn */
    { 314, 407, 26, "B" },               /*  65: drop */
};

/* The seven pieces, each rotation a 4 x 4 mask (bit 15 = top left). */
static const uint16_t s_shape[7][4] = {
    { 0x0F00, 0x2222, 0x00F0, 0x4444 },
    { 0x8E00, 0x6440, 0x0E20, 0x44C0 },
    { 0x2E00, 0x4460, 0x0E80, 0xC440 },
    { 0x6600, 0x6600, 0x6600, 0x6600 },
    { 0x6C00, 0x4620, 0x06C0, 0x8C40 },
    { 0x4E00, 0x4640, 0x0E40, 0x4C40 },
    { 0xC600, 0x2640, 0x0C60, 0x4C80 },
};

static uint16_t s_color[8], s_light[8], s_dark[8];
static msw_gfx_t *fb;
static uint8_t s_well[H][W];
static int s_state = TITLE;
static int s_kind, s_rot, s_px, s_py, s_next;
static uint8_t s_bag[7];
static int s_bag_left;
static int s_score, s_lines, s_level, s_best;
static int s_fall_ms, s_lock_ms;
static bool s_down[NBTN];
static int s_held_ms[NBTN];
static int s_clear_rows[4], s_nclear, s_clear_ms;
static bool s_dirty = true;
static int s_music_pos, s_music_ms;
static bool s_music = true;

/* ---- sound ---------------------------------------------------------------- */

static int hz(int midi)
{
    return (int)(440.0f * powf(2.0f, (midi - 69) / 12.0f) + 0.5f);
}

/* An original tune in A minor: note (MIDI, 0 = rest) and length in eighths. */
static const int8_t s_tune[][2] = {
    { 69, 2 }, { 72, 1 }, { 76, 1 }, { 74, 2 }, { 72, 1 }, { 71, 1 },
    { 69, 2 }, { 64, 2 }, { 65, 1 }, { 69, 1 }, { 72, 2 },
    { 71, 1 }, { 69, 1 }, { 67, 2 }, { 64, 2 }, { 0, 2 },
    { 65, 2 }, { 69, 1 }, { 72, 1 }, { 77, 2 }, { 76, 1 }, { 74, 1 },
    { 72, 2 }, { 71, 1 }, { 72, 1 }, { 74, 2 }, { 76, 2 },
    { 72, 2 }, { 69, 2 }, { 69, 2 }, { 0, 2 },
};
#define TUNE_LEN ((int)(sizeof(s_tune) / sizeof(s_tune[0])))
#define EIGHTH_MS 190

static void music(uint32_t dt)
{
    if (!s_music || s_state != PLAY) {
        return;
    }
    s_music_ms -= (int)dt;
    if (s_music_ms > 0) {
        return;
    }
    const int8_t *n = s_tune[s_music_pos];
    int len = n[1] * EIGHTH_MS;
    if (n[0]) {
        msw_sound(0, MSW_WAVE_TRIANGLE, hz(n[0]), len - 30, 22, true);
    }
    if (s_music_pos % 2 == 0) {
        msw_sound(3, MSW_WAVE_SQUARE, hz((n[0] ? n[0] : 69) - 24), len - 20, 8, true);
    }
    s_music_ms += len;
    s_music_pos = (s_music_pos + 1) % TUNE_LEN;
}

static void sfx_move(void) { msw_sound(1, MSW_WAVE_SQUARE, 880, 18, 20, false); }
static void sfx_rotate(void) { msw_sound(1, MSW_WAVE_SQUARE, 1320, 25, 20, true); }
static void sfx_lock(void) { msw_sound(2, MSW_WAVE_NOISE, 1800, 60, 30, true); }
static void sfx_drop(void) { msw_sound_slide(2, MSW_WAVE_SQUARE, 900, 200, 90, 30); }

static void sfx_clear(int n)
{
    msw_sound_slide(1, MSW_WAVE_SQUARE, 600, 600 + 300 * n, 120 + 60 * n, 35);
    msw_sound(2, MSW_WAVE_NOISE, 4000, 200, 25, true);
    msw_vibrate(40 + 30 * n, 60);
}

/* ---- the game --------------------------------------------------------------- */

static bool cell(uint16_t mask, int r, int c)
{
    return mask & (0x8000 >> (r * 4 + c));
}

static bool fits(int kind, int rot, int px, int py)
{
    uint16_t m = s_shape[kind][rot & 3];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!cell(m, r, c)) {
                continue;
            }
            int x = px + c, y = py + r;
            if (x < 0 || x >= W || y >= H || (y >= 0 && s_well[y][x])) {
                return false;
            }
        }
    }
    return true;
}

static int next_from_bag(void)
{
    if (!s_bag_left) {
        for (int i = 0; i < 7; i++) {
            s_bag[i] = (uint8_t)i;
        }
        for (int i = 6; i > 0; i--) {
            int j = (int)(msw_random() % (uint32_t)(i + 1));
            uint8_t t = s_bag[i];
            s_bag[i] = s_bag[j];
            s_bag[j] = t;
        }
        s_bag_left = 7;
    }
    return s_bag[--s_bag_left];
}

static int fall_interval(void)
{
    int ms = 800 - s_level * 70;
    return ms < 80 ? 80 : ms;
}

static void game_over(void)
{
    s_state = OVER;
    msw_sound_stop(-1);
    msw_sound_slide(1, MSW_WAVE_SAW, 500, 80, 900, 35);
    msw_vibrate(300, 80);
    if (s_score > s_best) {
        s_best = s_score;
        msw_store_put("best", &s_best, sizeof(s_best));
    }
}

static void spawn(void)
{
    s_kind = s_next;
    s_next = next_from_bag();
    s_rot = 0;
    s_px = 3;
    s_py = -1;
    s_fall_ms = fall_interval();
    s_lock_ms = 0;
    if (!fits(s_kind, s_rot, s_px, s_py)) {
        game_over();
    }
}

static void new_game(void)
{
    memset(s_well, 0, sizeof(s_well));
    s_score = s_lines = s_level = 0;
    s_bag_left = 0;
    s_nclear = 0;
    s_music_pos = 0;
    s_music_ms = 0;
    s_next = next_from_bag();
    s_state = PLAY;
    spawn();
}

static void lock_piece(void)
{
    uint16_t m = s_shape[s_kind][s_rot & 3];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (cell(m, r, c) && s_py + r >= 0) {
                s_well[s_py + r][s_px + c] = (uint8_t)(s_kind + 1);
            }
        }
    }
    s_nclear = 0;
    for (int y = 0; y < H; y++) {
        bool full = true;
        for (int x = 0; x < W && full; x++) {
            full = s_well[y][x] != 0;
        }
        if (full) {
            s_clear_rows[s_nclear++] = y;
        }
    }
    if (s_nclear) {
        static const int points[] = { 0, 100, 300, 500, 800 };
        s_score += points[s_nclear] * (s_level + 1);
        s_lines += s_nclear;
        s_level = s_lines / 10;
        s_clear_ms = 260;
        sfx_clear(s_nclear);
    } else {
        sfx_lock();
        spawn();
    }
}

static void finish_clear(void)
{
    for (int i = 0; i < s_nclear; i++) {
        int row = s_clear_rows[i];
        memmove(&s_well[1][0], &s_well[0][0], (size_t)row * W);
        memset(&s_well[0][0], 0, W);
    }
    s_nclear = 0;
    spawn();
}

static bool try_move(int dx, int dy, int drot)
{
    int rot = (s_rot + drot) & 3;
    static const int kicks[] = { 0, -1, 1, -2, 2 };
    for (int k = 0; k < (drot ? 5 : 1); k++) {
        if (fits(s_kind, rot, s_px + dx + kicks[k], s_py + dy)) {
            s_px += dx + kicks[k];
            s_py += dy;
            s_rot = rot;
            if (dy == 0) {
                s_lock_ms = 0;     /* moving on the floor gives a little more time */
            }
            return true;
        }
    }
    return false;
}

static int ghost_y(void)
{
    int y = s_py;
    while (fits(s_kind, s_rot, s_px, y + 1)) {
        y++;
    }
    return y;
}

/* ---- input ------------------------------------------------------------------ */

static int button_at(int x, int y)
{
    int best = -1, best_d = 1 << 30;
    for (int i = 0; i < NBTN; i++) {
        int dx = x - s_btn[i].x, dy = y - s_btn[i].y, d = dx * dx + dy * dy;
        int reach = s_btn[i].r + 14;
        if (d < reach * reach && d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

static void press(int b)
{
    if (s_state != PLAY || s_nclear) {
        return;
    }
    switch (b) {
    case B_LEFT:
        if (try_move(-1, 0, 0)) sfx_move();
        break;
    case B_RIGHT:
        if (try_move(1, 0, 0)) sfx_move();
        break;
    case B_DOWN:
        if (try_move(0, 1, 0)) {
            s_score += 1;
            s_fall_ms = fall_interval();
        }
        break;
    case B_UP:
        if (try_move(0, 0, 3)) sfx_rotate();
        break;
    case B_A:
        if (try_move(0, 0, 1)) sfx_rotate();
        break;
    case B_B: {
        int y = ghost_y();
        s_score += 2 * (y - s_py);
        s_py = y;
        sfx_drop();
        msw_vibrate(25, 50);
        lock_piece();
        break;
    }
    }
    s_dirty = true;
}

static void read_touch(uint32_t dt)
{
    int tx, ty;
    int b = msw_touch(&tx, &ty) ? button_at(tx, ty) : -1;
    bool touched = b >= 0 || msw_touch(NULL, NULL);
    static bool was_touched;
    if (s_state != PLAY && touched && !was_touched) {
        if (s_state == PAUSE) {
            s_state = PLAY;
        } else {
            new_game();
        }
        s_dirty = true;
    }
    was_touched = touched;
    for (int i = 0; i < NBTN; i++) {
        bool now = i == b;
        if (now && !s_down[i]) {
            press(i);
            s_held_ms[i] = 0;
        } else if (now) {
            s_held_ms[i] += (int)dt;
            int first = i == B_DOWN ? 60 : 170, every = i == B_DOWN ? 40 : 55;
            if ((i == B_LEFT || i == B_RIGHT || i == B_DOWN) && s_held_ms[i] >= first) {
                press(i);
                s_held_ms[i] -= every;
            }
        }
        if (now != s_down[i]) {
            s_dirty = true;
        }
        s_down[i] = now;
    }
}

/* ---- drawing ----------------------------------------------------------------- */

static void block(int x, int y, int k, bool ghost)
{
    if (ghost) {
        msw_gfx_rect(fb, x + 1, y + 1, CELL - 2, CELL - 2, s_dark[k]);
        return;
    }
    msw_gfx_fill_rect(fb, x, y, CELL, CELL, s_dark[k]);
    msw_gfx_fill_rect(fb, x, y, CELL - 2, CELL - 2, s_light[k]);
    msw_gfx_fill_rect(fb, x + 2, y + 2, CELL - 4, CELL - 4, s_color[k]);
}

static void text_center(int cx, int y, const char *s, uint16_t c, int size)
{
    msw_gfx_text(fb, cx - msw_text_width(s, size) / 2, y, s, c, size);
}

static void draw(void)
{
    msw_gfx_fill(fb, MSW_RGB(8, 10, 18));
    /* the well */
    msw_gfx_fill_rect(fb, WX - 4, WY - 4, W * CELL + 8, H * CELL + 8, MSW_RGB(70, 80, 110));
    msw_gfx_fill_rect(fb, WX, WY, W * CELL, H * CELL, MSW_RGB(16, 18, 30));
    for (int x = 1; x < W; x++) {
        msw_gfx_fill_rect(fb, WX + x * CELL, WY, 1, H * CELL, MSW_RGB(24, 27, 42));
    }
    for (int y = 0; y < H; y++) {
        bool flash = false;
        for (int i = 0; i < s_nclear; i++) {
            flash |= s_clear_rows[i] == y;
        }
        for (int x = 0; x < W; x++) {
            if (flash && (s_clear_ms / 50) % 2 == 0) {
                msw_gfx_fill_rect(fb, WX + x * CELL, WY + y * CELL, CELL, CELL, MSW_RGB(255, 255, 255));
            } else if (s_well[y][x]) {
                block(WX + x * CELL, WY + y * CELL, s_well[y][x], false);
            }
        }
    }
    if (s_state == PLAY && !s_nclear) {
        uint16_t m = s_shape[s_kind][s_rot];
        int gy = ghost_y();
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (!cell(m, r, c)) {
                    continue;
                }
                if (gy + r >= 0) {
                    block(WX + (s_px + c) * CELL, WY + (gy + r) * CELL, s_kind + 1, true);
                }
                if (s_py + r >= 0) {
                    block(WX + (s_px + c) * CELL, WY + (s_py + r) * CELL, s_kind + 1, false);
                }
            }
        }
    }
    /* next piece and numbers */
    msw_gfx_text(fb, 330, 92, "NEXT", MSW_RGB(150, 160, 190), 14);
    if (s_state != TITLE) {
        uint16_t m = s_shape[s_next][0];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (cell(m, r, c)) {
                    msw_gfx_fill_rect(fb, 330 + c * 12, 114 + r * 12, 11, 11, s_color[s_next + 1]);
                }
            }
        }
    }
    char buf[32];
    msw_gfx_text(fb, 330, 168, "SCORE", MSW_RGB(150, 160, 190), 14);
    snprintf(buf, sizeof(buf), "%d", s_score);
    msw_gfx_text(fb, 330, 186, buf, MSW_RGB(255, 255, 255), 20);
    snprintf(buf, sizeof(buf), "LV %d", s_level + 1);
    msw_gfx_text(fb, 64, 96, buf, MSW_RGB(255, 220, 120), 20);
    snprintf(buf, sizeof(buf), "LINES %d", s_lines);
    msw_gfx_text(fb, 56, 124, buf, MSW_RGB(150, 160, 190), 14);
    snprintf(buf, sizeof(buf), "BEST %d", s_best);
    msw_gfx_text(fb, 56, 144, buf, MSW_RGB(150, 160, 190), 14);
    /* buttons */
    for (int i = 0; i < NBTN; i++) {
        const button_t *b = &s_btn[i];
        uint16_t face = s_down[i] ? MSW_RGB(120, 140, 200) : i >= B_A ? MSW_RGB(70, 50, 90) : MSW_RGB(50, 60, 80);
        msw_gfx_fill_circle(fb, b->x, b->y, b->r, face);
        msw_gfx_circle(fb, b->x, b->y, b->r, MSW_RGB(140, 150, 180));
        text_center(b->x, b->y - 11, b->label, MSW_RGB(255, 255, 255), 20);
    }
    /* messages */
    if (s_state != PLAY) {
        msw_gfx_fill_rect(fb, WX - 10, 180, W * CELL + 20, 92, MSW_RGB(0, 0, 0));
        msw_gfx_rect(fb, WX - 10, 180, W * CELL + 20, 92, MSW_RGB(255, 220, 120));
        const char *title = s_state == TITLE ? "おちものブロック" : s_state == PAUSE ? "ポーズ" : "ゲームオーバー";
        text_center(WX + W * CELL / 2, 196, title, MSW_RGB(255, 220, 120), 0);
        text_center(WX + W * CELL / 2, 230, "タップでスタート", MSW_RGB(220, 220, 220), 0);
    }
    msw_fb_present_rect(24, 40, 418, 360);
}

/* ---- the callbacks -------------------------------------------------------------- */

static void tick(uint32_t dt, void *user)
{
    (void)user;
    read_touch(dt);
    music(dt);
    if (s_state == PLAY) {
        if (s_nclear) {
            s_clear_ms -= (int)dt;
            if (s_clear_ms <= 0) {
                finish_clear();
            }
            s_dirty = true;
        } else if (fits(s_kind, s_rot, s_px, s_py + 1)) {
            s_lock_ms = 0;
            s_fall_ms -= (int)dt;
            if (s_fall_ms <= 0) {
                s_py++;
                s_fall_ms += fall_interval();
                s_dirty = true;
            }
        } else {
            s_lock_ms += (int)dt;
            if (s_lock_ms >= 450) {
                lock_piece();
                s_dirty = true;
            }
        }
    }
    if (s_dirty) {
        draw();
        s_dirty = false;
    }
}

static uint16_t shade(int r, int g, int b, int pct)
{
    r = r * pct / 100;
    g = g * pct / 100;
    b = b * pct / 100;
    return MSW_RGB(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
}

int app_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    static const uint8_t rgb[8][3] = {
        { 0, 0, 0 }, { 41, 182, 246 }, { 66, 99, 235 }, { 255, 152, 0 },
        { 255, 202, 40 }, { 102, 187, 106 }, { 171, 71, 188 }, { 239, 83, 80 },
    };
    for (int i = 1; i < 8; i++) {
        s_color[i] = MSW_RGB(rgb[i][0], rgb[i][1], rgb[i][2]);
        s_light[i] = shade(rgb[i][0] + 60, rgb[i][1] + 60, rgb[i][2] + 60, 100);
        s_dark[i] = shade(rgb[i][0], rgb[i][1], rgb[i][2], 55);
    }
    fb = msw_fb();
    if (!fb) {
        return 1;
    }
    if (msw_store_get("best", &s_best, sizeof(s_best)) != sizeof(s_best)) {
        s_best = 0;
    }
    msw_set_fps(30);
    msw_on_tick(tick, NULL);
    draw();
    return 0;
}
