/*
 * ぴょこの森: a side-scrolling jump game for the museStopWatch.
 *
 * Test app 2 of the mini-app runtime (sprites from text art, scrolling,
 * physics, touch buttons, sound). All art and music here are original.
 * Pyoko, a leaf-topped sprout, runs to the flag: collect seeds, hit crystal
 * blocks from below, jump on the spiky rollers.
 * Move, two ways: put a finger on the pad at the lower left and slide it
 * left or right (the point where it went down is the middle), or hold the
 * tilt button (at 10 o'clock) and tilt the watch (the tilt when
 * it went down is level). Blue rim button (upper right): jump.
 * Yellow button: the app menu (go on / quit).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "msw_app.h"

#define TILE 24          /* 8 x 8 art drawn 3 times the size */
#define ROWS 16
#define OY 42            /* the map's top on the screen */
#define SCR 466
#define MAX_ENEMIES 16

/* The level: '#' ground, 'B' brick, 'Q' crystal block, '*' seed,
 * 'E' a roller starts here, 'F' the goal flag, '=' a ledge. Short rows are
 * padded with sky. */
static const char *const s_map[ROWS] = {
    "",
    "",
    "",
    "",
    "                                                                                                          ",
    "                         ***                                    * * *                                     ",
    "                                              BQB                                                         ",
    "              Q        =====         ***                       =====       Q Q Q                          ",
    "                                                         ===                                    ===   F   ",
    "         * *                  BBQBB          E     ====                *          ==    ===           #   ",
    "                     E                                                       BB                       #   ",
    "     *        ###          E           E   ####         E    E                 E          E    E       #  ",
    "             ####                     ###########                ###                ####             ###  ",
    "###########################   ######################   ###############   ###############################  ",
    "###########################   ######################   ###############   ################################",
    "###########################   ######################   ###############   ################################",
};

static char s_tiles[ROWS][128];
static int s_cols;

/* ---- art ----------------------------------------------------------------- */

static const msw_pal_t s_pal[] = {
    { 'L', MSW_RGB(124, 179, 66) }, { 'G', MSW_RGB(76, 175, 80) }, { 'W', MSW_RGB(255, 255, 255) },
    { 'K', MSW_RGB(27, 27, 27) }, { 'P', MSW_RGB(244, 143, 177) }, { 'F', MSW_RGB(46, 125, 50) },
    { 'g', MSW_RGB(102, 187, 106) }, { 'D', MSW_RGB(141, 99, 64) }, { 'd', MSW_RGB(115, 78, 50) },
    { 'B', MSW_RGB(198, 106, 66) }, { 'k', MSW_RGB(110, 52, 30) }, { 'C', MSW_RGB(77, 208, 225) },
    { 'c', MSW_RGB(0, 151, 167) }, { 'Y', MSW_RGB(255, 213, 79) }, { 'y', MSW_RGB(255, 160, 0) },
    { 'R', MSW_RGB(126, 87, 194) }, { 'S', MSW_RGB(224, 224, 224) }, { 'u', MSW_RGB(120, 110, 130) },
    { 0, 0 },
};

