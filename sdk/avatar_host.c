/*
 * museStopWatch SDK: runs an avatar on the build machine (sdk/build.py,
 * "kind": "avatar"), as tools/muse/avatar.py does for the official one.
 *
 * Built with the avatar's .c files by the machine's own C compiler, with the
 * address and undefined-behaviour sanitizers when it has them. Calls
 * app_main, then every state the watch shows, with the same poses as
 * app.avatar_preview on the watch (half a second of IDLE first, 25 frames a
 * second, a made-up voice level). The grid and palette are exactly as big
 * as on the watch, so a write past them stops the run with the line that
 * did it. Writes each state's frames to the file given (read by build.py,
 * which makes the PNG sheets).
 *
 *   avatar_host <out.bin> <frames per state>
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "msw_app.h"

int app_main(int argc, char **argv);

static const msw_avatar_def_t *s_def;
static int s_defined;

void msw_avatar_define(const msw_avatar_def_t *def)
{
    s_def = def;
    s_defined++;
}

typedef struct {
    const char *name;
    int mode, working, happy;
    float seconds;
} state_t;

/* Keep in step with STATES in msw_avatar.c. */
static const state_t STATES[] = {
    { "boot", MSW_MODE_BOOT, 0, 0, 1.6f },
    { "idle", MSW_MODE_IDLE, 0, 0, 2.4f },
    { "listening", MSW_MODE_LISTENING, 0, 0, 2.4f },
    { "thinking", MSW_MODE_THINKING, 0, 0, 2.4f },
    { "working", MSW_MODE_THINKING, 1, 0, 2.4f },
    { "speaking", MSW_MODE_SPEAKING, 0, 0, 2.4f },
    { "happy", MSW_MODE_IDLE, 0, 1, 1.9f },
    { "error", MSW_MODE_ERROR, 0, 0, 1.6f },
    { "off", MSW_MODE_OFF, 0, 0, 1.4f },
};

static float synth_level(int mode, float t, float mode_t)
{
    if (mode == MSW_MODE_SPEAKING) {
        if (fmodf(mode_t, 1.6f) > 1.25f) {
            return 0;
        }
        float v = 0.55f + 0.35f * sinf(t * 11.0f) + 0.1f * sinf(t * 23.0f);
        return v < 0 ? 0 : v > 1 ? 1 : v;
    }
    if (mode == MSW_MODE_LISTENING) {
        float v = 0.3f + 0.25f * sinf(t * 7.0f);
        return v < 0 ? 0 : v;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: avatar_host <out.bin> <frames per state>\n");
        return 2;
    }
    int frames = atoi(argv[2]);
    FILE *out = fopen(argv[1], "wb");
    if (!out || frames < 1 || frames > 9) {
        fprintf(stderr, "avatar_host: bad arguments\n");
        return 2;
    }
    char *av[] = { "avatar", NULL };
    int rc = app_main(1, av);
    if (rc != 0) {
        printf("FAIL app_main returned %d (it must return 0)\n", rc);
        return 1;
    }
    if (s_defined != 1 || !s_def || !s_def->render) {
        printf("FAIL app_main must call msw_avatar_define once, with a render function (called %d times)\n",
               s_defined);
        return 1;
    }
    /* Exactly the watch's sizes: the sanitizer stops a write past them. */
    uint8_t *grid = calloc(MSW_AVATAR_W * MSW_AVATAR_H, 1);
    uint16_t *pal = calloc(MSW_AVATAR_COLORS, sizeof(uint16_t));
    int bad = 0;
    float t = 100.0f;
    const float dt = 0.04f;
    for (size_t s = 0; s < sizeof(STATES) / sizeof(STATES[0]); s++) {
        const state_t *st = &STATES[s];
        int got = 0, lit_frames = 0;
        for (int f = -12; got < frames && f < 400; f++) {
            float mode_t = f < 0 ? 5.0f + (f + 12) * dt : f * dt;
            msw_avatar_pose_t pose = { .mode = f < 0 ? MSW_MODE_IDLE : st->mode, .t = t, .mode_t = mode_t };
            if (f >= 0) {
                pose.level = synth_level(st->mode, t, mode_t);
                pose.working = st->working ? 1.0f : 0.0f;
                if (st->happy) {
                    pose.happy = mode_t < 0.3f ? mode_t / 0.3f : fmaxf(0, 1.0f - (mode_t - 0.3f) / 1.6f);
                }
            }
            s_def->render(&pose, grid, pal);
            if (s_def->accent) {
                (void)s_def->accent(pose.mode);
            }
            if (f >= 0 && mode_t + dt / 2 >= got * st->seconds / frames) {
                int lit = 0;
                for (int i = 0; i < MSW_AVATAR_W * MSW_AVATAR_H; i++) {
                    lit += pal[grid[i]] != 0;
                }
                lit_frames += lit > 0;
                fwrite(grid, 1, MSW_AVATAR_W * MSW_AVATAR_H, out);
                fwrite(pal, sizeof(uint16_t), MSW_AVATAR_COLORS, out);
                got++;
            }
            t += dt;
        }
        if (!lit_frames) {
            printf("FAIL %s: every frame is black (nothing drawn, or the palette is not set)\n", st->name);
            bad++;
        }
    }
    fclose(out);
    free(grid);
    free(pal);
    printf(bad ? "FAIL\n" : "OK\n");
    return bad ? 1 : 0;
}
