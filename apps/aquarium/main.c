/*
 * まるい水槽: a round fish bowl for the museStopWatch.
 *
 * Test app 3 of the mini-app runtime (motion sensor, per-pixel drawing,
 * boids, sound, vibration). The screen is the bowl. The water stays level
 * with real gravity when you tilt the watch, and sloshes when you shake it.
 * The fish swim as a school (boids) and scatter when the water is shaken.
 * Tap to drop food. Blue: show or hide the numbers.
 * Yellow: the app menu (go on / quit).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "msw_app.h"

#define SCR 466
#define CX 233
#define CY 233
#define R 230                  /* the bowl's inner radius */
#define LEVEL 60.0f            /* the surface, this far above the centre (against gravity) */
#define NW 96                  /* wave samples across the surface */
#define NFISH 16
#define NFOOD 12
#define NBUB 24

typedef struct {
    float x, y, vx, vy;
    uint16_t body, fin;
    float size;
    float scare;
} fish_t;

typedef struct {
    float x, y;
    bool on;
} dot_t;

static msw_gfx_t *fb;
static float s_dx = 0, s_dy = 1;          /* "down" in screen axes, smoothed */
static float s_wave[NW], s_wave_v[NW];
static fish_t s_fish[NFISH];
static dot_t s_food[NFOOD];
static dot_t s_bub[NBUB];
static float s_shake;                     /* recent shaking, 0.. */
static float s_prev_ax, s_prev_ay, s_prev_az;
static bool s_imu_ok, s_info;
static int s_span[SCR][2];                /* each row's x range inside the bowl */
static uint32_t s_t;
static int s_bubble_ms;

static float frand(void)
{
    return (msw_random() & 0xFFFF) / 65535.0f;
}

/* Signed distance of a point below the surface (positive = in the water). */
static float depth(float x, float y)
{
    float d = (x - CX) * s_dx + (y - CY) * s_dy;
    float t = (x - CX) * -s_dy + (y - CY) * s_dx;      /* along the surface */
    int i = (int)((t + R) * (NW - 1) / (2 * R));
    i = i < 0 ? 0 : i >= NW ? NW - 1 : i;
    return d + LEVEL - s_wave[i];
}

static bool in_bowl(float x, float y, float margin)
{
    float dx = x - CX, dy = y - CY, r = R - margin;
    return dx * dx + dy * dy < r * r;
}

/* ---- sensing and water ----------------------------------------------------- */

static void sense(float dt)
{
    msw_imu_t m;
    if (!msw_imu(&m)) {
        s_imu_ok = false;
        return;
    }
    s_imu_ok = true;
    /* gravity in the screen's plane is (-ax, -ay); flat on a table it is ~0 */
    float gx = -m.ax, gy = -m.ay, n = sqrtf(gx * gx + gy * gy);
    if (n > 0.2f) {
        float k = 0.15f * dt;
        s_dx += (gx / n - s_dx) * k;
        s_dy += (gy / n - s_dy) * k;
        float l = sqrtf(s_dx * s_dx + s_dy * s_dy);
        s_dx /= l;
        s_dy /= l;
    }
    /* shaking: how fast the acceleration changes */
    float j = fabsf(m.ax - s_prev_ax) + fabsf(m.ay - s_prev_ay) + fabsf(m.az - s_prev_az);
    float side = (m.ax - s_prev_ax) * -s_dy + (m.ay - s_prev_ay) * s_dx;
    s_prev_ax = m.ax;
    s_prev_ay = m.ay;
    s_prev_az = m.az;
    if (j > 0.25f) {
        s_shake += j;
        /* push the surface: the side the watch moved towards rises */
        for (int i = 0; i < NW; i++) {
            float p = (float)i / (NW - 1) * 2 - 1;
            s_wave_v[i] += j * 9.0f * (p * (side >= 0 ? 1 : -1) + 0.4f * sinf(i * 0.7f + s_t * 0.01f));
        }
        if (j > 1.2f) {
            msw_vibrate(40, (int)(j * 30));
        }
        if (j > 0.6f) {
            msw_sound(1, MSW_WAVE_NOISE, 900 + (int)(j * 500), 160, (int)fminf(40, j * 25), true);
        }
    }
    s_shake *= powf(0.9f, dt);
}