static const char *const ART_PYOKO[2][8] = {
    { "...LL...", "..GGGG..", ".GGGGGG.", ".GWKGWK.", ".GGGGGG.", ".GGPPGG.", "..GGGG..", ".FF..FF." },
    { "...LL...", "..GGGG..", ".GGGGGG.", ".GWKGWK.", ".GGGGGG.", ".GGPPGG.", "..GGGG..", "..FFFF.." },
};
static const char *const ART_JUMP[8] = {
    "..LL....", "..GGGG..", ".GGGGGG.", ".GWKGWK.", ".GGGGGG.", "FGGPPGGF", "..GGGG..", "........",
};
static const char *const ART_ROLLER[2][8] = {
    { "...S....", ".S.RR.S.", "..RRRR..", "SRWKRWKS", "..RRRR..", ".S.RR.S.", "...S....", "........" },
    { "....S...", ".S.RR.S.", "..RRRR..", "SRKWRKWS", "..RRRR..", ".S.RR.S.", "....S...", "........" },
};
static const char *const ART_FLAT[8] = {
    "........", "........", "........", "........", "........", ".S.RR.S.", "SRKRRKRS", "RRRRRRRR",
};
static const char *const ART_GRASS[8] = {
    "gGgGGgGg", "GGGGGGGG", "DdDDDdDD", "DDDdDDDD", "DDDDDdDD", "DdDDDDDD", "DDDDdDDD", "DDdDDDDD",
};
static const char *const ART_DIRT[8] = {
    "DDDdDDDD", "DdDDDDdD", "DDDDDDDD", "DDdDDDDD", "DDDDDdDD", "dDDDDDDD", "DDDDdDDD", "DdDDDDDd",
};
static const char *const ART_BRICK[8] = {
    "BBBkBBBB", "BBBkBBBB", "kkkkkkkk", "BBBBBBBk", "BBBBBBBk", "kkkkkkkk", "BBBkBBBB", "BBBkBBBB",
};
static const char *const ART_CRYSTAL[8] = {
    "cccccccc", "cCCWCCCc", "cCWCCCCc", "cCCCCWCc", "cCCCWCCc", "cCWCCCCc", "cCCCCCCc", "cccccccc",
};
static const char *const ART_USED[8] = {
    "uuuuuuuu", "uSuuuuSu", "uuuuuuuu", "uuuuuuuu", "uuuuuuuu", "uuuuuuuu", "uSuuuuSu", "uuuuuuuu",
};
static const char *const ART_SEED[8] = {
    "........", "...YY...", "..YWYY..", ".YYYYYy.", ".YYYYyy.", "..yyyy..", "...yy...", "........",
};
static const char *const ART_LEDGE[8] = {
    "gGgGGgGg", "GGGGGGGG", "dDDdDDDd", "........", "........", "........", "........", "........",
};

static msw_sprite_t *s_pyoko[2], *s_jump, *s_roller[2], *s_flat, *s_grass, *s_dirt, *s_brick,
    *s_crystal, *s_used, *s_seed, *s_ledge;

/* ---- state -------------------------------------------------------------------- */

enum { TITLE, PLAY, PAUSE, DEAD, CLEAR };
enum { B_SLIDE, B_TILT, NBTN };

static const struct { int x, y, r, size; const char *label; } s_btn[NBTN] = {
    /* along the rim at the lower left: 190 px from the centre at 135 degrees */
    { 99, 367, 40, 20, LV_SYMBOL_LEFT LV_SYMBOL_RIGHT },   /* touch and slide */
    { 69, 138, 28, 0, "傾き" },      /* hold and tilt: at 10 o'clock, 190 px at 210 degrees */
};
/* Slide: how far (px) from where the finger went down is full speed. */
#define SLIDE_FULL 40.0f
#define SLIDE_CURVE 1.5f
/* Tilt (sideways gravity, in g) for no move and for full speed. */
#define TILT_DEAD 0.0f
#define TILT_FULL 0.15f
/* speed = tilt ^ TILT_CURVE: slow and fine for a small tilt, fast for a big one */
#define TILT_CURVE 1.8f
/* A mark by the blue rim button (upper right, -45 degrees): jump. */
#define JUMP_MARK_X 370
#define JUMP_MARK_Y 96

typedef struct {
    float x, y, vx, vy;   /* top left, world pixels */
    bool alive, on_ground;
    int squash_ms;
} body_t;

static msw_gfx_t *fb;
static int s_state = TITLE;
static body_t s_me;
static body_t s_en[MAX_ENEMIES];
static int s_nen;
static bool s_face_left;
static int s_anim_ms, s_seeds, s_lives = 3, s_best, s_state_ms;
static float s_cam;
static bool s_btn_down[NBTN], s_jump_down, s_jump_held;
static float s_tilt, s_tilt_zero;   /* sideways gravity: smoothed, and at the press */
static float s_move;                /* -1 (left) .. 1 (right), from the pad or the tilt */
static int s_slide_x0;              /* where the finger went down on the pad */
static int s_bump_col = -1, s_bump_row, s_bump_ms;
static int s_music_pos, s_music_ms;

