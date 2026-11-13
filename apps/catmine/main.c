#include "msw_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ねこマインスイーパー: 地雷の代わりに猫が隠れているマインスイーパー。
 * 盤面 8x8、猫 10 匹。タップでマスを開き、長押しで旗を立てる。
 * 青ボタンではたモード切替（タップで旗を置くモード）。
 * 最初に開いたマスとその周りには猫がいない（安全スタート）。
 * 終わり方は黄ボタンの標準メニュー。 */

#define COLS 8
#define ROWS 8
#define NCATS 10
#define CELL 36
#define BX ((466 - COLS * CELL) / 2)   /* 89 */
#define BY 100
#define LONG_PRESS_MS 480

static msw_gfx_t *fb;
static msw_sprite_t *cat_spr;

static bool is_cat[ROWS][COLS];
static bool is_open[ROWS][COLS];
static bool is_flag[ROWS][COLS];
static int cell_num[ROWS][COLS];

static bool cats_placed;
static int opened_count, flag_count;
static int game_state;                 /* 0: プレイ中, 1: クリア, 2: 猫を踏んだ */
static int hit_x, hit_y;
static bool flag_mode;
static uint32_t start_ms;              /* 0: まだ最初のマスを開いていない */
static uint32_t end_ms;
static uint32_t frozen_sec;

/* 猫の顔スプライト (16x12) */
static const char *const CAT_ART[12] = {
    ".KK..........KK.",
    ".KPK........KPK.",
    ".KKKKKKKKKKKKKK.",
    ".KOOOOOOOOOOOOK.",
    ".KOOOOOOOOOOOOK.",
    ".KOOEOOOOOOEOOK.",
    ".KOOOOOOOOOOOOK.",
    ".KOOOOOPPOOOOOK.",
    ".KOOOOKOOKOOOOK.",
    ".KOOOOOOOOOOOOK.",
    "..KOOOOOOOOOOK..",
    "...KKKKKKKKKK...",
};
static const msw_pal_t CAT_PAL[] = {
    {'K', MSW_RGB(64, 44, 32)},
    {'O', MSW_RGB(246, 169, 59)},
    {'P', MSW_RGB(255, 130, 160)},
    {'E', MSW_RGB(30, 26, 24)},
    {0, 0},
};

static const uint16_t NUM_COLOR[9] = {
    0,
    MSW_RGB(60, 90, 220), MSW_RGB(40, 160, 60), MSW_RGB(225, 60, 50),
    MSW_RGB(130, 60, 190), MSW_RGB(190, 110, 20), MSW_RGB(20, 160, 170),
    MSW_RGB(40, 40, 40), MSW_RGB(120, 120, 120),
};

/* ---- 猫の鳴き声シンセ (16kHz PCM, 全量プリレンダリングして送出) -----
 * 鳴き声は「声」として組み立てるソース・フィルタ方式にした。
 * 倍音を含む音源を、口の開閉で動く2つの共鳴 (フォルマント F1/F2) に通す。
 *  - 立ち上がり: 鼻音「ン」のこもった低い響き (音量小、くぐもり気味)
 *  - 「ニャ」の移行: F2 が高い位置 (口蓋ロカス) から下がり、F1 が開く
 *  - 母音部: F1 が開いて明るい「ア」、語尾で口が閉じて暗く消える
 *  - 音程: 短く上がって山を越し、緩やかに下降。震えは控えめ (+-1.4%) */
#define PI_F 3.14159265f

typedef struct {
    int dur;             /* サンプル数 (16kHz) */
    float f0s, f0p, f0e; /* 開始 / 山 / 終わりの基本周波数 Hz */
    float gain;
    int gap_after;       /* 次セグメントまでの無音サンプル数 */
    int sharp;           /* 1: 短い驚きの「にゃっ」、0: 長い「にゃーん」 */
} meow_seg_t;

static meow_seg_t meow_q[3];
static int meow_qn, meow_qi;
static int meow_t;            /* 現在セグメント内の経過サンプル */
static int meow_gap;
static float meow_phase;      /* 基本波の位相 (周) */
static float meow_vphase;     /* ビブラートの位相 (周) */
static uint32_t meow_lfsr = 0x2545F491u;

