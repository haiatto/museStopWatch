/*
 * しずく (Shizuku): the sample avatar of the museStopWatch SDK (idea 009).
 *
 * Who: a small water drop that lives in the watch. Round at the bottom,
 *      pointed on top, two little nub hands, no feet (it sits on its round
 *      bottom). Clear water blue with a white shine on the upper left, dark
 *      bead eyes, pink cheeks.
 * Personality: calm and curious; a bit shy, very happy to be petted.
 *
 * It follows app.avatar_doc: parts (a drop, two hand discs, stamped eyes and
 * mouth) shaded from the top left with a 4x4 Bayer dither, a 1 px outline,
 * per-state colours blended over time, random blinks and gaze, and the
 * official speaking gestures (muse_speaking.h). Per-pixel loops use integers
 * only; floats are per frame and per part.
 */
#include <math.h>
#include <string.h>

#include "msw_app.h"
#include "muse_speaking.h"

#define W MSW_AVATAR_W
#define H MSW_AVATAR_H
#define TAU 6.2831853f

/* ---- colours ------------------------------------------------------------ */

enum {
    C_BG = 0,
    C_OUT,       /* outline */
    C_OUT2,      /* seam where a hand lies on the body */
    C_BD,        /* body: dark, mid, light, highlight */
    C_BM,
    C_BL,
    C_BH,
    C_EYE,
    C_SHINE,
    C_BLUSH,
    C_MOUTH,
    C_TONGUE,
    C_G0,        /* the state's glow: bright ... */
    C_G1,
    C_G2,
    C_G3,        /* ... deep */
    C_AURA1,
    C_AURA2,
    C_SPK,
    C_ACC,
    C_SHADOW,
    C_HEART,
    C_WHITE,
    C_COUNT,
};

static const uint32_t FIXED[C_COUNT] = {
    [C_BG] = 0x000000, [C_OUT] = 0x0b2340, [C_OUT2] = 0x1d4f80,
    [C_BD] = 0x1f6fb0, [C_BM] = 0x3d9be0, [C_BL] = 0x7cc8f6, [C_BH] = 0xd2f0ff,
    [C_EYE] = 0x0a1a2c, [C_SHINE] = 0xffffff, [C_BLUSH] = 0xff8fb8, [C_MOUTH] = 0x0a1a2c,
    [C_TONGUE] = 0xff6f91, [C_SHADOW] = 0x0f1a26, [C_HEART] = 0xff5c8a, [C_WHITE] = 0xffffff,
};

/* Per state: the glow ramp (bright -> deep) and the accent. Water tones. */
typedef struct {
    uint32_t f[4];
    uint32_t acc;
} scheme_t;

static const scheme_t SCHEMES[7] = {
    [MSW_MODE_BOOT] = { { 0xffffff, 0xd6efff, 0x96ccff, 0x4f8fe0 }, 0xb0dcff },
    [MSW_MODE_IDLE] = { { 0xe6fbff, 0x9fe3ff, 0x4fb8f0, 0x1f6fb8 }, 0x5cc6ff },
    [MSW_MODE_LISTENING] = { { 0xe8fff9, 0x8ff5dc, 0x3fd9b8, 0x1f8f8a }, 0x5cf0d0 },
    [MSW_MODE_THINKING] = { { 0xfff6e0, 0xffd98f, 0xf0a84f, 0xb0602a }, 0xffc85c },
    [MSW_MODE_SPEAKING] = { { 0xf0ffe8, 0xbff59f, 0x7fd95f, 0x3f9a3a }, 0x9cf06f },
    [MSW_MODE_ERROR] = { { 0xffd6d6, 0xff6b6b, 0xc7304a, 0x6b1a3a }, 0xff5c5c },
    [MSW_MODE_OFF] = { { 0xd8e4ff, 0x8fa0d9, 0x5a68b0, 0x2e3870 }, 0x7c8cd0 },
};
static const scheme_t WORKING = { { 0xfff0e0, 0xffc08f, 0xf08f4f, 0xb04f2a }, 0xffa05c };

