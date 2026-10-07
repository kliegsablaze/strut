/*
 * bench: what a block costs at the worst Strut can be asked for, every pad
 * sounding with both voices, the longest rings. Runs natively and on the Move
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

int main(void) {
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    void *p = a->create_instance(".", "");
    int16_t out[256];
    char key[32];
    for (int i = 1; i <= STRUT_PADS; i++) {
        snprintf(key, sizeof(key), "p%02d_s_ring", i);
        a->set_param(p, key, "1");
        snprintf(key, sizeof(key), "p%02d_s_snap", i);
        a->set_param(p, key, "1");          /* the longest hit, measured at its start */
        snprintf(key, sizeof(key), "p%02d_s_hit", i);
        a->set_param(p, key, "Burst");
    }
    double onset = 0;
    for (int r = 0; r < 20; r++) {
        double t0 = now_us();
        hit_all(a, p);
        a->render_block(p, out, 128);
        double us = now_us() - t0;
        if (us > onset) onset = us;
        for (int b = 0; b < 4; b++) a->render_block(p, out, 128);
    }
    hit_all(a, p);   /* both voices of every pad now ring */
    double total = 0, worst = 0;
    const int blocks = 3000;
    for (int b = 0; b < blocks; b++) {
        double t0 = now_us();
        a->render_block(p, out, 128);
        double us = now_us() - t0;
        total += us;
        if (us > worst) worst = us;
    }
    printf("%-22s %8s %8s %7s %9s\n", "", "mean us", "worst us", "block", "onset us");
    printf("%-22s %8.1f %8.1f %6.1f%% %9.1f\n", "skin, 32 voices", total / blocks, worst,
           total / blocks / 2902.0 * 100.0, onset);
    a->destroy_instance(p);
    return 0;
}