/* バンドパス (共鳴) の状態 */
typedef struct { float x1, x2, y1, y2; } bp_t;
static bp_t bp_f1, bp_f2;
static float lp_body;         /* 低域の胴体成分用ローパスの状態 */
static float lp_nose;         /* 鼻音のくぐもり用ローパスの状態 */
static float rough_s;         /* 声のざらつき用 平滑ノイズ */

/* 鳴き声は丸ごと先に生成してから送る (プリレンダリング)。
 * 以前は tick ごとに最大800サンプルずつ作って送っていたが、実測の tick は
 * 約18回/秒しかなく、供給が 16kHz の実時間消費に届かずバッファが枯れて
 * ブツブツ切れた。全量を先に作り、開始時にデバイス側バッファへ入るだけ
 * 先行投入し、あとは tick ごとに空きへ補充し続ける。 */
#define MEOW_BUF_SAMPLES 15000          /* 最長構成 8000+1700+5100=14800 */
static int16_t meow_buf[MEOW_BUF_SAMPLES];
static int meow_total;                  /* 生成済みサンプル数 */
static int meow_sent;                   /* デバイスへ渡し終えた数 */

static float smooth01(float x) {
    if (x < 0) return 0;
    if (x > 1) return 1;
    return x * x * (3.0f - 2.0f * x);
}

/* RBJ バンドパス (ピークゲイン 0dB) で 1 サンプル通す */
static float bp_run(bp_t *b, float x, float freq, float bw) {
    float w = 2.0f * PI_F * freq / 16000.0f;
    float alpha = sinf(w) / (2.0f * (freq / bw));
    float cw = cosf(w);
    float a0 = 1.0f + alpha;
    float y = (alpha * (x - b->x2) + 2.0f * cw * b->y1
               - (1.0f - alpha) * b->y2) / a0;
    b->x2 = b->x1; b->x1 = x;
    b->y2 = b->y1; b->y1 = y;
    return y;
}