typedef struct {
    float r, g, b;
} rgb_t;

static rgb_t s_scheme[5];
static bool s_scheme_ok;

static rgb_t hex(uint32_t c)
{
    return (rgb_t){ (float)(c >> 16 & 255), (float)(c >> 8 & 255), (float)(c & 255) };
}

static rgb_t mix(rgb_t a, rgb_t b, float k)
{
    return (rgb_t){ a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k };
}

static uint16_t to565(rgb_t c, float k)
{
    int r = (int)(c.r * k), g = (int)(c.g * k), b = (int)(c.b * k);
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return MSW_RGB(r, g, b);
}

static uint32_t accent(int mode)
{
    if (mode < 0 || mode > MSW_MODE_OFF) {
        mode = MSW_MODE_IDLE;
    }
    return SCHEMES[mode].acc;
}

/* The state's colours, blended toward the target with 1 - exp(-dt * 7). */
static void palette(uint16_t *pal, const scheme_t *target, float dt, float fade)
{
    float k = s_scheme_ok ? 1.0f - expf(-dt * 7.0f) : 1.0f;
    for (int i = 0; i < 4; i++) {
        s_scheme[i] = mix(s_scheme[i], hex(target->f[i]), k);
    }
    s_scheme[4] = mix(s_scheme[4], hex(target->acc), k);
    s_scheme_ok = true;
    rgb_t acc = s_scheme[4];
    for (int i = 0; i < C_COUNT; i++) {
        pal[i] = to565(hex(FIXED[i]), fade);
    }
    pal[C_BG] = 0;
    for (int i = 0; i < 4; i++) {
        pal[C_G0 + i] = to565(s_scheme[i], fade);
    }
    pal[C_ACC] = to565(acc, fade);
    pal[C_AURA1] = to565(acc, 0.16f * fade);
    pal[C_AURA2] = to565(acc, 0.34f * fade);
    pal[C_SPK] = to565(mix(acc, hex(0xffffff), 0.45f), fade);
    pal[C_EYE] = to565(hex(FIXED[C_EYE]), 1);   /* eyes stay dark as the glow fades */
}

/* ---- helpers --------------------------------------------------------------- */

static uint8_t *g;      /* this frame's grid */

static const int8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

static inline void px(int x, int y, uint8_t c)
{
    if ((unsigned)x < W && (unsigned)y < H) {
        g[y * W + x] = c;
    }
}

static inline uint8_t get(int x, int y)
{
    return (unsigned)x < W && (unsigned)y < H ? g[y * W + x] : C_BG;
}

static inline int iround(float v)
{
    return (int)floorf(v + 0.5f);
}

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static uint32_t s_seed = 0x5eed1234u;

static float frand(void)
{
    s_seed = s_seed * 1664525u + 1013904223u;
    return (s_seed >> 8) / 16777216.0f;
}

/* A stamped bitmap: '#' fill, 'o' alt, '.' nothing. Centred on x, y. */
static void stamp(const char *const *rows, int n, int x, int y, uint8_t fill, uint8_t alt)
{
    int w = (int)strlen(rows[0]);
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < w; c++) {
            char ch = rows[r][c];
            if (ch != '.') {
                px(x - w / 2 + c, y - n / 2 + r, ch == 'o' ? alt : fill);
            }
        }
    }
}

/* A small filled disc (per part: a few dozen pixels). */
static void disc(float cx, float cy, float r, uint8_t c)
{
    int r2 = (int)(r * r * 16);
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++) {
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            int dx = (int)((x + 0.5f - cx) * 4), dy = (int)((y + 0.5f - cy) * 4);
            if (dx * dx + dy * dy <= r2) {
                px(x, y, c);
            }
        }
    }
}

/* Unit circle, 64 steps, built once. */
static float s_cos[64], s_sin[64];

static void tables(void)
{
    for (int i = 0; i < 64; i++) {
        s_cos[i] = cosf(i * TAU / 64);
        s_sin[i] = sinf(i * TAU / 64);
    }
}