#define PW 18            /* Pyoko's box inside its 24 px picture */
#define PH 21
#define POX 3
#define POY 3

/* ---- sound ---------------------------------------------------------------------- */

static int hz(int midi)
{
    return (int)(440.0f * powf(2.0f, (midi - 69) / 12.0f) + 0.5f);
}

/* An original bouncy tune in C major: MIDI note (0 rest), eighths. */
static const int8_t s_tune[][2] = {
    { 72, 1 }, { 76, 1 }, { 79, 2 }, { 76, 1 }, { 79, 1 }, { 81, 2 },
    { 79, 1 }, { 77, 1 }, { 76, 1 }, { 74, 1 }, { 72, 2 }, { 0, 2 },
    { 74, 1 }, { 77, 1 }, { 81, 2 }, { 79, 1 }, { 77, 1 }, { 76, 2 },
    { 74, 1 }, { 76, 1 }, { 77, 1 }, { 74, 1 }, { 79, 2 }, { 0, 2 },
};
static const int8_t s_bass[] = { 48, 55, 53, 55, 50, 57, 55, 43 };
#define TUNE_LEN ((int)(sizeof(s_tune) / sizeof(s_tune[0])))
#define EIGHTH_MS 160

static void music(uint32_t dt)
{
    if (s_state != PLAY) {
        return;
    }
    s_music_ms -= (int)dt;
    if (s_music_ms > 0) {
        return;
    }
    int len = s_tune[s_music_pos][1] * EIGHTH_MS;
    if (s_tune[s_music_pos][0]) {
        msw_sound(0, MSW_WAVE_SQUARE, hz(s_tune[s_music_pos][0]), len - 25, 14, true);
    }
    static int beat;
    msw_sound(3, MSW_WAVE_TRIANGLE, hz(s_bass[(beat / 4) % 8]), len - 10, 30, false);
    beat += s_tune[s_music_pos][1];
    s_music_ms += len;
    s_music_pos = (s_music_pos + 1) % TUNE_LEN;
}

/* ---- the world ---------------------------------------------------------------- */

static char tile_at(int col, int row)
{
    if (col < 0 || col >= s_cols) {
        return '#';                         /* walls at both ends */
    }
    if (row < 0) {
        return ' ';
    }
    if (row >= ROWS) {
        return ' ';                         /* pits fall out of the world */
    }
    return s_tiles[row][col];
}

static bool solid(char t)
{
    return t == '#' || t == 'B' || t == 'Q' || t == 'q' || t == '=';
}

static void load_level(void)
{
    s_cols = 0;
    for (int r = 0; r < ROWS; r++) {
        int n = (int)strlen(s_map[r]);
        s_cols = n > s_cols ? n : s_cols;
    }
    if (s_cols > 127) {
        s_cols = 127;
    }
    s_nen = 0;
    for (int r = 0; r < ROWS; r++) {
        int n = (int)strlen(s_map[r]);
        for (int c = 0; c < s_cols; c++) {
            char t = c < n ? s_map[r][c] : ' ';
            if (t == 'E' && s_nen < MAX_ENEMIES) {
                body_t *e = &s_en[s_nen++];
                memset(e, 0, sizeof(*e));
                e->x = (float)(c * TILE);
                e->y = (float)(r * TILE);
                e->vx = -0.9f;
                e->alive = true;
                t = ' ';
            }
            s_tiles[r][c] = t;
        }
    }
}

static void start_run(bool fresh)
{
    if (fresh) {
        s_lives = 3;
        s_seeds = 0;
    }
    load_level();
    memset(&s_me, 0, sizeof(s_me));
    s_me.x = 2 * TILE;
    s_me.y = 10 * TILE;
    s_me.alive = true;
    s_cam = 0;
    s_state = PLAY;
    s_music_pos = 0;
    s_music_ms = 0;
}

