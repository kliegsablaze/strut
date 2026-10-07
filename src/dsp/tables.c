/*
 * Wave's tables (tables.h). Each frame is a list of harmonics, from a recipe
 * below or measured off a waveform drawn in time, and each of its ten copies
 * is that list cut at a harmonic count and summed by an inverse FFT. Every
 * frame is scaled to the same loudness, so sweeping WAVE does not swell or
 * dip.
 *
 * Tables made from recipes share one set of phases per table, Schroeder's
 * (IEEE Trans. Information Theory 16(1), 1970): they keep a many-harmonic
 * frame from piling up into one tall spike, so it sits as loud as a smooth
 * one under the same peak. One set for every frame of a table, so morphing
 * between frames never cancels a harmonic.
 */
#include <math.h>
#include <stdatomic.h>
#include <string.h>

#include "fft.h"
#include "noise.h"
#include "tables.h"

#define PI 3.14159265358979
#define TD_N 4096           /* a time-drawn waveform's resolution */
#define RMS 0.35            /* every frame's loudness */
#define NFRAMES (3 + (WT_TABLES - 1) * WT_FRAMES)
#define FRAME_FLOATS (WT_N(0) + WT_N(1) + WT_N(2) + 7 * WT_N(3) + WT_LEVELS)

static float pool[NFRAMES][FRAME_FLOATS];
static wt_frame_t frames[NFRAMES];
float wt_sine[WT_SINE_N + 1];

const wt_frame_t *wt_frame(int t, int f) {
    return t == WT_ANALOG ? &frames[f] : &frames[3 + (t - 1) * WT_FRAMES + f];
}

static double re[TD_N], im[TD_N];

/* ---- a frame from its harmonics: x = sum ca[h] cos(2 pi h t) + sb[h] sin(2 pi h t) ---- */

static void make(wt_frame_t *fr, float *mem, const double *ca, const double *sb) {
    double e = 0;
    for (int h = 1; h <= WT_H; h++) e += ca[h] * ca[h] + sb[h] * sb[h];
    double scale = e > 0 ? RMS / sqrt(e / 2) : 0;
    for (int l = 0; l < WT_LEVELS; l++) {
        const int n = WT_N(l), top = WT_H >> l;
        memset(re, 0, sizeof(double) * n);
        memset(im, 0, sizeof(double) * n);
        for (int h = 1; h <= top && h < n / 2; h++) {
            re[h] = ca[h] / 2, im[h] = -sb[h] / 2;
            re[n - h] = ca[h] / 2, im[n - h] = sb[h] / 2;
        }
        fft(re, im, n, 1);
        if (l == 0) {       /* no frame peaks over full scale */
            double peak = 0;
            for (int i = 0; i < n; i++) peak = fmax(peak, fabs(re[i]));
            if (peak * scale > 1.0) scale = 1.0 / peak;
        }
        for (int i = 0; i < n; i++) mem[i] = (float)(re[i] * scale);
        mem[n] = mem[0];
        fr->level[l] = mem;
        mem += n + 1;
    }
    fr->ca[0] = fr->sb[0] = 0;
    for (int h = 1; h <= WT_H; h++) fr->ca[h] = (float)(ca[h] * scale), fr->sb[h] = (float)(sb[h] * scale);
}

/* A waveform drawn in time, g over one cycle, measured into harmonics. */
static void measure(double *ca, double *sb) {
    memset(im, 0, sizeof(im));
    fft(re, im, TD_N, -1);
    for (int h = 1; h <= WT_H; h++) ca[h] = 2 * re[h] / TD_N, sb[h] = -2 * im[h] / TD_N;
}

/* ---- the recipes; u runs 0..1 across a table's frames ---- */

/* Hard sync: a saw restarted every cycle, running 1 to 8 times as fast. */
static void sync_wave(double u) {
    const double r = pow(2.0, 3 * u);
    for (int i = 0; i < TD_N; i++) {
        const double x = r * i / TD_N;
        re[i] = 2 * (x - floor(x)) - 1;
    }
}

/* A sine folded back on itself more and more, as a wavefolder does. */
static void fold_wave(double u) {
    const double g = 1 + 7 * u;
    for (int i = 0; i < TD_N; i++) re[i] = sin(PI / 2 * g * sin(2 * PI * i / TD_N));
}

/* Peterson and Barney's vowels (JASA 24(2), 1952, men's averages): the
 * first three formants of u, o, a, e and i, in Hz. */