/* ---- eyes: random blinks and a wandering gaze ------------------------------ */

static float s_blink_at = 2.0f, s_blink_t = -1, s_gaze_at, s_gx, s_gy, s_tx, s_ty;
static bool s_double;

static float eyes_update(float t, float dt, int mode, bool working)
{
    if (t >= s_blink_at && s_blink_t < 0) {
        s_blink_t = 0;
    }
    float shut = 0;
    if (s_blink_t >= 0) {
        s_blink_t += dt;
        shut = s_blink_t < 0.16f ? 1 : 0;
        if (s_blink_t >= 0.16f) {
            s_blink_t = -1;
            if (!s_double && frand() < 0.25f) {
                s_double = true;
                s_blink_at = t + 0.12f;
            } else {
                s_double = false;
                s_blink_at = t + 2.2f + frand() * 3.0f;
            }
        }
    }
    if (t >= s_gaze_at) {
        s_tx = frand() * 2 - 1;
        s_ty = (frand() * 2 - 1) * 0.6f;
        s_gaze_at = t + 1.2f + frand() * 2.4f;
    }
    float tx = s_tx, ty = s_ty;
    if (mode == MSW_MODE_LISTENING) {
        tx = 0;
        ty = 0;
    } else if (working) {
        tx = 0;
        ty = 1;                                     /* down on the work */
    } else if (mode == MSW_MODE_THINKING) {
        tx = sinf(t * 1.3f);
        ty = -0.9f;                                 /* up and around */
    }
    float k = 1.0f - expf(-dt * 14.0f);
    s_gx += (tx - s_gx) * k;
    s_gy += (ty - s_gy) * k;
    return shut;
}

/* ---- the body -------------------------------------------------------------- */

typedef struct {
    float x, y;
} pt_t;

/* The drop's half width (in body radii) on a row; < 0 outside. Per row. */
static float drop_half(float y, float cy, float ry)
{
    float dy = (y - cy) / ry;
    if (dy >= -0.2f) {
        return dy < 1.0f ? sqrtf(1.0f - dy * dy) : -1;
    }
    float k = (dy + 2.0f) / 1.8f;   /* 0 at the tip (2 radii up), 1 where the round part starts */
    return k > 0 ? 0.98f * powf(k, 0.85f) : -1;
}

static bool is_body(uint8_t c)
{
    return c >= C_BD && c <= C_BH;
}

/* Body tones from a fake normal and the top-left light, dithered.
 * Per pixel: integers only (Q12). */
static void draw_drop(float cx, float cy, float rx, float ry)
{
    int qx = (int)(4096.0f / rx), qy = (int)(4096.0f / ry);
    int y0 = (int)(cy - 2 * ry) - 1, y1 = (int)(cy + ry) + 1;
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < H; y++) {
        float half = drop_half(y + 0.5f, cy, ry);
        if (half < 0) {
            continue;
        }
        int xl = (int)ceilf(cx - half * rx - 0.5f), xr = (int)floorf(cx + half * rx - 0.5f);
        int ly = (int)((y + 0.5f - cy) * qy);                 /* Q12 */
        int lx = (int)((xl + 0.5f - cx) * qx);
        for (int x = xl; x <= xr; x++, lx += qx) {
            if ((unsigned)x >= W) {
                continue;
            }
            /* light = -0.55 lx - 0.65 ly + 0.15, in sixteenths, + dither */
            int v = ((-2253 * lx - 2662 * ly) >> 20) + 2 + BAYER[y & 3][x & 3] - 8;
            g[y * W + x] = v > 13 ? C_BH : v > 5 ? C_BL : v > -6 ? C_BM : C_BD;
        }
    }
}