static void waves(float dt)
{
    /* the wave equation with damping, a few small steps a frame */
    for (int step = 0; step < 3; step++) {
        for (int i = 0; i < NW; i++) {
            float l = s_wave[i > 0 ? i - 1 : i], r = s_wave[i < NW - 1 ? i + 1 : i];
            s_wave_v[i] += ((l + r) / 2 - s_wave[i]) * 0.5f * dt - s_wave[i] * 0.004f * dt;
            s_wave_v[i] *= 0.985f;
        }
        float mean = 0;
        for (int i = 0; i < NW; i++) {
            s_wave[i] += s_wave_v[i] * dt * 0.33f;
            s_wave[i] = fmaxf(-90, fminf(90, s_wave[i]));
            mean += s_wave[i];
        }
        mean /= NW;
        for (int i = 0; i < NW; i++) {
            s_wave[i] -= mean;            /* water is not made or lost */
        }
    }
}

/* ---- fish ------------------------------------------------------------------------ */

static void fish_init(void)
{
    static const uint8_t colors[][6] = {
        { 255, 112, 67, 255, 204, 128 }, { 255, 238, 88, 255, 160, 0 }, { 79, 195, 247, 225, 245, 254 },
        { 240, 98, 146, 255, 205, 210 }, { 255, 255, 255, 255, 138, 101 },
    };
    for (int i = 0; i < NFISH; i++) {
        fish_t *f = &s_fish[i];
        const uint8_t *c = colors[i % 5];
        f->body = MSW_RGB(c[0], c[1], c[2]);
        f->fin = MSW_RGB(c[3], c[4], c[5]);
        f->size = 5 + frand() * 4;
        f->x = CX + (frand() - 0.5f) * 200;
        f->y = CY + 40 + (frand() - 0.5f) * 120;
        float a = frand() * 6.283f;
        f->vx = cosf(a) * 1.5f;
        f->vy = sinf(a) * 1.5f;
    }
}