static const double VOWELS[5][3] = {
    { 300, 870, 2240 }, { 570, 840, 2410 }, { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
};
static const double VOWEL_BW[3] = { 80, 100, 140 };

/* A two-pole resonance, unity at DC, peaking at fc. */
static double reso(double f, double fc, double bw) {
    return fc * fc / sqrt((fc * fc - f * f) * (fc * fc - f * f) + bw * bw * f * f);
}

static int prime(int n) {
    if (n < 2) return 0;
    for (int d = 2; d * d <= n; d++) if (n % d == 0) return 0;
    return 1;
}

static void recipe(int t, double u, double *a) {
    memset(a, 0, sizeof(double) * (WT_H + 1));
    for (int h = 1; h <= WT_H; h++) {
        switch (t) {
        case WT_SWEEP: {        /* a saw through a resonant low-pass sweeping up */
            const double x = h / pow(2.0, 5 * u), q = 5;
            a[h] = 1.0 / h / sqrt((1 - x * x) * (1 - x * x) + x * x / (q * q));
            break;
        }
        case WT_VOWEL: {        /* a voice at 110 Hz moving through u, o, a, e, i */
            const double v = u * 4;
            const int i = v >= 4 ? 3 : (int)v;
            const double w = v - i, f = 110.0 * h;
            a[h] = 1.0 / h;
            for (int k = 0; k < 3; k++)
                a[h] *= reso(f, VOWELS[i][k] * pow(VOWELS[i + 1][k] / VOWELS[i][k], w), VOWEL_BW[k]);
            break;
        }
        case WT_HOLLOW:         /* odd harmonics only, brightening */
            if (h & 1) a[h] = pow(h, -(2 - 1.4 * u));
            break;
        case WT_METAL: {        /* a weak fundamental and a band of prime harmonics, rising */
            const double c = pow(2.0, 1.5 + 4.5 * u), x = log2(h / c);
            if (h == 1) a[h] = 0.3;
            else if (prime(h)) a[h] = exp(-x * x / (2 * 0.6 * 0.6)) / sqrt(h);
            break;
        }
        case WT_GLASS: {        /* harmonics at the squares, 1, 4, 9, 16...: a bar's spacing */
            const int n = (int)lround(sqrt(h));
            if (n * n == h) a[h] = pow(n, -(2.5 - 2 * u));
            break;
        }
        }
    }
}

static void build(void) {
    static double ca[WT_H + 1], sb[WT_H + 1], amp[WT_FRAMES][WT_H + 1], phase[WT_H + 1];

    /* Analog: sine, triangle, saw; WAVE's square and pulse are two saws */
    for (int f = 0; f < 3; f++) {
        memset(ca, 0, sizeof(ca));
        memset(sb, 0, sizeof(sb));
        for (int h = 1; h <= WT_H; h++) {
            if (f == WT_A_SINE) sb[h] = h == 1;
            else if (f == WT_A_TRI) sb[h] = (h & 1) ? ((h / 2) & 1 ? -1.0 : 1.0) / ((double)h * h) : 0;
            else sb[h] = (h & 1 ? 1.0 : -1.0) / h;     /* rising, through zero at the start */
        }
        make(&frames[f], pool[f], ca, sb);
    }

    for (int t = 1; t < WT_TABLES; t++) {
        const int first = 3 + (t - 1) * WT_FRAMES;
        if (t == WT_SYNC || t == WT_FOLD) {
            for (int f = 0; f < WT_FRAMES; f++) {
                (t == WT_SYNC ? sync_wave : fold_wave)((double)f / (WT_FRAMES - 1));
                measure(ca, sb);
                make(&frames[first + f], pool[first + f], ca, sb);
            }
            continue;
        }
        /* Schroeder's phases, from the table's mean spectrum */
        double total = 0;
        for (int f = 0; f < WT_FRAMES; f++) {
            recipe(t, (double)f / (WT_FRAMES - 1), amp[f]);
            double e = 0;
            for (int h = 1; h <= WT_H; h++) e += amp[f][h] * amp[f][h];
            for (int h = 1; h <= WT_H; h++) amp[f][h] /= sqrt(e);
        }
        for (int h = 1; h <= WT_H; h++) {
            double p = 0;
            for (int f = 0; f < WT_FRAMES; f++) p += amp[f][h] * amp[f][h];
            phase[h] = p, total += p;
        }
        double s0 = 0, s1 = 0;
        for (int h = 1; h <= WT_H; h++) {
            const double p = phase[h] / total;
            phase[h] = -2 * PI * (h * s0 - s1);
            s0 += p, s1 += h * p;
        }
        for (int f = 0; f < WT_FRAMES; f++) {
            for (int h = 1; h <= WT_H; h++) ca[h] = amp[f][h] * sin(phase[h]), sb[h] = amp[f][h] * cos(phase[h]);
            make(&frames[first + f], pool[first + f], ca, sb);
        }
    }

    for (int i = 0; i <= WT_SINE_N; i++) wt_sine[i] = (float)sin(2 * PI * i / WT_SINE_N);
    nt_build();
}

void wt_build(void) {
    static atomic_int state;    /* 0 unbuilt, 1 building, 2 built */
    int unbuilt = 0;
    if (atomic_compare_exchange_strong(&state, &unbuilt, 1)) {
        build();
        atomic_store(&state, 2);
    } else {
        while (atomic_load(&state) != 2) {}
    }
}