/* A hand: a small disc in body tones, with a seam where it lies on the body. */
static void draw_hand(pt_t h)
{
    const float r = 2.7f;
    int r2 = (int)(r * r * 16), in2 = (int)((r - 1) * (r - 1) * 16);
    for (int y = (int)(h.y - r - 1); y <= (int)(h.y + r + 1); y++) {
        for (int x = (int)(h.x - r - 1); x <= (int)(h.x + r + 1); x++) {
            int dx = (int)((x + 0.5f - h.x) * 4), dy = (int)((y + 0.5f - h.y) * 4);
            int d2 = dx * dx + dy * dy;
            if (d2 > r2 || (unsigned)x >= W || (unsigned)y >= H) {
                continue;
            }
            uint8_t under = g[y * W + x];
            if (d2 > in2 && is_body(under)) {
                g[y * W + x] = C_OUT2;
            } else {
                int v = ((-dx - dy) >> 2) + BAYER[y & 3][x & 3] - 6;
                g[y * W + x] = v > 3 ? C_BL : v > -5 ? C_BM : C_BD;
            }
        }
    }
}

/* 1 px outline: background next to the body (4 neighbours), in a box. */
static void outline(int x0, int y0, int x1, int y1)
{
    x0 = x0 < 0 ? 0 : x0;
    y0 = y0 < 0 ? 0 : y0;
    x1 = x1 > W - 1 ? W - 1 : x1;
    y1 = y1 > H - 1 ? H - 1 : y1;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            uint8_t c = g[y * W + x];
            if (is_body(c) || c == C_OUT2 || c == C_OUT) {
                continue;
            }
            uint8_t a = get(x - 1, y), b = get(x + 1, y), u = get(x, y - 1), d = get(x, y + 1);
            if (is_body(a) || is_body(b) || is_body(u) || is_body(d) || a == C_OUT2 || b == C_OUT2 ||
                u == C_OUT2 || d == C_OUT2) {
                g[y * W + x] = C_OUT;
            }
        }
    }
}

/* ---- background and effects --------------------------------------------- */

/* A soft dithered aura: integers per pixel (squared distances against a
 * radius for each dither step, worked out per frame). */
static void draw_aura(float cx, float cy, float radius, float strength)
{
    if (strength <= 0.02f) {
        return;
    }
    int lim1[16], lim2[16];
    for (int th = 0; th < 16; th++) {
        float r1 = radius * clampf(strength * (1.0f - (th + 2) / 24.0f), 0, 1);
        float r2 = radius * clampf(strength * (1.0f - (th + 10) / 24.0f), 0, 1);
        lim1[th] = (int)(r1 * r1 * 16);
        lim2[th] = (int)(r2 * r2 * 16);
    }
    int y0 = (int)(cy - radius) - 1, y1 = (int)(cy + radius) + 1;
    for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < H; y++) {
        int dy = (int)((y + 0.5f - cy) * 4);
        for (int x = 0; x < W; x++) {
            int dx = (int)((x + 0.5f - cx) * 4);
            int d2 = dx * dx + dy * dy, th = BAYER[y & 3][x & 3];
            if (d2 < lim2[th]) {
                g[y * W + x] = C_AURA2;
            } else if (d2 < lim1[th]) {
                g[y * W + x] = C_AURA1;
            }
        }
    }
}

/* Dotted rings that grow out and fade (listening, speaking). */
static void draw_rings(float cx, float cy, float t, float level, float speed)
{
    for (int i = 0; i < 3; i++) {
        float ph = t * speed + i / 3.0f;
        ph -= floorf(ph);
        float r = 14.0f + ph * (14.0f + level * 6.0f);
        uint8_t c = ph < 0.4f ? C_G1 : ph < 0.75f ? C_G2 : C_G3;
        for (int k = 0; k < 64; k += 2) {
            px(iround(cx + s_cos[k] * r), iround(cy + s_sin[k] * r * 0.85f), c);
        }
    }
}

static void draw_shadow(float cx, float y, float half_w)
{
    for (int dy = -1; dy <= 1; dy++) {
        for (int x = (int)(cx - half_w); x <= (int)(cx + half_w); x++) {
            float u = (x + 0.5f - cx) / half_w;
            int lim = (int)((1 - u * u - (dy ? 0.5f : 0)) * 16);
            int yy = iround(y) + dy;
            if (lim > BAYER[yy & 3][x & 3] && get(x, yy) != C_OUT) {
                px(x, yy, C_SHADOW);
            }
        }
    }
}

