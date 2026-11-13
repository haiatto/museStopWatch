#include "msw_app.h"

/* かんたん鍵盤: 画面の真ん中に8つの白鍵。タッチでドレミファソラシドが鳴る。
 * 指を滑らせるとグリッサンドもできる。終わり方は黄ボタンの標準メニュー。 */

static msw_gfx_t *fb;

#define NKEYS 8
#define KEY_W 50
#define KEY_H 180
#define KB_X 33              /* 33 + 8*50 = 433, 中心 233 */
#define KB_Y 143             /* 143 + 180 = 323, 中心 233 */

static const int k_hz[NKEYS] = {262, 294, 330, 349, 392, 440, 494, 523};
static const char *k_label[NKEYS] = {"ド", "レ", "ミ", "ファ", "ソ", "ラ", "シ", "ド"};

static int cur_key = -1;          /* 今おさえている鍵 */
static int flash_key = -1;        /* はなした直後の光る鍵 */
static uint32_t flash_until = 0;

static void play_key(int k) {
    msw_sound(1, MSW_WAVE_TRIANGLE, k_hz[k], 700, 70, true);
    msw_vibrate(15, 30);
    msw_log("note %s %d Hz", k_label[k], k_hz[k]);
}

static int key_at(int x, int y) {
    if (x < KB_X || x >= KB_X + NKEYS * KEY_W) return -1;
    if (y < KB_Y || y >= KB_Y + KEY_H) return -1;
    return (x - KB_X) / KEY_W;
}

static void tick(uint32_t dt_ms, void *user) {
    (void)dt_ms; (void)user;
    int tx, ty;
    bool down = msw_touch(&tx, &ty);
    int k = down ? key_at(tx, ty) : -1;

    if (down) {
        if (k >= 0 && k != cur_key) {   /* 新しく触れた / 滑って隣の鍵へ */
            play_key(k);
            flash_key = k;
            flash_until = msw_millis() + 180;
        }
        cur_key = k;
    } else {
        cur_key = -1;
    }

    uint16_t bg = MSW_RGB(16, 16, 44);
    uint16_t white = MSW_RGB(245, 245, 245);
    uint16_t lit = MSW_RGB(255, 213, 79);
    uint16_t black = MSW_RGB(20, 20, 20);
    uint16_t gray = MSW_RGB(150, 150, 170);

    msw_gfx_fill(fb, bg);

    const char *title = "かんたんけんばん";
    int tw = msw_text_width(title, 0);
    msw_gfx_text(fb, 233 - tw / 2, 84, title, white, 0);

    for (int i = 0; i < NKEYS; i++) {
        int x = KB_X + i * KEY_W;
        bool on = (i == cur_key) || (i == flash_key && msw_millis() < flash_until);
        msw_gfx_fill_rect(fb, x, KB_Y, KEY_W, KEY_H, on ? lit : white);
        msw_gfx_rect(fb, x, KB_Y, KEY_W, KEY_H, black);
        int lw = msw_text_width(k_label[i], 0);
        msw_gfx_text(fb, x + (KEY_W - lw) / 2, KB_Y + KEY_H - 26, k_label[i], black, 0);
    }

    const char *hint = "タッチでおとがなる";
    int hw = msw_text_width(hint, 0);
    msw_gfx_text(fb, 233 - hw / 2, 356, hint, gray, 0);

    msw_fb_present();
}

int app_main(int argc, char **argv) {
    (void)argc; (void)argv;
    fb = msw_fb();
    if (!fb) return 1;
    msw_set_fps(30);
    msw_on_tick(tick, NULL);
    msw_log("piano start");
    return 0;
}