static void fish_step(float dt)
{
    for (int i = 0; i < NFISH; i++) {
        fish_t *f = &s_fish[i];
        float sx = 0, sy = 0, ax = 0, ay = 0, cx = 0, cy = 0;
        int n = 0;
        for (int k = 0; k < NFISH; k++) {
            if (k == i) {
                continue;
            }
            float dx = s_fish[k].x - f->x, dy = s_fish[k].y - f->y, d2 = dx * dx + dy * dy;
            if (d2 < 70 * 70) {
                n++;
                ax += s_fish[k].vx;
                ay += s_fish[k].vy;
                cx += dx;
                cy += dy;
                if (d2 < 22 * 22 && d2 > 0.01f) {
                    sx -= dx / d2 * 22;
                    sy -= dy / d2 * 22;
                }
            }
        }
        float fx = sx * 0.6f, fy = sy * 0.6f;
        if (n) {
            fx += (ax / n - f->vx) * 0.05f + cx / n * 0.004f;
            fy += (ay / n - f->vy) * 0.05f + cy / n * 0.004f;
        }
        /* food */
        float best = 1e9f;
        int target = -1;
        for (int k = 0; k < NFOOD; k++) {
            if (!s_food[k].on) {
                continue;
            }
            float dx = s_food[k].x - f->x, dy = s_food[k].y - f->y, d2 = dx * dx + dy * dy;
            if (d2 < best) {
                best = d2;
                target = k;
            }
        }
        if (target >= 0) {
            float d = sqrtf(best) + 0.01f;
            fx += (s_food[target].x - f->x) / d * 0.12f;
            fy += (s_food[target].y - f->y) / d * 0.12f;
            if (d < f->size + 3) {
                s_food[target].on = false;
                msw_sound(2, MSW_WAVE_SINE, 1400 + (int)(frand() * 400), 50, 25, true);
            }
        }
        /* stay in the water: the bowl's wall and the surface push back */
        float r = sqrtf((f->x - CX) * (f->x - CX) + (f->y - CY) * (f->y - CY)) + 0.01f;
        if (r > R - 40) {
            fx -= (f->x - CX) / r * (r - (R - 40)) * 0.02f;
            fy -= (f->y - CY) / r * (r - (R - 40)) * 0.02f;
        }
        float under = depth(f->x, f->y);
        if (under < 30) {
            fx += s_dx * (30 - under) * 0.02f;
            fy += s_dy * (30 - under) * 0.02f;
        }
        /* shaken: dart away in panic */
        if (s_shake > 1.0f && f->scare <= 0) {
            f->scare = 30 + frand() * 30;
            float a = frand() * 6.283f;
            f->vx += cosf(a) * 4;
            f->vy += sinf(a) * 4;
        }
        if (f->scare > 0) {
            f->scare -= dt;
        }
        f->vx += fx * dt;
        f->vy += fy * dt;
        float sp = sqrtf(f->vx * f->vx + f->vy * f->vy) + 0.001f;
        float max = f->scare > 0 ? 4.5f : 1.8f, min = 0.6f;
        if (sp > max) {
            f->vx *= max / sp;
            f->vy *= max / sp;
        } else if (sp < min) {
            f->vx *= min / sp;
            f->vy *= min / sp;
        }
        f->x += f->vx * dt;
        f->y += f->vy * dt;
        if (!in_bowl(f->x, f->y, 12)) {
            f->x = CX + (f->x - CX) * 0.97f;
            f->y = CY + (f->y - CY) * 0.97f;
        }
    }
}

static void fish_draw(const fish_t *f)
{
    float sp = sqrtf(f->vx * f->vx + f->vy * f->vy) + 0.001f;
    float ux = f->vx / sp, uy = f->vy / sp, s = f->size;
    float wag = sinf(s_t * 0.02f + f->size * 3) * 0.5f;
    int tx = (int)(f->x - ux * s * 1.6f), ty = (int)(f->y - uy * s * 1.6f);
    for (int k = 0; k < 3; k++) {          /* a fanned tail */
        float a = (k - 1) * 0.5f + wag;
        float bx = -ux * cosf(a) + uy * sinf(a), by = -uy * cosf(a) - ux * sinf(a);
        msw_gfx_line(fb, tx, ty, (int)(tx + bx * s * 1.4f), (int)(ty + by * s * 1.4f), f->fin);
    }
    msw_gfx_fill_circle(fb, (int)(f->x - ux * s * 0.7f), (int)(f->y - uy * s * 0.7f), (int)(s * 0.8f), f->body);
    msw_gfx_fill_circle(fb, (int)f->x, (int)f->y, (int)s, f->body);
    msw_gfx_fill_circle(fb, (int)(f->x + ux * s * 0.5f - uy * s * 0.3f), (int)(f->y + uy * s * 0.5f + ux * s * 0.3f),
                        2, MSW_RGB(20, 20, 30));
}

/* ---- drawing ----------------------------------------------------------------------- */

static uint16_t s_deep[321];              /* water colour by depth */

/* The bowl in 2 x 2 blocks: air above the surface, water below, by depth.
 * Depth and the place along the surface change linearly along a row, so
 * they are stepped, not worked out for each pixel. Outside the circle stays
 * black from the start. */