/* Moves a body by its speed, stopping at solid tiles. Returns the tile hit
 * from below (a head bump), as col, or -1. */
static int move_body(body_t *b, float w, float h, float ox, float oy, float dt)
{
    int bumped = -1;
    float nx = b->x + b->vx * dt;
    float top = b->y + oy, bottom = b->y + oy + h - 1;
    float left = nx + ox, right = nx + ox + w - 1;
    for (int r = (int)floorf(top / TILE); r <= (int)floorf(bottom / TILE); r++) {
        if (b->vx > 0 && solid(tile_at((int)floorf(right / TILE), r))) {
            nx = floorf(right / TILE) * TILE - w - ox;
            b->vx = -b->vx * 0.0f;
        } else if (b->vx < 0 && solid(tile_at((int)floorf(left / TILE), r))) {
            nx = (floorf(left / TILE) + 1) * TILE - ox;
            b->vx = 0;
        }
    }
    b->x = nx;
    float ny = b->y + b->vy * dt;
    left = b->x + ox;
    right = b->x + ox + w - 1;
    b->on_ground = false;
    for (int c = (int)floorf(left / TILE); c <= (int)floorf(right / TILE); c++) {
        if (b->vy > 0) {
            float bot = ny + oy + h - 1;
            int r = (int)floorf(bot / TILE);
            char t = tile_at(c, r);
            bool ledge_ok = t != '=' || (b->y + oy + h - 1) < r * TILE;   /* ledges: from above only */
            if (solid(t) && ledge_ok) {
                ny = r * TILE - h - oy;
                b->vy = 0;
                b->on_ground = true;
            }
        } else if (b->vy < 0) {
            int r = (int)floorf((ny + oy) / TILE);
            char t = tile_at(c, r);
            if (solid(t) && t != '=') {
                ny = (r + 1) * TILE - oy;
                b->vy = 0;
                float mid = left + w / 2;
                if (bumped < 0 || fabsf((c + 0.5f) * TILE - mid) < fabsf((bumped + 0.5f) * TILE - mid)) {
                    bumped = c;
                    s_bump_row = r;
                }
            }
        }
    }
    b->y = ny;
    return bumped;
}

static void die(void)
{
    s_state = DEAD;
    s_state_ms = 0;
    s_lives--;
    msw_sound_stop(-1);
    msw_sound_slide(1, MSW_WAVE_SQUARE, 700, 120, 700, 35);
    msw_vibrate(250, 80);
}

static void collect(int col, int row)
{
    s_tiles[row][col] = ' ';
    s_seeds++;
    msw_sound_slide(2, MSW_WAVE_SQUARE, 1200, 2000, 90, 25);
}