/* Sparkles circling the body; behind (front = false) or in front. */
static void draw_sparkles(float cx, float cy, float t, float speed, int count, bool front)
{
    for (int i = 0; i < count; i++) {
        float a = t * speed * 0.35f + i * TAU / 6;
        int k = (int)(a * 64 / TAU) & 63;
        bool is_front = s_sin[k] >= 0;
        if (is_front != front) {
            continue;
        }
        int x = iround(cx + s_cos[k] * 23.0f), y = iround(cy - 8 + s_sin[k] * 12.0f);
        float tw = sinf(t * 5.0f + i * 1.7f);
        px(x, y, C_SPK);
        if (tw > 0.3f) {
            px(x - 1, y, C_G1); px(x + 1, y, C_G1); px(x, y - 1, C_G1); px(x, y + 1, C_G1);
        }
    }
}

static void draw_thought_dots(float x, float y, float t)
{
    for (int i = 0; i < 3; i++) {
        float up = fmaxf(0, sinf(t * 5.0f - i * 0.9f)) * 2.0f;
        disc(x + i * 4.0f, y - i * 2.0f - up, 1.2f + i * 0.3f, C_G1);
    }
}

static void draw_spinner(float x, float y, float t)
{
    int head = (int)(t * 12.0f) & 7;
    for (int i = 0; i < 8; i++) {
        int k = i * 8;
        int age = (head - i) & 7;
        px(iround(x + s_cos[k] * 3), iround(y + s_sin[k] * 3), age == 0 ? C_WHITE : age < 3 ? C_G1 : C_G3);
    }
}

static void draw_keys(float cx, float y, float t)
{
    int x0 = iround(cx) - 7, yy = iround(y);
    for (int x = 0; x < 15; x++) {
        px(x0 + x, yy, C_G3);
        px(x0 + x, yy + 1, (x & 1) ? C_G2 : C_G3);
    }
    int lit = (int)(t * 11.0f) % 7;
    px(x0 + 1 + lit * 2, yy + 1, C_WHITE);
}

static void draw_hearts(float cx, float top, float t, float amount)
{
    static const char *const HEART[] = { ".#.#.", "#o###", "#####", ".###.", "..#.." };
    for (int i = 0; i < 2; i++) {
        float ph = t * 0.9f + i * 0.5f;
        ph -= floorf(ph);
        if (amount * (1 - ph) < 0.15f) {
            continue;
        }
        int x = iround(cx + (i ? 10 : -10) + sinf(t * 3 + i) * 1.5f), y = iround(top + 4 - ph * 12);
        stamp(HEART, 5, x, y, C_HEART, C_WHITE);
    }
}

/* ---- the face -------------------------------------------------------------- */

static void draw_eye(int x, int y, int style, float open, float gx, float gy)
{
    static const char *const BEAD[] = { "##", "o#", "##" };
    static const char *const BIG[] = { ".##.", "#o##", "####", ".##." };
    static const char *const HAPPY[] = { ".##.", "#..#" };
    static const char *const XS[] = { "#.#", ".#.", "#.#" };
    static const char *const SHUT[] = { "###" };
    int ox = iround(gx), oy = iround(gy * 0.8f);
    switch (style) {
    case 1: stamp(BIG, 4, x + ox, y + oy, C_EYE, C_SHINE); break;
    case 2: stamp(HAPPY, 2, x, y, C_EYE, C_EYE); break;
    case 3: stamp(XS, 3, x, y, C_EYE, C_EYE); break;
    default:
        if (open < 0.5f) {
            stamp(SHUT, 1, x, y + 1, C_EYE, C_EYE);
        } else {
            stamp(BEAD, 3, x + ox, y + oy, C_EYE, C_SHINE);
        }
    }
}