static void draw_water(void)
{
    const uint16_t air = MSW_RGB(18, 26, 38), line = MSW_RGB(190, 235, 255);
    const float to_i = (NW - 1) / (2.0f * R);
    for (int y = 0; y < SCR; y += 2) {
        uint16_t *row = fb->px + y * fb->stride;
        int x0 = s_span[y][0] & ~1, x1 = s_span[y][1];
        float d = (x0 - CX) * s_dx + (y - CY) * s_dy + LEVEL;
        float t = ((x0 - CX) * -s_dy + (y - CY) * s_dx + R) * to_i;
        float dd = 2 * s_dx, dt = -2 * s_dy * to_i;
        for (int x = x0; x < x1; x += 2, d += dd, t += dt) {
            int i = (int)t;
            i = i < 0 ? 0 : i >= NW ? NW - 1 : i;
            int k = (int)(d - s_wave[i]);
            uint16_t c = k < 0 ? air : k < 4 ? line : s_deep[k > 320 ? 320 : k];
            row[x] = c;
            row[x + 1] = c;
        }
        if (y + 1 < SCR) {
            memcpy(row + fb->stride + x0, row + x0, (size_t)(x1 - x0) * 2);
        }
    }
}

static void draw_bowl(void)
{
    /* sand and weeds sit on the bowl's bottom (the screen's), whatever the tilt */
    for (int y = CY + 150; y < SCR; y++) {
        int x0 = s_span[y][0], x1 = s_span[y][1];
        int wave = (int)(4 * sinf(y * 0.3f));
        uint16_t sand = (y + wave) % 7 ? MSW_RGB(215, 185, 142) : MSW_RGB(190, 160, 120);
        msw_gfx_fill_rect(fb, x0, y, x1 - x0, 1, sand);
    }
    static const int weeds[][2] = { { 120, 60 }, { 150, 90 }, { 300, 110 }, { 330, 70 }, { 360, 50 }, { 200, 45 } };
    for (int w = 0; w < 6; w++) {
        int bx = weeds[w][0], h = weeds[w][1], by = CY + 165;
        int px = bx, py = by;
        for (int k = 1; k <= 10; k++) {
            float sway = sinf(s_t * 0.004f + w + k * 0.4f) * k * 0.9f;
            int nx = bx + (int)sway, ny = by - h * k / 10;
            msw_gfx_line(fb, px, py, nx, ny, MSW_RGB(67, 160, 71));
            msw_gfx_line(fb, px + 1, py, nx + 1, ny, MSW_RGB(46, 125, 50));
            px = nx;
            py = ny;
        }
    }
    msw_gfx_fill_circle(fb, 250, CY + 172, 16, MSW_RGB(130, 120, 110));
    msw_gfx_fill_circle(fb, 272, CY + 176, 11, MSW_RGB(150, 140, 128));
    /* the glass */
    msw_gfx_circle(fb, CX, CY, R, MSW_RGB(180, 220, 240));
    msw_gfx_circle(fb, CX, CY, R + 1, MSW_RGB(120, 160, 190));
    for (int k = 0; k < 40; k++) {                 /* a highlight on the glass */
        float a = 3.6f + k * 0.012f;
        msw_gfx_pixel(fb, CX + (int)(cosf(a) * (R - 14)), CY + (int)(sinf(a) * (R - 14)), MSW_RGB(230, 245, 255));
        msw_gfx_pixel(fb, CX + (int)(cosf(a) * (R - 15)), CY + (int)(sinf(a) * (R - 15)), MSW_RGB(230, 245, 255));
    }
}

static void draw(void)
{
    draw_water();
    draw_bowl();
    for (int i = 0; i < NFOOD; i++) {
        if (s_food[i].on) {
            msw_gfx_fill_circle(fb, (int)s_food[i].x, (int)s_food[i].y, 2, MSW_RGB(161, 110, 60));
        }
    }
    for (int i = 0; i < NBUB; i++) {
        if (s_bub[i].on) {
            msw_gfx_circle(fb, (int)s_bub[i].x, (int)s_bub[i].y, 3, MSW_RGB(225, 245, 255));
        }
    }
    for (int i = 0; i < NFISH; i++) {
        fish_draw(&s_fish[i]);
    }
    if (s_info) {
        char buf[64];
        snprintf(buf, sizeof(buf), "down %.2f %.2f  shake %.1f", s_dx, s_dy, s_shake);
        msw_gfx_text(fb, 110, 70, s_imu_ok ? buf : "no motion sensor", MSW_RGB(255, 255, 255), 14);
    }
    msw_fb_present();
}