static void update(uint32_t dt_ms)
{
    float dt = dt_ms / 33.3f;
    if (dt > 2.0f) {
        dt = 2.0f;
    }
    /* Pyoko */
    if (s_btn_down[B_SLIDE] || s_btn_down[B_TILT]) {
        /* the speed is the slide or the tilt, at once */
        s_me.vx = 4.2f * s_move;
        if (s_move < -0.05f || s_move > 0.05f) {
            s_face_left = s_move < 0.0f;
        }
    } else {
        s_me.vx *= s_me.on_ground ? 0.75f : 0.95f;
    }
    s_me.vx = s_me.vx > 4.2f ? 4.2f : s_me.vx < -4.2f ? -4.2f : s_me.vx;
    if (s_jump_down && !s_jump_held && s_me.on_ground) {
        s_me.vy = -11.8f;
        msw_sound_slide(1, MSW_WAVE_SQUARE, 400, 900, 120, 25);
    }
    if (!s_jump_down && s_me.vy < -4.0f) {
        s_me.vy = -4.0f;                    /* let go early: a lower jump */
    }
    s_jump_held = s_jump_down;
    s_me.vy += 0.75f * dt;
    if (s_me.vy > 12.0f) {
        s_me.vy = 12.0f;
    }
    int bump = move_body(&s_me, PW, PH, POX, POY, dt);
    if (bump >= 0) {
        char t = s_tiles[s_bump_row][bump];
        s_bump_col = bump;
        s_bump_ms = 150;
        if (t == 'Q') {
            s_tiles[s_bump_row][bump] = 'q';
            s_seeds++;
            msw_sound_slide(2, MSW_WAVE_TRIANGLE, 900, 1800, 160, 35);
            msw_vibrate(30, 50);
        } else {
            msw_sound(2, MSW_WAVE_NOISE, 400, 60, 30, true);
        }
    }
    if (s_me.x < 0) {
        s_me.x = 0;
    }
    /* seeds and the flag under Pyoko */
    for (int r = (int)((s_me.y + POY) / TILE); r <= (int)((s_me.y + POY + PH - 1) / TILE); r++) {
        for (int c = (int)((s_me.x + POX) / TILE); c <= (int)((s_me.x + POX + PW - 1) / TILE); c++) {
            char t = tile_at(c, r);
            if (t == '*') {
                collect(c, r);
            } else if (t == 'F') {
                s_state = CLEAR;
                s_state_ms = 0;
                msw_sound_stop(-1);
                static const int fanfare[] = { 72, 76, 79, 84 };
                for (int i = 0; i < 4; i++) {
                    msw_sound(i, MSW_WAVE_SQUARE, hz(fanfare[i]), 900, 18, true);
                }
                if (s_seeds > s_best) {
                    s_best = s_seeds;
                    msw_store_put("best", &s_best, sizeof(s_best));
                }
                return;
            }
        }
    }
    if (s_me.y > ROWS * TILE) {
        die();
        return;
    }
    /* rollers */
    for (int i = 0; i < s_nen; i++) {
        body_t *e = &s_en[i];
        if (!e->alive) {
            continue;
        }
        if (e->squash_ms) {
            e->squash_ms -= (int)dt_ms;
            if (e->squash_ms <= 0) {
                e->alive = false;
            }
            continue;
        }
        if (fabsf(e->x - s_me.x) > SCR) {
            continue;                       /* asleep off screen */
        }
        float was = e->vx;
        e->vy += 0.75f * dt;
        move_body(e, 20, 20, 2, 4, dt);
        if (e->vx == 0) {
            e->vx = was > 0 ? -0.9f : 0.9f;  /* hit a wall: turn */
        } else if (e->on_ground) {
            int ahead = (int)((e->x + (e->vx > 0 ? 23 : 0)) / TILE);
            int below = (int)((e->y + 25) / TILE);
            if (!solid(tile_at(ahead, below))) {
                e->vx = -e->vx;              /* an edge: turn */
            }
        }
        if (e->y > ROWS * TILE) {
            e->alive = false;
            continue;
        }
        /* touching Pyoko? */
        float ax = s_me.x + POX, ay = s_me.y + POY, bx = e->x + 2, by = e->y + 4;
        if (ax < bx + 20 && ax + PW > bx && ay < by + 20 && ay + PH > by) {
            if (s_me.vy > 0 && ay + PH < by + 12) {
                e->squash_ms = 400;
                s_me.vy = -8.0f;
                msw_sound_slide(2, MSW_WAVE_SQUARE, 300, 900, 100, 35);
                msw_vibrate(30, 60);
            } else {
                die();
                return;
            }
        }
    }
    /* camera */
    float want = s_me.x + 12 - SCR / 2 + (s_face_left ? -30 : 30);
    s_cam += (want - s_cam) * 0.12f * dt;
    float max = (float)(s_cols * TILE - SCR);
    s_cam = s_cam < 0 ? 0 : s_cam > max ? max : s_cam;
    s_anim_ms += (int)dt_ms;
    if (s_bump_ms > 0) {
        s_bump_ms -= (int)dt_ms;
    }
}

/* ---- input ------------------------------------------------------------------------ */

/* The finger holds the tilt button; jump is the blue button, so the
 * screen never needs two fingers. */
