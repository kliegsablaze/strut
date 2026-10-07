/*
 * bench: what a block costs at the worst Strut can be asked for, every pad
 * sounding with both voices, the longest rings, Wave at its dearest. Runs natively and on the Move
 * (scripts/bench.sh). The target is a quarter of the Move's 2902 us block
 * (CLAUDE.md). "onset us" is the worst block in which all sixteen pads are hit
 * at once, which also measures starting a hit.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#include "host/plugin_api_v1.h"
#include "strut.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static double now_us(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e6 + (double)t.tv_nsec / 1e3;
}

static void hit_all(plugin_api_v2_t *a, void *p) {
    for (int i = 0; i < STRUT_PADS; i++) {
        const uint8_t on[3] = { 0x90, (uint8_t)(STRUT_NOTE0 + i), 127 };
        a->on_midi(p, on, 3, 0);
    }
}

static void set_all(plugin_api_v2_t *a, void *p, const char *k, const char *v) {
    char key[32];
    for (int i = 1; i <= STRUT_PADS; i++) {
        snprintf(key, sizeof(key), "p%02d_%s", i, k);
        a->set_param(p, key, v);
    }
}

/* Every pad hit twice so both voices ring, then blocks timed; and the worst
 * block in which all sixteen are hit at once. */
static void run(plugin_api_v2_t *a, void *p, const char *name) {
    int16_t out[256];
    double onset = 0;
    for (int r = 0; r < 20; r++) {
        double t0 = now_us();
        hit_all(a, p);
        a->render_block(p, out, 128);
        double us = now_us() - t0;
        if (us > onset) onset = us;
        for (int b = 0; b < 4; b++) a->render_block(p, out, 128);
    }
    hit_all(a, p);
    double total = 0, worst = 0;
    const int blocks = 3000;
    for (int b = 0; b < blocks; b++) {
        double t0 = now_us();
        a->render_block(p, out, 128);
        double us = now_us() - t0;
        total += us;
        if (us > worst) worst = us;
    }
    printf("%-26s %8.1f %8.1f %6.1f%% %9.1f\n", name, total / blocks, worst,
           total / blocks / 2902.0 * 100.0, onset);
}

int main(void) {
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    double t0 = now_us();
    void *p = a->create_instance(".", "");     /* the first builds Wave's tables */
    printf("load, building the tables: %.1f ms\n", (now_us() - t0) / 1000.0);
    printf("%-26s %8s %8s %7s %9s\n", "", "mean us", "worst us", "block", "onset us");

    set_all(a, p, "s_ring", "1");
    set_all(a, p, "s_snap", "1");           /* the longest hit, measured at its start */
    set_all(a, p, "s_hit", "Burst");
    run(a, p, "skin, 32 voices");

    /* Wave at its dearest: four table reads (the pulse), the ring, FM, the
     * longest fall, and striking Skin, which is sized at the hit */
    set_all(a, p, "wave", "1");
    set_all(a, p, "w_decay", "1");
    set_all(a, p, "w_wave", "0.9");
    set_all(a, p, "w_fm", "0.5");
    set_all(a, p, "w_ring", "0.5");
    set_all(a, p, "w_bend", "0.5");
    set_all(a, p, "s_hit", "Wave");
    run(a, p, "skin + wave, 32 voices");
    a->destroy_instance(p);
    return 0;
}