/* ---- food and bubbles ------------------------------------------------------------- */

static void particles(float dt, uint32_t dt_ms)
{
    for (int i = 0; i < NFOOD; i++) {
        dot_t *p = &s_food[i];
        if (!p->on) {
            continue;
        }
        float d = depth(p->x, p->y);
        float v = d < 0 ? 3.0f : 0.5f;                 /* falls fast in air, sinks slowly */
        p->x += s_dx * v * dt + sinf(s_t * 0.01f + i) * 0.2f;
        p->y += s_dy * v * dt;
        if (!in_bowl(p->x, p->y, 8) || p->y > CY + 160) {
            p->on = false;
        }
    }
    s_bubble_ms -= (int)dt_ms;
    if (s_bubble_ms <= 0) {
        s_bubble_ms = 300 + (int)(frand() * 700);
        for (int i = 0; i < NBUB; i++) {
            if (!s_bub[i].on) {
                s_bub[i] = (dot_t){ 255 + frand() * 10, CY + 155, true };
                break;
            }
        }
    }
    for (int i = 0; i < NBUB; i++) {
        dot_t *b = &s_bub[i];
        if (!b->on) {
            continue;
        }
        b->x -= s_dx * 1.4f * dt + sinf(s_t * 0.02f + i) * 0.3f;
        b->y -= s_dy * 1.4f * dt;
        if (depth(b->x, b->y) < 2 || !in_bowl(b->x, b->y, 6)) {
            b->on = false;
            if (frand() < 0.3f) {
                msw_sound(3, MSW_WAVE_SINE, 900 + (int)(frand() * 800), 40, 10, true);
            }
        }
    }
}

static void touch(void)
{
    static bool was;
    int x, y;
    bool now = msw_touch(&x, &y);
    if (now && !was) {
        for (int k = 0; k < 4; k++) {
            for (int i = 0; i < NFOOD; i++) {
                if (!s_food[i].on) {
                    s_food[i] = (dot_t){ x + (frand() - 0.5f) * 20, y + (frand() - 0.5f) * 10, true };
                    break;
                }
            }
        }
        msw_sound_slide(2, MSW_WAVE_TRIANGLE, 600, 1200, 80, 20);
    }
    was = now;
}

/* ---- the callbacks ------------------------------------------------------------------- */

static void tick(uint32_t dt_ms, void *user)
{
    (void)user;
    float dt = dt_ms / 33.3f;
    dt = dt > 2.5f ? 2.5f : dt;
    s_t += dt_ms;
    sense(dt);
    waves(dt);
    touch();
    fish_step(dt);
    particles(dt, dt_ms);
    draw();
}

static void button(int b, bool down, void *user)
{
    (void)b;
    (void)user;
    if (down) {
        s_info = !s_info;
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
    for (int y = 0; y < SCR; y++) {
        float dy = y + 0.5f - CY;
        float w = R * R - dy * dy;
        int half = w > 0 ? (int)sqrtf(w) : 0;
        s_span[y][0] = CX - half;
        s_span[y][1] = CX + half;
    }
    for (int k = 0; k <= 320; k++) {
        int q = k > 300 ? 300 : k;
        s_deep[k] = MSW_RGB(30 - q / 12, 150 - q / 3, 230 - q / 3);
    }
    fish_init();
    msw_set_fps(25);
    msw_on_tick(tick, NULL);
    msw_on_button(button, NULL);
    msw_log("%d fish", NFISH);
    return 0;
}