enum { M_SMILE, M_O, M_HMM, M_FLAT, M_TALK, M_GRIN };

static void draw_mouth(int x, int y, int m, float open)
{
    static const char *const SMILE[] = { "#..#", ".##." };
    static const char *const O[] = { ".#.", "#.#", ".#." };
    static const char *const HMM[] = { "..#", "##." };
    static const char *const FLAT[] = { "###" };
    static const char *const GRIN[] = { "#####", ".#o#.", "..#.." };
    switch (m) {
    case M_O: stamp(O, 3, x, y + 1, C_MOUTH, C_MOUTH); break;
    case M_HMM: stamp(HMM, 2, x, y, C_MOUTH, C_MOUTH); break;
    case M_FLAT: stamp(FLAT, 1, x, y, C_MOUTH, C_MOUTH); break;
    case M_GRIN: stamp(GRIN, 3, x, y + 1, C_MOUTH, C_TONGUE); break;
    case M_TALK: {
        int h = 1 + iround(open * 3.0f);
        for (int i = 0; i < h; i++) {
            for (int dx = -1; dx <= 1; dx++) {
                px(x + dx, y + i, (i > 0 && i == h - 1 && dx == 0) ? C_TONGUE : C_MOUTH);
            }
        }
        break;
    }
    default: stamp(SMILE, 2, x, y, C_MOUTH, C_MOUTH); break;
    }
}

/* ---- a frame ------------------------------------------------------------------ */

