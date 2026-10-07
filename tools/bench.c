/*
 * bench: what a block costs at the worst Strut can be asked for, every pad
 * sounding, the longest rings, each engine at its dearest. Runs natively and on the Move
 * (scripts/bench.sh). The target is a quarter of the Move's 2902 us block
 * (CLAUDE.md). "onset us" is the worst block in which all sixteen pads are hit
 * at once, which also measures starting a hit.
 */
#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include "noise.h"
#include "tables.h"

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

/* A stand-in library the bench writes for itself, as the Move's copy of
 * the bench carries none: one sample as long as the library's longest, 4 s
 * of decaying noise. */
static const char *fake_library(void) {
    static const char *dir = "/tmp/strut-bench-lib";
    mkdir(dir, 0755);
    mkdir("/tmp/strut-bench-lib/samples", 0755);
    mkdir("/tmp/strut-bench-lib/samples/Kick", 0755);
    FILE *f = fopen("/tmp/strut-bench-lib/samples/Kick/Kick 001.wav", "wb");
    if (!f) return dir;
    const int n = 4 * STRUT_SR;
    const unsigned bytes = (unsigned)n * 2;
    unsigned char h[44] = "RIFF....WAVEfmt ";
    const unsigned v[] = { 36 + bytes, 16, 1 | 1 << 16, STRUT_SR, STRUT_SR * 2, 2 | 16 << 16 };
    memcpy(h + 4, &v[0], 4), memcpy(h + 16, &v[1], 4), memcpy(h + 20, &v[2], 4);
    memcpy(h + 24, &v[3], 4), memcpy(h + 28, &v[4], 4), memcpy(h + 32, &v[5], 4);
    memcpy(h + 36, "data", 4), memcpy(h + 40, &bytes, 4);
    fwrite(h, 1, 44, f);
    unsigned r = 1;
    for (int i = 0; i < n; i++) {
        r = r * 1664525u + 1013904223u;
        const short x = (short)((int)(r >> 16) - 32768) * 0.9f * expf(-3.0f * i / n);
        fwrite(&x, 2, 1, f);
    }
    fclose(f);
    return dir;
}

/* Sorts a few numbers, for their middle: one busy moment on the Move
 * should not decide. */
static void sort(double *x, int n) {
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && x[j] < x[j - 1]; j--) { const double t = x[j]; x[j] = x[j - 1]; x[j - 1] = t; }
}