/* 次の 1 サンプルを返す。鳴き声が終わっていたら 0 で、meow_qi が meow_qn に達する */
static float meow_next_sample(void) {
    if (meow_gap > 0) { meow_gap--; return 0.0f; }
    if (meow_qi >= meow_qn) return 0.0f;
    const meow_seg_t *s = &meow_q[meow_qi];
    float u = (float)meow_t / (float)s->dur;
    int sh = s->sharp;

    float nas = sh ? 0.055f : 0.095f;  /* 鼻音「ン」が終わる位置 */
    float ris = sh ? 0.21f : 0.36f;    /* 音程の山・全開に着く位置 */
    float clo = sh ? 0.45f : 0.62f;    /* 口が閉じ始める位置 */

    /* 音程: 鼻音で少しせり上がり、山へ滑らかに、後は緩やかに下降 */
    float f;
    if (u < nas)      f = s->f0s * (1.0f + 0.07f * (u / nas));
    else if (u < ris) f = s->f0s * 1.07f
                        + (s->f0p - s->f0s * 1.07f) * smooth01((u - nas) / (ris - nas));
    else              f = s->f0p
                        + (s->f0e - s->f0p) * smooth01((u - ris) / (1.0f - ris));
    meow_vphase += 5.2f / 16000.0f;
    float venv = smooth01((u - 0.28f) / 0.25f) * (1.0f - smooth01((u - 0.85f) / 0.15f));
    f *= 1.0f + 0.014f * venv * sinf(2.0f * PI_F * meow_vphase);
    meow_phase += f / 16000.0f;
    if (meow_phase >= 1.0f) meow_phase -= 1.0f;

    /* 音源: 倍音を 1/k 気味に重ね、かすかな息を混ぜる */
    static const float harm[8] = {1.0f, 0.52f, 0.34f, 0.24f,
                                  0.17f, 0.12f, 0.09f, 0.06f};
    float ph = 2.0f * PI_F * meow_phase;
    float src = 0.0f;
    for (int k = 0; k < 8; k++) {
        if (f * (float)(k + 1) > 7600.0f) break;
        src += harm[k] * sinf((float)(k + 1) * ph);
    }
    meow_lfsr = meow_lfsr * 1664525u + 1013904223u;
    float nz = ((int)(meow_lfsr >> 24) - 128) / 128.0f;
    rough_s += 0.06f * (nz - rough_s);
    src += 0.05f * nz;

    /* 口の開き具合 o: 鼻音で 0、母音部で 1、語尾で少し閉じる */
    float o;
    if (u < clo) o = smooth01((u - nas * 0.5f) / (ris - nas * 0.5f));
    else         o = 1.0f - 0.78f * smooth01((u - clo) / (1.0f - clo));
    float f1 = 315.0f + 725.0f * o;
    float f2v;
    if (u < clo) f2v = 1630.0f - 620.0f * o;
    else         f2v = 1010.0f - 260.0f * smooth01((u - clo) / (1.0f - clo));

    float y1 = bp_run(&bp_f1, src, f1, 115.0f);
    float y2 = bp_run(&bp_f2, src, f2v, 140.0f);
    lp_body += 0.10f * (src - lp_body);
    float v = 1.05f * y1 + 0.55f * y2 + 0.30f * lp_body;

    /* 鼻音の間は全体をくぐもらせる (鼻から出る「ン」の感じ) */
    lp_nose += 0.055f * (v - lp_nose);
    float m = 1.0f - smooth01((u - nas * 0.5f) / (nas * 1.1f));
    v = v * (1.0f - m) + lp_nose * m * 1.15f;

    /* 声のざらつき (弱い振幅の揺れ) */
    float rf = smooth01((u - 0.12f) / 0.20f) * (1.0f - smooth01((u - 0.90f) / 0.10f));
    v *= 1.0f + (sh ? 0.05f : 0.028f) * rf * rough_s;

    /* 音量: 鼻音は小さく、開くと膨らみ、閉じながら消える */
    float env;
    if (sh) {
        float lvl = 0.45f + 0.55f * smooth01((u - nas * 0.5f) / (0.16f - nas * 0.5f));
        lvl *= 1.0f - smooth01((u - 0.80f) / 0.20f);
        env = smooth01(u / 0.03f) * lvl;
    } else {
        float lvl = 0.38f + 0.62f * smooth01((u - nas * 0.5f) / (0.26f - nas * 0.5f));
        float dd = (u - 0.155f) / 0.045f;
        lvl *= 1.0f - 0.16f * expf(-dd * dd);   /* 「にゃ」への移行の小さな凹み */
        lvl *= 1.0f - smooth01((u - 0.66f) / 0.34f);
        env = smooth01(u / 0.045f) * lvl;
    }

    meow_t++;
    if (meow_t >= s->dur) {
        meow_t = 0;
        meow_gap = s->gap_after;
        meow_qi++;
    }
    float out = v * env * s->gain * 0.55f;
    if (out > 0.95f) out = 0.95f;
    if (out < -0.95f) out = -0.95f;
    return out;
}

static bool meow_busy(void) { return meow_sent < meow_total; }

/* デバイス側バッファへ、受け付けてもらえる限り一気に渡す */
static void meow_feed(void) {
    while (meow_sent < meow_total) {
        int want = meow_total - meow_sent;
        int took = msw_pcm_write(meow_buf + meow_sent, want);
        if (took <= 0) break;
        meow_sent += took;
        if (took < want) break;         /* 一杯になったら次 tick で補充 */
    }
}

/* キューに入っている鳴き声を全量生成する */
static void meow_render(void) {
    meow_total = 0;
    meow_sent = 0;
    while (meow_total < MEOW_BUF_SAMPLES &&
           (meow_qi < meow_qn || meow_gap > 0))
        meow_buf[meow_total++] =
            (int16_t)(meow_next_sample() * 30000.0f);
}

static void meow_start(const meow_seg_t *segs, int n) {
    for (int i = 0; i < n && i < 3; i++) meow_q[i] = segs[i];
    meow_qn = n;
    meow_qi = 0;
    meow_t = 0;
    meow_gap = 0;
    meow_phase = 0;
    meow_vphase = 0;
    memset(&bp_f1, 0, sizeof(bp_f1));
    memset(&bp_f2, 0, sizeof(bp_f2));
    lp_body = lp_nose = rough_s = 0;
    meow_render();                      /* 全量を先に作る */
    meow_feed();                        /* バッファへ先行投入 */
    msw_log("catmine: meow samples %d primed %d", meow_total, meow_sent);
}