static int hit(int tx, int ty, int i)
{
    int dx = tx - s_btn[i].x, dy = ty - s_btn[i].y, reach = s_btn[i].r + 18;
    return dx * dx + dy * dy < reach * reach;
}

/* A tap (or blue) on a message: start, go on or try again. */
static void go_on(void)
{
    if (s_state == PLAY || s_state_ms <= 600) {
        return;
    }
    if (s_state == PAUSE) {
        s_state = PLAY;
    } else if (s_state == DEAD && s_lives > 0) {
        start_run(false);
    } else {
        start_run(true);
    }
    s_jump_held = true;                     /* the press that started it does not jump */
}

static void read_touch(void)
{
    int x, y;
    bool touched = msw_touch(&x, &y);
    static bool was;
    msw_imu_t m;
    if (msw_imu(&m)) {
        s_tilt += (-m.ax - s_tilt) * 0.6f;  /* gravity to the right, smoothed */
    }
    if (!touched) {
        s_btn_down[B_SLIDE] = s_btn_down[B_TILT] = false;
    } else if (!was) {
        /* a new touch: the nearest control under it keeps it until it lifts */
        int best = -1, best_d = 0;
        for (int i = 0; i < NBTN; i++) {
            int dx = x - s_btn[i].x, dy = y - s_btn[i].y, d = dx * dx + dy * dy;
            if (hit(x, y, i) && (best < 0 || d < best_d)) {
                best = i;
                best_d = d;
            }
        }
        s_btn_down[B_SLIDE] = best == B_SLIDE;
        s_btn_down[B_TILT] = best == B_TILT;
        s_slide_x0 = x;
        s_tilt_zero = s_tilt;               /* the tilt now is level */
    }
    s_move = 0.0f;
    if (s_btn_down[B_SLIDE]) {
        float a = (x - s_slide_x0) / SLIDE_FULL, b = a < 0 ? -a : a;
        b = b > 1.0f ? 1.0f : powf(b, SLIDE_CURVE);
        s_move = a < 0 ? -b : b;
    } else if (s_btn_down[B_TILT]) {
        float t = s_tilt - s_tilt_zero, a = t < 0 ? -t : t;
        if (a > TILT_DEAD) {
            a = (a - TILT_DEAD) / (TILT_FULL - TILT_DEAD);
            a = a > 1.0f ? 1.0f : powf(a, TILT_CURVE);
            s_move = t < 0 ? -a : a;
        }
    }
    if (touched && !was) {
        go_on();
    }
    was = touched;
}

/* The blue button: jump while playing, or the same as a tap on a message. */
static void button(int b, bool down, void *user)
{
    (void)user;
    if (b != MSW_BUTTON_BLUE) {
        return;
    }
    s_jump_down = down;
    if (down) {
        go_on();
    }
}

/* ---- drawing ------------------------------------------------------------------------ */

static void text_center(int cx, int y, const char *s, uint16_t c, int size)
{
    msw_gfx_text(fb, cx - msw_text_width(s, size) / 2, y, s, c, size);
}