/* Every pad hit, then blocks timed in five runs;
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
    smp_catalogue(fake_library());
    struct rusage r0, r1;
    getrusage(RUSAGE_SELF, &r0);
    const clock_t c0 = clock();
    double t0 = now_us();
    void *p = a->create_instance(".", "");     /* the first builds the tables */
    printf("load, building the tables: %.1f ms\n", (now_us() - t0) / 1000.0);
    getrusage(RUSAGE_SELF, &r1);
    /* where it went: if the processor time falls well short of the whole,
     * the rest was waiting (memory, the system); page faults count memory
     * first touched or taken back */
    printf("  processor %.1f ms (user %.1f, system %.1f), page faults %ld minor %ld major\n",
           (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC,
           (r1.ru_utime.tv_sec - r0.ru_utime.tv_sec) * 1e3 + (r1.ru_utime.tv_usec - r0.ru_utime.tv_usec) / 1e3,
           (r1.ru_stime.tv_sec - r0.ru_stime.tv_sec) * 1e3 + (r1.ru_stime.tv_usec - r0.ru_stime.tv_usec) / 1e3,
           r1.ru_minflt - r0.ru_minflt, r1.ru_majflt - r0.ru_majflt);
    printf("  wave %.1f ms, noise spectra %.1f, transforms %.1f, storing %.1f\n", wt_profile[WT_P_WAVE] * 1e3,
           wt_profile[WT_P_SPECTRA] * 1e3, wt_profile[WT_P_FFT] * 1e3, wt_profile[WT_P_STORE] * 1e3);
    /* Noise's tables again, now the memory is warm */
    for (int k = 0; k < WT_P_COUNT; k++) wt_profile[k] = 0;
    t0 = now_us();
    nt_build();
    printf("  noise again: %.1f ms (spectra %.1f, transforms %.1f, storing %.1f)\n", (now_us() - t0) / 1000.0,
           wt_profile[WT_P_SPECTRA] * 1e3, wt_profile[WT_P_FFT] * 1e3, wt_profile[WT_P_STORE] * 1e3);
    printf("%-26s %8s %8s %7s %9s\n", "", "mean us", "worst us", "block", "onset us");

    /* the kit's effects last, so the rows before stay comparable */
    a->set_param(p, "space", "0");

    /* Skin at its dearest: METAL's partials, the longest ring and hit */
    set_all(a, p, "s_ring", "1");
    set_all(a, p, "s_snap", "1");           /* the longest hit, measured at its start */
    set_all(a, p, "s_metal", "1");
    set_all(a, p, "s_hit", "Burst");
    run(a, p, "skin, 16 pads");

    /* Wave at its dearest: four table reads (the pulse), the ring, FM, the
     * longest fall, and striking Skin, which is sized at the hit */
    set_all(a, p, "wave", "1");
    set_all(a, p, "w_decay", "1");
    set_all(a, p, "w_wave", "0.9");
    set_all(a, p, "w_fm", "0.5");
    set_all(a, p, "w_ring", "0.5");
    set_all(a, p, "w_bend", "0.5");
    set_all(a, p, "s_hit", "Wave");
    run(a, p, "skin + wave, 16 pads");

    /* Noise at its dearest: COLOR's filter, pitched off its copies' rate,
     * the longest fall, and striking Skin, which is sized at the hit */
    set_all(a, p, "noise", "1");
    set_all(a, p, "n_decay", "1");
    set_all(a, p, "n_color", "0.3");
    set_all(a, p, "n_pitch", "5");
    set_all(a, p, "s_hit", "Burst");
    run(a, p, "all three, Skin's burst");
    set_all(a, p, "s_hit", "Noise");
    run(a, p, "all three, 16 pads");

    /* and every pad's finish on: COLOR, DRIVE, CRUSH, both shelves, PAN */
    set_all(a, p, "color", "0.3");
    set_all(a, p, "drive", "0.5");
    set_all(a, p, "crush", "0.2");
    set_all(a, p, "low", "6");
    set_all(a, p, "high", "6");
    set_all(a, p, "pan", "0.3");
    run(a, p, "and every effect");

    /* and every engine's modulator moving something each 32 samples: an
     * envelope on Skin's pitch, an LFO on Wave's position, an envelope on
     * Noise's COLOR (its level match worked out anew), every CURVE Soft */
    set_all(a, p, "s_kind", "Envelope");
    set_all(a, p, "s_rate", "-0.6");
    set_all(a, p, "s_depth1", "0.3");
    set_all(a, p, "w_kind", "LFO");
    set_all(a, p, "w_aim1", "Wave");
    set_all(a, p, "w_depth1", "0.5");
    set_all(a, p, "n_kind", "Envelope");
    set_all(a, p, "n_rate", "-0.6");
    set_all(a, p, "n_aim1", "Color");
    set_all(a, p, "n_depth1", "0.5");
    set_all(a, p, "s_curve", "Soft");
    set_all(a, p, "w_curve", "Soft");
    set_all(a, p, "n_curve", "Soft");
    run(a, p, "and every modulator");

    /* and the Kit page: the room at its longest, GLUE and WARM full up */
    a->set_param(p, "space", "0.5");
    a->set_param(p, "size", "1");
    a->set_param(p, "glue", "1");
    a->set_param(p, "warm", "1");
    run(a, p, "and the kit's effects");

    /* and Noise playing a 4 s sample on every pad instead of a table,
     * looped (its crossing back each time round), once it has loaded */
    const double l0 = now_us();
    smp_t *one = smp_load(0);
    printf("loading a 4 s sample: %.1f ms\n", (now_us() - l0) / 1000.0);
    smp_free(one);
    set_all(a, p, "n_table", "Kick 001");
    set_all(a, p, "n_loop", "0.5");
    strut_t *st = p;
    int16_t out[256];
    for (int i = 0, ready = 0; i < 400 && !ready; i++) {
        a->render_block(p, out, 128);
        const struct timespec ms = { 0, 5000000 };
        nanosleep(&ms, NULL);
        ready = 1;
        for (int k = 0; k < STRUT_PADS; k++) ready &= __atomic_load_n(&st->lib.ready[k], __ATOMIC_ACQUIRE) != NULL;
    }
    run(a, p, "and samples, looped");
    a->destroy_instance(p);
    return 0;
}