/* 猫を踏んだとき: 驚いた短い「にゃっ!」 */
static void meow_cry(void) {
    static const meow_seg_t segs[1] = {
        {7700, 690.0f, 1240.0f, 600.0f, 1.0f, 0, 1},
    };
    meow_start(segs, 1);
    msw_log("catmine: meow cry v3");
}

/* クリア時: 可愛い「にゃーん」に、小さな「にゃ」を添える */
static void meow_happy(void) {
    static const meow_seg_t segs[2] = {
        {8000, 760.0f, 1170.0f, 890.0f, 0.95f, 1700, 0},
        {5100, 960.0f, 1310.0f, 1050.0f, 0.85f, 0, 0},
    };
    meow_start(segs, 2);
    msw_log("catmine: meow happy v3");
}


static void new_game(void) {
    memset(is_cat, 0, sizeof(is_cat));
    memset(is_open, 0, sizeof(is_open));
    memset(is_flag, 0, sizeof(is_flag));
    memset(cell_num, 0, sizeof(cell_num));
    cats_placed = false;
    opened_count = 0;
    flag_count = 0;
    game_state = 0;
    hit_x = hit_y = -1;
    flag_mode = false;
    start_ms = 0;
    frozen_sec = 0;
    msw_log("catmine: new game");
}

static uint32_t elapsed_sec(void) {
    if (game_state != 0) return frozen_sec;
    if (start_ms == 0) return 0;
    return (msw_millis() - start_ms) / 1000;
}

static void place_cats(int sx, int sy) {
    int placed = 0;
    while (placed < NCATS) {
        int i = (int)(msw_random() % (COLS * ROWS));
        int cx = i % COLS, cy = i / COLS;
        if (is_cat[cy][cx]) continue;
        int dx = cx - sx; if (dx < 0) dx = -dx;
        int dy = cy - sy; if (dy < 0) dy = -dy;
        if (dx <= 1 && dy <= 1) continue;   /* 最初の一手の周りは安全 */
        is_cat[cy][cx] = true;
        placed++;
    }
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) continue;
                    if (is_cat[ny][nx]) n++;
                }
            cell_num[y][x] = n;
        }
}

static void finish(int state) {
    game_state = state;
    end_ms = msw_millis();
    frozen_sec = start_ms ? (end_ms - start_ms) / 1000 : 0;
    if (state == 1) {
        meow_happy();
        msw_log("catmine: clear in %u sec", (unsigned)frozen_sec);
    } else {
        meow_cry();
        msw_vibrate(250, 80);
        msw_log("catmine: stepped on a cat at %d,%d", hit_x, hit_y);
    }
}

static void open_from(int sx, int sy) {
    /* 空白マスの連鎖オープン（明示スタック） */
    int stack[COLS * ROWS];
    int sp = 0;
    stack[sp++] = sy * COLS + sx;
    while (sp > 0) {
        int i = stack[--sp];
        int x = i % COLS, y = i / COLS;
        if (is_open[y][x] || is_flag[y][x] || is_cat[y][x]) continue;
        is_open[y][x] = true;
        opened_count++;
        if (cell_num[y][x] != 0) continue;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= COLS || ny >= ROWS) continue;
                if (!is_open[ny][nx]) stack[sp++] = ny * COLS + nx;
            }
    }
}

static void do_open(int cx, int cy) {
    if (game_state != 0 || is_open[cy][cx] || is_flag[cy][cx]) return;
    if (!cats_placed) {
        place_cats(cx, cy);
        cats_placed = true;
        start_ms = msw_millis();
    }
    if (is_cat[cy][cx]) {
        hit_x = cx; hit_y = cy;
        finish(2);
        return;
    }
    open_from(cx, cy);
    msw_sound(0, MSW_WAVE_SQUARE, 740, 50, 35, true);
    if (opened_count >= COLS * ROWS - NCATS) finish(1);
}

static void do_flag(int cx, int cy) {
    if (game_state != 0 || is_open[cy][cx]) return;
    is_flag[cy][cx] = !is_flag[cy][cx];
    flag_count += is_flag[cy][cx] ? 1 : -1;
    msw_sound(0, MSW_WAVE_TRIANGLE, is_flag[cy][cx] ? 1180 : 880, 45, 40, true);
    msw_vibrate(15, 30);
    msw_log("catmine: flag %d at %d,%d", is_flag[cy][cx], cx, cy);
}