static void draw(void)
{
    /* sky */
    for (int y = 0; y < SCR; y += 4) {
        int k = y * 100 / SCR;
        msw_gfx_fill_rect(fb, 0, y, SCR, 4, MSW_RGB(110 + k, 180 + k / 2, 240));
    }
    /* far hills, scrolling slower */
    int cam = (int)s_cam;
    for (int i = -1; i < 6; i++) {
        int hx = i * 160 - (cam / 3) % 160;
        msw_gfx_fill_circle(fb, hx + 80, OY + 13 * TILE + 30, 110, MSW_RGB(129, 199, 132));
    }
    for (int i = -1; i < 5; i++) {
        int cx = i * 220 - (cam / 5) % 220 + 60;
        msw_gfx_fill_circle(fb, cx, 120, 18, MSW_RGB(255, 255, 255));
        msw_gfx_fill_circle(fb, cx + 22, 112, 24, MSW_RGB(255, 255, 255));
        msw_gfx_fill_circle(fb, cx + 46, 122, 16, MSW_RGB(255, 255, 255));
    }
    /* tiles */
    int c0 = cam / TILE, c1 = (cam + SCR) / TILE;
    for (int r = 0; r < ROWS; r++) {
        for (int c = c0; c <= c1 && c < s_cols; c++) {
            char t = s_tiles[r][c];
            int x = c * TILE - cam, y = OY + r * TILE;
            if (c == s_bump_col && r == s_bump_row && s_bump_ms > 0) {
                y -= 6;
            }
            const msw_sprite_t *s = NULL;
            switch (t) {
            case '#':
                s = r > 0 && s_tiles[r - 1][c] == '#' ? s_dirt : s_grass;
                break;
            case 'B':
                s = s_brick;
                break;
            case 'Q':
                s = s_crystal;
                break;
            case 'q':
                s = s_used;
                break;
            case '=':
                s = s_ledge;
                break;
            case '*':
                y += (s_anim_ms / 200 + c) % 2 ? -2 : 0;
                s = s_seed;
                break;
            case 'F':
                msw_gfx_fill_rect(fb, x + 10, y - 5 * TILE, 4, 6 * TILE, MSW_RGB(240, 240, 240));
                msw_gfx_fill_rect(fb, x + 14, y - 5 * TILE + 4, 30, 20, MSW_RGB(255, 112, 67));
                msw_gfx_fill_circle(fb, x + 12, y - 5 * TILE, 5, MSW_RGB(255, 213, 79));
                break;
            }
            if (s) {
                msw_sprite_draw(fb, s, x, y, 3, 0);
            }
        }
    }
    /* rollers */
    for (int i = 0; i < s_nen; i++) {
        const body_t *e = &s_en[i];
        if (!e->alive) {
            continue;
        }
        int x = (int)e->x - cam, y = OY + (int)e->y;
        if (x < -TILE || x > SCR) {
            continue;
        }
        msw_sprite_draw(fb, e->squash_ms ? s_flat : s_roller[(s_anim_ms / 180) % 2], x, y, 3, 0);
    }
    /* Pyoko */
    if (s_state != DEAD || (s_state_ms / 100) % 2) {
        const msw_sprite_t *p = !s_me.on_ground ? s_jump
                                : fabsf(s_me.vx) > 0.5f ? s_pyoko[(s_anim_ms / 120) % 2] : s_pyoko[0];
        msw_sprite_draw(fb, p, (int)s_me.x - cam, OY + (int)s_me.y, 3, s_face_left ? MSW_FLIP_X : 0);
    }
    /* score */
    char buf[48];
    msw_sprite_draw(fb, s_seed, 170, 40, 3, 0);
    snprintf(buf, sizeof(buf), "%d", s_seeds);
    msw_gfx_text(fb, 196, 44, buf, MSW_RGB(255, 255, 255), 20);
    snprintf(buf, sizeof(buf), "x%d", s_lives);
    msw_sprite_draw(fb, s_pyoko[0], 236, 40, 3, 0);
    msw_gfx_text(fb, 262, 44, buf, MSW_RGB(255, 255, 255), 20);
    /* buttons */
    for (int i = 0; i < NBTN; i++) {
        uint16_t ring = s_btn_down[i] ? MSW_RGB(255, 255, 255) : MSW_RGB(40, 60, 90);
        msw_gfx_circle(fb, s_btn[i].x, s_btn[i].y, s_btn[i].r, ring);
        msw_gfx_circle(fb, s_btn[i].x, s_btn[i].y, s_btn[i].r - 1, ring);
        text_center(s_btn[i].x, s_btn[i].y - (s_btn[i].size ? 11 : 9), s_btn[i].label, ring, s_btn[i].size);
        if (s_btn_down[i]) {
            /* how fast it goes: a dot that slides under the label */
            int by = s_btn[i].y + s_btn[i].r - 10;
            msw_gfx_fill_circle(fb, s_btn[i].x + (int)(s_move * (s_btn[i].r - 14)), by, 5,
                                MSW_RGB(255, 213, 79));
        }
    }
    {
        uint16_t c = s_jump_down ? MSW_RGB(255, 255, 255) : MSW_RGB(66, 133, 244);
        msw_gfx_fill_circle(fb, JUMP_MARK_X, JUMP_MARK_Y, 15, c);
        text_center(JUMP_MARK_X, JUMP_MARK_Y - 10, LV_SYMBOL_UP, MSW_RGB(10, 20, 40), 20);
    }
    /* messages */
    const char *title = NULL, *sub = "タップでスタート";
    if (s_state == TITLE) {
        title = "ぴょこの森";
    } else if (s_state == PAUSE) {
        title = "ポーズ";
        sub = "タップでつづける";
    } else if (s_state == DEAD) {
        title = s_lives > 0 ? "あいたた…" : "ゲームオーバー";
        sub = s_lives > 0 ? "タップでもう一度" : "タップで最初から";
    } else if (s_state == CLEAR) {
        title = "ゴール！";
        snprintf(buf, sizeof(buf), "たね %d こ  (さいこう %d)", s_seeds, s_best);
        sub = buf;
    }
    if (title) {
        int top = s_state == TITLE ? 140 : 170, h = s_state == TITLE ? 150 : 90;
        msw_gfx_fill_rect(fb, 103, top, 260, h, MSW_RGB(20, 40, 30));
        msw_gfx_rect(fb, 103, top, 260, h, MSW_RGB(255, 213, 79));
        text_center(233, top + 18, title, MSW_RGB(255, 213, 79), 0);
        text_center(233, top + 52, sub, MSW_RGB(230, 230, 230), 0);
        if (s_state == TITLE) {
            text_center(233, top + 80, "左下: 指を左右にずらす", MSW_RGB(230, 230, 230), 0);
            text_center(233, top + 102, "左上の傾き: 押して傾ける", MSW_RGB(230, 230, 230), 0);
            text_center(233, top + 124, "ジャンプ: 右上の青ボタン", MSW_RGB(130, 177, 255), 0);
        }
    }
    msw_fb_present();
}