static void render(const msw_avatar_pose_t *p, uint8_t *grid, uint16_t *pal)
{
    static bool ready;
    static float last_t = -1;
    static muse_speaking_t speech;
    static pt_t hands[2];
    if (!ready) {
        tables();
        ready = true;
    }
    g = grid;
    float dt = last_t >= 0 ? clampf(p->t - last_t, 0, 0.2f) : 0.04f;
    last_t = p->t;
    float t = p->t, mt = p->mode_t, level = p->level, happy = p->happy;
    int mode = p->mode < 0 || p->mode > MSW_MODE_OFF ? MSW_MODE_IDLE : p->mode;
    bool working = mode == MSW_MODE_THINKING && p->working > 0.5f;
    if (mode == MSW_MODE_ERROR) {
        happy = 0;
    }
    muse_speaking_update(&speech, mode == MSW_MODE_SPEAKING, dt, mt, level);

    float fade = mode == MSW_MODE_OFF ? clampf(1.0f - mt / 1.3f, 0, 1) : 1.0f;
    palette(pal, working ? &WORKING : &SCHEMES[mode], dt, 0.25f + 0.75f * fade);
    float shut = eyes_update(t, dt, mode, working);

    memset(grid, C_BG, W * H);

    /* ---- body motion ---- */
    float bob, lean = 0, hop = 0;
    switch (mode) {
    case MSW_MODE_LISTENING: bob = sinf(t * 3.0f) * 0.5f; break;
    case MSW_MODE_THINKING:
        bob = working ? sinf(t * 4.2f) * 0.5f : sinf(t * 2.4f) * 0.8f;
        lean = working ? 0 : sinf(t * 1.3f) * 1.2f;
        break;
    case MSW_MODE_SPEAKING:
        bob = sinf(t * 2.5f) * 0.25f - speech.energy * 0.6f;
        lean = (speech.lift[1] - speech.lift[0]) * 0.65f;
        break;
    case MSW_MODE_ERROR:
        bob = 1.0f;
        lean = mt < 0.6f ? sinf(t * 18.0f) * 1.2f : 0;
        break;
    default: bob = sinf(t * 1.8f) * 1.0f; break;
    }
    if (happy > 0) {
        hop = fabsf(sinf(t * 9.0f)) * 3.0f * happy;
    }
    float boot = mode == MSW_MODE_BOOT ? clampf(mt / 1.4f, 0, 1) : 1.0f;
    float pop = mode == MSW_MODE_BOOT ? clampf(mt / 0.6f, 0, 1) : 1.0f;
    float squash = 1.0f - (1.0f - pop) * 0.35f + sinf(pop * 3.1416f) * 0.06f;
    float breathe = sinf(t * 2.0f + 1.0f) * 0.03f;
    float rx = 16.5f * (1 + breathe) * (2.0f - squash) + level * 0.6f;
    float ry = 15.5f * (1 - breathe) * squash;
    float cx = 32.0f + lean;
    float cy = 56.5f - ry + bob * 0.5f - hop;
    float top = cy - 2 * ry;

    /* ---- behind ---- */
    draw_aura(cx, cy - 8, 30.0f + level * 3.0f + sinf(t * 1.5f), (0.75f * boot + level * 0.4f) * fade);
    if (mode == MSW_MODE_LISTENING) {
        draw_rings(cx, cy - 4, t, level, 0.9f);
    } else if (mode == MSW_MODE_SPEAKING) {
        draw_rings(cx, cy - 4, t, level, 0.6f);
    }
    draw_shadow(cx, 58.5f, 12.0f - hop * 0.8f);
    float spk = working ? 1.8f : mode == MSW_MODE_THINKING ? 2.8f : mode == MSW_MODE_LISTENING ? 1.2f
              : mode == MSW_MODE_SPEAKING ? 1.5f : 0.6f;
    int nspk = mode == MSW_MODE_BOOT ? (int)(boot * 6) : working ? 3 : (int)(6 * fade);
    draw_sparkles(cx, cy, t, spk, nspk, false);

    /* ---- the drop and its hands ---- */
    draw_drop(cx, cy, rx, ry);

    float side_x = rx + 0.5f, hy = cy + 4.0f;
    pt_t want[2];
    for (int i = 0; i < 2; i++) {
        float s = i ? 1.0f : -1.0f;
        float sway = sinf(t * 1.8f + 0.6f + i * 3.1416f) * 0.6f;
        want[i] = (pt_t){ cx + s * side_x, hy + sway };
        switch (mode) {
        case MSW_MODE_LISTENING: want[i] = (pt_t){ cx + s * (rx - 1.5f), cy - 3.0f }; break;
        case MSW_MODE_THINKING:
            if (working) {
                float tap = fmaxf(0, sinf(t * 11.0f) * (i ? -1 : 1)) * 1.5f;
                want[i] = (pt_t){ cx + s * 5.0f, cy + 9.0f - tap };
            } else if (i) {
                want[i] = (pt_t){ cx + 5.0f, cy + 8.0f };          /* to the chin */
            }
            break;
        case MSW_MODE_SPEAKING:
            want[i].x += s * (speech.extend[i] * 3.2f - speech.inward[i] * 8.5f);
            want[i].y -= speech.lift[i] * 10.0f + speech.extend[i] * 2.0f;
            break;
        case MSW_MODE_OFF:
            if (i) {
                want[i] = (pt_t){ cx + rx + 1.5f, cy - 6.0f + sinf(t * 12.0f) * 1.5f * fade };
            }
            break;
        default: break;
        }
        if (happy > 0.05f && mode != MSW_MODE_ERROR) {
            want[i] = (pt_t){ cx + s * (rx + 1.5f), cy - 8.0f + sinf(t * 14.0f + i * 3.1416f) * 1.2f };
        }
    }
    float hk = 1.0f - expf(-dt / 0.07f);       /* hands ease to their pose (and in and out of speaking) */
    for (int i = 0; i < 2; i++) {
        if (hands[i].x == 0) {
            hands[i] = want[i];
        }
        hands[i].x += (want[i].x - hands[i].x) * hk;
        hands[i].y += (want[i].y - hands[i].y) * hk;
        hands[i].x = clampf(hands[i].x, 3, W - 4);
        hands[i].y = clampf(hands[i].y, 3, H - 4);
        draw_hand(hands[i]);
    }
    outline((int)(cx - rx) - 6, (int)top - 2, (int)(cx + rx) + 6, (int)(cy + ry) + 2);
    /* the shine, upper left */
    disc(cx - rx * 0.45f, cy - ry * 0.6f, 2.0f * boot, C_SHINE);
    px(iround(cx - rx * 0.2f), iround(cy - ry * 1.15f), C_SHINE);

    /* ---- the face ---- */
    float fx = cx + lean * 0.3f, fy = cy + 1.0f + speech.nod * 0.8f;
    int ex = iround(rx * 0.36f);
    int style = 0;
    float open = 1.0f - shut;
    int mouth = M_SMILE;
    switch (mode) {
    case MSW_MODE_BOOT: open = mt < 0.9f ? 0 : 1; break;
    case MSW_MODE_LISTENING: style = 1; mouth = M_O; break;
    case MSW_MODE_THINKING: mouth = working ? M_SMILE : M_HMM; break;
    case MSW_MODE_SPEAKING:
        mouth = M_TALK;
        if (speech.eye_wide > 0.4f) {
            style = 1;
        }
        break;
    case MSW_MODE_ERROR: style = 3; mouth = M_FLAT; break;
    case MSW_MODE_OFF: open = mt < 0.7f ? open : 0; break;
    default: break;
    }
    if (happy > 0.2f && mode != MSW_MODE_ERROR) {
        style = 2;
        mouth = M_GRIN;
    }
    float gx = mode == MSW_MODE_SPEAKING ? clampf(s_gx + speech.gaze, -1, 1) : s_gx;
    int eyx = iround(fx), eyy = iround(fy);
    draw_eye(eyx - ex - 1, eyy, style, open, gx, s_gy);
    draw_eye(eyx + ex, eyy, style, open, gx, s_gy);

    /* brows for the expressive states */
    int by = eyy - 4;
    if (working || mode == MSW_MODE_LISTENING) {
        px(eyx - ex - 2, by - 1, C_EYE); px(eyx - ex - 1, by - 1, C_EYE);
        px(eyx + ex, by - 1, C_EYE); px(eyx + ex + 1, by - 1, C_EYE);
    } else if (mode == MSW_MODE_THINKING) {
        px(eyx - ex - 2, by + 1, C_EYE); px(eyx - ex - 1, by + 1, C_EYE);
        px(eyx + ex, by, C_EYE); px(eyx + ex + 1, by - 1, C_EYE);
    } else if (mode == MSW_MODE_SPEAKING && speech.brow > 0.2f) {
        int raise = iround(speech.brow * 2);
        px(eyx - ex - 2, by - raise, C_EYE); px(eyx - ex - 1, by - raise, C_EYE);
        px(eyx + ex, by - raise, C_EYE); px(eyx + ex + 1, by - raise, C_EYE);
    }
    float blush = 0.5f + happy * 0.5f + (mode == MSW_MODE_SPEAKING ? 0.15f : 0);
    if (blush > 0.45f) {
        disc(fx - ex - 5.0f, fy + 3.5f, 1.2f + blush * 0.6f, C_BLUSH);
        disc(fx + ex + 4.0f, fy + 3.5f, 1.2f + blush * 0.6f, C_BLUSH);
    }
    draw_mouth(eyx, eyy + 4, mouth, speech.mouth);

    /* ---- in front ---- */
    draw_sparkles(cx, cy, t, spk, nspk, true);
    if (working) {
        draw_keys(cx, cy + 11.5f, t);
        draw_spinner(cx + 17, top + 6, t);
    } else if (mode == MSW_MODE_THINKING) {
        draw_thought_dots(cx + 11, top + 6, t);
    }
    if (happy > 0) {
        draw_hearts(cx, top + 4, t, happy);
    }
    if (mode == MSW_MODE_ERROR) {
        static const char *const BANG[] = { ".##.", ".##.", ".##.", ".##.", "....", ".##." };
        stamp(BANG, 6, iround(cx + 17), iround(top + 5), C_ACC, C_ACC);
    }
}

static const msw_avatar_def_t def = { render, accent };

int app_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    msw_avatar_define(&def);
    return 0;
}