/* ---- タッチ ------------------------------------------------------------ */

static bool t_was_down;
static bool t_acted;
static uint32_t t_start;
static int t_cx, t_cy;

static int cell_at(int px, int py, int *cx, int *cy) {
    if (px < BX || px >= BX + COLS * CELL || py < BY || py >= BY + ROWS * CELL)
        return 0;
    *cx = (px - BX) / CELL;
    *cy = (py - BY) / CELL;
    return 1;
}

static void touch_update(void) {
    int tx, ty;
    bool down = msw_touch(&tx, &ty);
    uint32_t now = msw_millis();

    if (down && !t_was_down) {                 /* 押し始め */
        t_start = now;
        t_acted = false;
        if (!cell_at(tx, ty, &t_cx, &t_cy)) { t_cx = -1; t_cy = -1; }
        if (game_state != 0 && now > end_ms + 500) {
            new_game();                        /* 終了後はタップで再スタート */
            t_acted = true;
        }
    } else if (down && t_was_down) {           /* 押し続け: 長押しで旗 */
        if (!t_acted && game_state == 0 && t_cx >= 0 &&
            now - t_start >= LONG_PRESS_MS) {
            int cx, cy;
            if (cell_at(tx, ty, &cx, &cy) && cx == t_cx && cy == t_cy) {
                do_flag(t_cx, t_cy);
                t_acted = true;
            }
        }
    } else if (!down && t_was_down) {          /* 離した: 短ければタップ */
        if (!t_acted && t_cx >= 0) {
            if (game_state == 0) {
                if (flag_mode) do_flag(t_cx, t_cy);
                else do_open(t_cx, t_cy);
            }
        }
        t_cx = t_cy = -1;
    }
    t_was_down = down;
}

static void on_button(int button, bool down, void *user) {
    (void)user;
    if (button == MSW_BUTTON_BLUE && down) {
        flag_mode = !flag_mode;
        msw_sound(2, MSW_WAVE_SQUARE, flag_mode ? 1320 : 990, 40, 30, true);
        msw_log("catmine: flag mode %d", flag_mode);
    }
}

/* ---- 描画 -------------------------------------------------------------- */

static void draw_flag_at(int x, int y) {
    uint16_t pole = MSW_RGB(70, 50, 40);
    uint16_t red = MSW_RGB(230, 50, 60);
    msw_gfx_fill_rect(fb, x + 16, y + 8, 2, 20, pole);
    for (int i = 0; i < 10; i++) {
        int w = 13 - (i * 13) / 10;
        if (w > 0) msw_gfx_fill_rect(fb, x + 18, y + 8 + i, w, 1, red);
    }
    msw_gfx_fill_rect(fb, x + 11, y + 28, 14, 2, pole);
}

static void draw_cell(int cx, int cy) {
    int x = BX + cx * CELL, y = BY + cy * CELL;
    uint16_t border = MSW_RGB(120, 95, 70);

    bool reveal_cat = game_state != 0 && is_cat[cy][cx] && !is_flag[cy][cx];

    if (is_open[cy][cx]) {
        msw_gfx_fill_rect(fb, x, y, CELL, CELL, MSW_RGB(240, 236, 226));
        msw_gfx_rect(fb, x, y, CELL, CELL, MSW_RGB(205, 200, 190));
        int n = cell_num[cy][cx];
        if (n > 0) {
            char buf[2] = {(char)('0' + n), 0};
            int w = msw_text_width(buf, 20);
            msw_gfx_text(fb, x + (CELL - w) / 2, y + 8, buf, NUM_COLOR[n], 20);
        }
        return;
    }

    if (reveal_cat) {   /* 終了後の猫公開（踏んだ猫は赤背景） */
        bool hit = (game_state == 2 && cx == hit_x && cy == hit_y);
        msw_gfx_fill_rect(fb, x, y, CELL, CELL,
                          hit ? MSW_RGB(255, 90, 80) : MSW_RGB(255, 224, 190));
        msw_gfx_rect(fb, x, y, CELL, CELL, border);
        msw_sprite_draw(fb, cat_spr, x + 2, y + 6, 2, 0);
        return;
    }

    /* 未オープンのマス */
    msw_gfx_fill_rect(fb, x, y, CELL, CELL, MSW_RGB(255, 196, 96));
    msw_gfx_fill_rect(fb, x, y, CELL, 3, MSW_RGB(255, 226, 160));
    msw_gfx_rect(fb, x, y, CELL, CELL, border);
    if (is_flag[cy][cx]) {
        draw_flag_at(x, y);
        if (game_state == 2 && !is_cat[cy][cx]) {   /* 間違い旗にバツ */
            uint16_t red = MSW_RGB(220, 40, 40);
            msw_gfx_line(fb, x + 6, y + 6, x + CELL - 7, y + CELL - 7, red);
            msw_gfx_line(fb, x + CELL - 7, y + 6, x + 6, y + CELL - 7, red);
        }
    }
}

