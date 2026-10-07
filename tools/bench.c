/*
 * bench: what a block costs at the worst Strut can be asked for, every pad
 * sounding with both voices, the longest rings, Wave at its dearest. Runs natively and on the Move
 * (scripts/bench.sh). The target is a quarter of the Move's 2902 us block
 * (CLAUDE.md). "onset us" is the worst block in which all sixteen pads are hit
 * at once, which also measures starting a hit.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* Sorts a few numbers, for their middle: one busy moment on the Move
 * should not decide. */
static void sort(double *x, int n) {
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && x[j] < x[j - 1]; j--) { const double t = x[j]; x[j] = x[j - 1]; x[j - 1] = t; }
}

/* Every pad hit twice so both voices ring, then blocks timed in five runs;
 * and the worst block in which all sixteen are hit at once. */
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
    double mean[5], worst = 0;
    for (int r = 0; r < 5; r++) {
        hit_all(a, p);
        double total = 0;
        const int blocks = 600;
        for (int b = 0; b < blocks; b++) {
            double t0 = now_us();
            a->render_block(p, out, 128);
            double us = now_us() - t0;
            total += us;
            if (us > worst) worst = us;
        }
        mean[r] = total / blocks;
    }
    sort(mean, 5);          /* mean[2] the middle, [0] and [4] the ends */
    const double lo = mean[0], hi = mean[4];
    printf("%-26s %8.1f %8.1f %6.1f%% %9.1f   (runs %.1f%% .. %.1f%%)\n", name, mean[2], worst,
           mean[2] / 2902.0 * 100.0, onset, lo / 2902.0 * 100.0, hi / 2902.0 * 100.0);
}

/* Which processor, and how fast it is running: the Move's figures mean
 * little without them. */
static void machine(void) {
    char line[256];
    FILE *f = fopen("/proc/cpuinfo", "r");
    int cores = 0;
    const char *want[] = { "model name", "CPU implementer", "CPU part", "Hardware" };
    int shown[4] = { 0 };
    while (f && fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "processor", 9)) cores++;
        for (int i = 0; i < 4; i++)
            if (!shown[i] && !strncmp(line, want[i], strlen(want[i]))) { printf("cpu: %s", line); shown[i] = 1; }
    }
    if (f) fclose(f);
    printf("cpu: %d cores\n", cores);
    f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f && fgets(line, sizeof(line), f)) printf("cpu: running at %ld MHz\n", atol(line) / 1000);
    if (f) fclose(f);
}

int main(void) {
    machine();
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    double t0 = now_us();
    void *p = a->create_instance(".", "");     /* the first builds Wave's tables */
    printf("load, building the tables: %.1f ms\n", (now_us() - t0) / 1000.0);
    printf("%-26s %8s %8s %7s %9s\n", "", "mean us", "worst us", "block", "onset us");

    /* Skin at its dearest: METAL's partials, the longest ring and hit */
    set_all(a, p, "s_ring", "1");
    set_all(a, p, "s_snap", "1");           /* the longest hit, measured at its start */
    set_all(a, p, "s_metal", "1");
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