/* ---- the callbacks ---------------------------------------------------------------------- */

static void tick(uint32_t dt, void *user)
{
    (void)user;
    read_touch();
    s_state_ms += (int)dt;
    if (s_state == PLAY) {
        update(dt);
        music(dt);
    }
    draw();
}

#define ART(a) msw_sprite_from_art(a, 8, s_pal)

int app_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fb = msw_fb();
    if (!fb) {
        return 1;
    }
    s_pyoko[0] = ART(ART_PYOKO[0]);
    s_pyoko[1] = ART(ART_PYOKO[1]);
    s_jump = ART(ART_JUMP);
    s_roller[0] = ART(ART_ROLLER[0]);
    s_roller[1] = ART(ART_ROLLER[1]);
    s_flat = ART(ART_FLAT);
    s_grass = ART(ART_GRASS);
    s_dirt = ART(ART_DIRT);
    s_brick = ART(ART_BRICK);
    s_crystal = ART(ART_CRYSTAL);
    s_used = ART(ART_USED);
    s_seed = ART(ART_SEED);
    s_ledge = ART(ART_LEDGE);
    if (!s_pyoko[0] || !s_pyoko[1] || !s_jump || !s_roller[0] || !s_roller[1] || !s_flat || !s_grass ||
        !s_dirt || !s_brick || !s_crystal || !s_used || !s_seed || !s_ledge) {
        msw_log("sprite art failed");
        return 1;
    }
    if (msw_store_get("best", &s_best, sizeof(s_best)) != sizeof(s_best)) {
        s_best = 0;
    }
    load_level();
    s_me.x = 2 * TILE;
    s_me.y = 12 * TILE - PH - POY;
    s_me.on_ground = true;
    msw_set_fps(30);
    msw_on_tick(tick, NULL);
    msw_on_button(button, NULL);
    return 0;
}