static void draw_text_center(int y, const char *s, uint16_t color, int size) {
    int w = msw_text_width(s, size);
    msw_gfx_text(fb, 233 - w / 2, y, s, color, size);
}

static void tick(uint32_t dt_ms, void *user) {
    (void)dt_ms; (void)user;

    if (meow_busy()) meow_feed();              /* 鳴き声の PCM 補充 */

    touch_update();

    uint16_t white = MSW_RGB(245, 245, 245);
    uint16_t gray = MSW_RGB(160, 160, 185);
    msw_gfx_fill(fb, MSW_RGB(20, 18, 40));

    const char *title = "ねこマインスイーパー";
    draw_text_center(42, title, white, 0);
    int tw = msw_text_width(title, 0);
    msw_sprite_draw(fb, cat_spr, 233 - tw / 2 - 24, 44, 1, 0);
    msw_sprite_draw(fb, cat_spr, 233 + tw / 2 + 8, 44, 1, MSW_FLIP_X);

    char buf[64];
    if (game_state == 1) {
        snprintf(buf, sizeof(buf), "クリア! %u びょう", (unsigned)elapsed_sec());
        draw_text_center(68, buf, MSW_RGB(255, 213, 79), 0);
    } else if (game_state == 2) {
        draw_text_center(68, "ねこを ふんじゃった!", MSW_RGB(255, 120, 110), 0);
    } else {
        snprintf(buf, sizeof(buf), "ねこ %d  はた %d  %u びょう",
                 NCATS - flag_count, flag_count, (unsigned)elapsed_sec());
        draw_text_center(68, buf, white, 0);
    }

    msw_gfx_fill_rect(fb, BX - 5, BY - 5, COLS * CELL + 10, ROWS * CELL + 10,
                      MSW_RGB(52, 40, 34));
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLS; x++)
            draw_cell(x, y);

    if (game_state != 0) {
        draw_text_center(396, "タップで もういちど", white, 0);
    } else if (flag_mode) {
        draw_text_center(396, "はたモード: タップではた", MSW_RGB(255, 213, 79), 0);
        draw_text_center(416, "あおボタンで もどる", gray, 0);
    } else {
        draw_text_center(396, "タップ:あける ながおし:はた", gray, 0);
        draw_text_center(416, "あおボタン:はたモード", gray, 0);
    }

    msw_fb_present();
}

int app_main(int argc, char **argv) {
    (void)argc; (void)argv;
    fb = msw_fb();
    if (!fb) return 1;
    /* デバイス側 PCM バッファの容量実測 (無音を断られるまで書く)。
     * 鳴き声の先行投入で何サンプル分まで一気に渡せるかの目安にする。 */
    {
        static const int16_t silence[1024] = {0};
        int probe = 0;
        for (int i = 0; i < 64; i++) {
            int took = msw_pcm_write(silence, 1024);
            if (took <= 0) break;
            probe += took;
            if (took < 1024) break;
        }
        msw_log("catmine: pcm buffer probe accepted %d", probe);
    }
    cat_spr = msw_sprite_from_art(CAT_ART, 12, CAT_PAL);
    if (!cat_spr) { msw_log("catmine: cat sprite failed"); return 1; }
    new_game();
    msw_set_fps(30);
    msw_on_tick(tick, NULL);
    msw_on_button(on_button, NULL);
    msw_log("catmine start");
    return 0;
}
