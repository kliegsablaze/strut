/*
 * Noise (noise.h). Each table is a loop of noise made from a spectrum: a
 * colour drawn as the level at each frequency, given random phases and
 * summed by an inverse FFT (Serra and Smith's "keep the spectrum, lose the
 * phase"). Three are drawn in time instead, because their character is in
 * their shape, not their colour: Metal's six square waves, Crackle's
 * scattered clicks, Grit's held steps. Each is then measured into a spectrum
 * and made the same way.
 *
 * Every loop is kept at seven brightnesses an octave apart, as Wave's cycles
 * are, so noise played higher does not fold back from above the audio band;
 * at twice the output's rate, so it can be read smoothly at any speed
 * (noise.h, nt_read); and as 16-bit numbers, 8 MB for all eight (as floats
 * it would be 16). Every table is set to the same loudness by the ear's
 * weighting (A, IEC 61672), then held under full scale, so a change of TABLE
 * is a change of colour, not of level.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fft.h"
#include "noise.h"
#include "samples.h"
#include "strut.h"
#include "tables.h"

#define PI 3.14159265358979
#define LOUD 0.16           /* every table's A-weighted RMS */
#define TOP (0.45 * STRUT_SR)   /* the brightest copy's top, 19.8 kHz */
#define SPAN (2 * NT_N + 7 * NT_LEVELS)
#define COLOR_K 1.0f        /* COLOR's filter, a touch of peak at its corner */
#define MATCH_MAX 1000.0f   /* PITCH and COLOR make up at most 60 dB; the tables keep 78 dB under that */
#define SM_MATCH 4.0f       /* and 12 dB on a sample: there COLOR is a filter, not a colour */
#define SM_GAIN 2.0f        /* the library (-18 LUFS) about as loud as the tables: half its sounds within 1 dB of White or louder */

static int16_t pool[NT_TABLES][SPAN];
static float grain_w[RS_GRAIN];     /* Resynth's grains' window: sin, so two half a grain apart keep the power */
static nt_table_t tables[NT_TABLES];
static float edge[NT_BANDS + 1];   /* the bands' edges, Hz */

const nt_table_t *nt_table(int t) { return &tables[t < 0 ? 0 : t >= NT_TABLES ? NT_TABLES - 1 : t]; }

/* ---- the tables ---- */

static uint32_t rs = 0x2545F491u;

/* The build's shared parts: the A weighting at each frequency, worked out
 * once for all eight tables, and a sine for random phases. */
#define PHASES 4096
static float *aw;
static float sine[PHASES + PHASES / 4];

static double uni(void) {
    rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
    return (rs >> 8) * (1.0 / 16777216.0);
}

/* The A weighting's power, 1 at 1 kHz. */
static double aweight(double f) {
    const double f2 = f * f, a = 12194.0 * 12194.0;
    const double r = a * f2 * f2 / ((f2 + 20.6 * 20.6) * sqrt((f2 + 107.7 * 107.7) * (f2 + 737.9 * 737.9)) * (f2 + a));
    return r * r / (0.7943 * 0.7943);
}

static double hp2(double f, double fc) { const double x = (f / fc) * (f / fc); return x / sqrt(1 + x * x); }

/* Wires' rattle: narrow peaks scattered from 2 to 9 kHz. */
#define RATTLES 60
static double rattle_f[RATTLES], rattle_a[RATTLES];

/* A drawn colour's level at f, but for Wires' rattle (spectrum). In single
 * precision, and with few divisions: the Move's processor is slow at
 * dividing in double, and this runs for each of 59,000 frequencies. */
static float colour(int t, float f) {
    switch (t) {
    case NT_PINK: return 1 / sqrtf(f * (1.0f / 1000));
    case NT_BROWN: return 1000 / sqrtf(f * f + 60 * 60);    /* rumble, flat under 60 Hz */
    case NT_HISS: {
        const float x = f * (1.0f / 5000), x2 = x * x;
        return 0.25f + 0.75f * x2 / (1 + x2);
    }
    case NT_WIRES: {            /* the band the rattle sits in */
        const float x = f * (1.0f / 10000);
        return (float)hp2(f, 1500) / (1 + x * x);
    }
    default: return 1;
    }
}

/* Six square waves at unrelated pitches, as a metal hat is made in an
 * analogue drum machine, then made bright: the body kept faintly, the
 * shimmer above 5 kHz full. The pitches are our own. */
static const double SQUARES[6] = { 317, 401, 463, 587, 677, 839 };

#define KMIN ((int)ceil(20.0 * NT_N / NT_SR))     /* nothing under 20 Hz */
#define KMAX ((int)(TOP * NT_N / NT_SR))            /* or over the top */

/* Crackle or Grit, drawn in time into x. */
static void draw(int t, float *x) {
    double y = 0;
    int hold = 0;
    for (int n = 0; n < NT_N; n++) {
        if (t == NT_CRACKLE) {     /* 3000 clicks a second, of near one size, so its peaks leave room for its level */
            if (uni() < 3000.0 / NT_SR) {
                const double sign = uni() < 0.5 ? -1 : 1;
                y += sign * (0.6 + 0.4 * uni());
            }
            x[n] = (float)y;
            y *= 0.7;
        } else {                    /* eight levels, each held 1 to 20 of the output's samples */
            if (hold-- <= 0) y = ((int)(uni() * 8) - 3.5) / 3.5, hold = (int)(uni() * 40) + 1;
            x[n] = (float)y;
        }
    }
}

/* Crackle in re and Grit in im, measured by one transform, since both are
 * real: Z = A + jB, so A[k] = (Z[k] + Z*[n - k]) / 2, B[k] = (Z[k] - Z*[n - k]) / 2j. */
static void drawn(float *re, float *im, float *sp[4]) {
    fft(re, im, NT_N, -1);
    for (int k = KMIN; k <= KMAX; k++) {
        const float zr = re[k], zi = im[k], wr = re[NT_N - k], wi = -im[NT_N - k];
        sp[0][k] = 0.5f * (zr + wr), sp[1][k] = 0.5f * (zi + wi);
        sp[2][k] = 0.5f * (zi - wi), sp[3][k] = -0.5f * (zr - wr);
    }
}

/* Table t's spectrum, bins KMIN to KMAX, into sre and sim; re and im are
 * scratch (Crackle and Grit draw into them, and drawn measures). */
static void spectrum(int t, float *re, float *im, float *sre, float *sim) {
    rs = 0x2545F491u ^ (0x9E3779B9u * (uint32_t)(t + 1));     /* each its own noise, whatever the others draw */
    memset(sre, 0, sizeof(float) * (NT_N / 2 + 1));
    memset(sim, 0, sizeof(float) * (NT_N / 2 + 1));
    if (t == NT_CRACKLE || t == NT_GRIT) {
        /* drawn in time with its own noise; measured together (drawn) */
        draw(t, t == NT_CRACKLE ? re : im);
    } else if (t == NT_METAL) {
        for (int s = 0; s < 6; s++) {
            const int k0 = (int)lround(SQUARES[s] * NT_N / NT_SR);
            for (int m = 1; m * k0 <= KMAX; m += 2) sim[m * k0] -= 1.0f / (float)m;
        }
        for (int k = 0; k <= KMAX; k++) {
            const double g = k < KMIN ? 0 : 0.15 + hp2((double)k * NT_SR / NT_N, 5000);
            sre[k] *= (float)g, sim[k] *= (float)g;
        }
    } else {
        /* the level at each frequency, into sre */
        const float hz = (float)NT_SR / NT_N;
        for (int k = KMIN; k <= KMAX; k++) sre[k] = colour(t, (float)k * hz);
        if (t == NT_WIRES) {
            /* the rattle: each peak added near itself only, ten of its
             * widths each way, where it has fallen to 1 % */
            for (int k = KMIN; k <= KMAX; k++) sim[k] = 0.4f;
            for (int i = 0; i < RATTLES; i++) {
                const float w = (float)rattle_f[i] / 60, a = (float)rattle_a[i];
                const int lo = (int)((rattle_f[i] - 10 * w) / hz), hi = (int)((rattle_f[i] + 10 * w) / hz) + 1;
                for (int k = lo < KMIN ? KMIN : lo; k <= hi && k <= KMAX; k++) {
                    const float x = ((float)k * hz - (float)rattle_f[i]) / w;
                    sim[k] += a / (1 + x * x);
                }
            }
            for (int k = KMIN; k <= KMAX; k++) sre[k] = 0.1f + sre[k] * sim[k];
        }
        /* then a random phase each, from the sine */
        for (int k = KMIN; k <= KMAX; k++) {
            rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
            const int i = (int)(rs >> 20);
            const float m = sre[k];
            sre[k] = m * sine[i + PHASES / 4], sim[k] = m * sine[i];
        }
    }
}

/* The scale that sets a spectrum to the same loudness to the ear. */
static double loudness(const float *sre, const float *sim) {
    double wa = 0;
    for (int k = KMIN; k <= KMAX; k++) wa += ((double)sre[k] * sre[k] + (double)sim[k] * sim[k]) * aw[k];
    return LOUD / sqrt(2 * wa);
}

_Static_assert(NT_CRACKLE % 2 == 0 && NT_GRIT == NT_CRACKLE + 1, "Crackle and Grit are built as a pair");

static void store(int16_t *q, const float *x, double scale, int n) {
    const float g = (float)(scale * 32767);
    for (int i = 0; i < n; i++) {   /* rounded half away from zero: no library call, so four at a time */
        const float v = x[i] * g;
        q[i] = (int16_t)(int32_t)(v + (v < 0 ? -0.5f : 0.5f));
    }
    q[-2] = q[n - 2], q[-1] = q[n - 1];
    for (int i = 0; i < 5; i++) q[n + i] = q[i];
}

/* Two tables at once, a and a + 1: one inverse FFT of A + jB gives a in
 * its real part and b in its imaginary, since both are real; half the
 * work, and the build is most of Strut's load. */
static void build_pair(int a, float *re, float *im, float *sp[4]) {
    double scale[2];
    clock_t t = clock();
    for (int j = 0; j < 2; j++) spectrum(a + j, re, im, sp[2 * j], sp[2 * j + 1]);
    if (a == NT_CRACKLE) drawn(re, im, sp);
    for (int j = 0; j < 2; j++) scale[j] = loudness(sp[2 * j], sp[2 * j + 1]);
    wt_profile[WT_P_SPECTRA] += (double)(clock() - t) / CLOCKS_PER_SEC;
    const float *ar = sp[0], *ai = sp[1], *br = sp[2], *bi = sp[3];
    for (int l = 0; l < NT_LEVELS; l++) {
        const int n = NT_N >> l, top = KMAX >> l;
        memset(re, 0, sizeof(float) * n);
        memset(im, 0, sizeof(float) * n);
        for (int k = KMIN; k <= top; k++) {
            re[k] = ar[k] - bi[k], im[k] = ai[k] + br[k];
            re[n - k] = ar[k] + bi[k], im[n - k] = br[k] - ai[k];
        }
        t = clock();
        fft(re, im, n, 1);
        wt_profile[WT_P_FFT] += (double)(clock() - t) / CLOCKS_PER_SEC;
        t = clock();
        if (l == 0) {       /* under full scale */
            double pa = 0, pb = 0;
            for (int i = 0; i < n; i++) pa = fmax(pa, fabs(re[i])), pb = fmax(pb, fabs(im[i]));
            if (pa * scale[0] > 0.99) scale[0] = 0.99 / pa;
            if (pb * scale[1] > 0.99) scale[1] = 0.99 / pb;
        }
        store((int16_t *)tables[a].level[l], re, scale[0], n);
        store((int16_t *)tables[a + 1].level[l], im, scale[1], n);
        wt_profile[WT_P_STORE] += (double)(clock() - t) / CLOCKS_PER_SEC;
    }
    /* each band's share of the power, for the level and Skin's strike */
    for (int j = 0; j < 2; j++) {
        const float *sr = sp[2 * j], *si = sp[2 * j + 1];
        double var = 0, band[NT_BANDS] = { 0 };
        for (int k = KMIN, b = 0; k <= KMAX; k++) {
            const double pw = 2 * ((double)sr[k] * sr[k] + (double)si[k] * si[k]) * scale[j] * scale[j];
            while (b < NT_BANDS && (double)k * NT_SR / NT_N >= edge[b + 1]) b++;
            if (b < NT_BANDS) band[b] += pw;
            var += pw;
        }
        for (int b = 0; b < NT_BANDS; b++) tables[a + j].band[b] = (float)band[b];
        tables[a + j].var = (float)var;
    }
}

void nt_build(void) {
    for (int b = 0; b <= NT_BANDS; b++) edge[b] = 20.0f * exp2f((float)b / 4.0f);
    for (int t = 0; t < NT_TABLES; t++) {
        int16_t *q = pool[t] + 2;
        for (int l = 0; l < NT_LEVELS; l++) tables[t].level[l] = q, q += (NT_N >> l) + 7;
    }
    for (int i = 0; i < PHASES + PHASES / 4; i++) sine[i] = (float)sin(2 * PI * i / PHASES);
    for (int i = 0; i < RS_GRAIN; i++) grain_w[i] = (float)sin(PI * i / RS_GRAIN);
    rs = 0x5BD1E995u;
    for (int i = 0; i < RATTLES; i++) {
        rattle_f[i] = 2000 * pow(4.5, uni());
        rattle_a[i] = 0.5 + uni();
    }
    /* scratch for the build only: 4 MB, given back */
    float *re = malloc(sizeof(float) * NT_N), *im = malloc(sizeof(float) * NT_N), *sp[4];
    aw = malloc(sizeof(float) * (KMAX + 1));
    int ok = re && im && aw;
    for (int k = KMIN; ok && k <= KMAX; k++) aw[k] = (float)aweight((double)k * NT_SR / NT_N);
    for (int j = 0; j < 4; j++) ok &= (sp[j] = malloc(sizeof(float) * (NT_N / 2 + 1))) != NULL;
    if (ok)
        for (int t = 0; t < NT_TABLES; t += 2) build_pair(t, re, im, sp);
    free(re), free(im), free(aw);
    aw = NULL;
    for (int j = 0; j < 4; j++) free(sp[j]);
    fft_done();
}

/* ---- playing ---- */

/* PITCH and TUNE as a speed, at most six octaves either way. */
static float rate_of(const float *p) {
    return fminf(fmaxf(exp2f((p[P_N_PITCH] + p[P_TUNE]) / 12.0f), 1.0f / 64), 64.0f);
}

/* The speed the source is read at: a sample's own rate taken in, and a
 * cycle's PITCH 0 at A1, 55 Hz, as Skin's and Wave's. */
static float speed_of(const noise_voice_t *v, const float *p) {
    const float r = rate_of(p);
    if (!v->smp) return r;
    return r * v->smp->speed * (v->smp->cycle ? 55.0f / CY_HZ : 1.0f);
}

/* What the voice plays, as a table, and its key for the level match's memory. */
static const nt_table_t *source(const noise_voice_t *v, int *key) {
    if (v->smp) { *key = NT_TABLES + v->smp->entry; return &v->smp->t; }
    *key = v->tab;
    return nt_table(v->tab);
}

/* The copy to read at a speed: the brightest that folds nothing back under
 * 16 kHz. Each copy keeps up to half an octave over the top of the band,
 * whose fold lands above 16 kHz, where it is only more hiss; so the top
 * of what is heard is 14 to 20 kHz, not 10 to 20 as by octaves exactly. */
static int level_of(float rate) {
    if (rate <= 1.4142f) return 0;
    const int l = (int)ceilf(log2f(rate / 1.4142f) - 1e-4f);
    return l < NT_LEVELS ? l : NT_LEVELS - 1;
}

/* DECAY from 15 ms to 4 s, as Wave's, and the Pad page's DECAY scales it. */
float noise_t60(const float *p) {
    return fminf(0.015f * powf(4.0f / 0.015f, p[P_N_DECAY]) * powf(4.0f, p[P_DECAY]), 12.0f);
}

/* COLOR: a low-pass sweeping down to 100 Hz to the left, a high-pass up to
 * 11 kHz to the right, nothing at the centre. */
static int color_of(const float *p, float *fc) {
    const float c = p[P_N_COLOR];
    if (c < 0.0f) { *fc = 18000.0f * exp2f(7.5f * c); return 1; }
    if (c > 0.0f) { *fc = 30.0f * exp2f(8.5f * c); return 2; }
    return 0;
}

/* Where a frequency is heard, folded under the output's top. */
static float heard(float f) { return f > 0.5f * STRUT_SR ? (float)STRUT_SR - f : f; }

/* The filter's power at f (the trapezoidal filter's exact response). */
static float power_at(int filt, float fc, float f) {
    if (!filt) return 1.0f;
    const float w = tanf((float)PI * fminf(heard(f), 0.499f * STRUT_SR) / STRUT_SR) / tanf((float)PI * fc / STRUT_SR);
    const float w2 = w * w, den = (1 - w2) * (1 - w2) + COLOR_K * COLOR_K * w2;
    return filt == 1 ? 1.0f / den : w2 * w2 / den;
}

/* The bands a copy keeps: those under its top. */
static int bands_kept(float rate) {
    const float top = (float)TOP / (float)(1 << level_of(rate));
    int b = 0;
    while (b < NT_BANDS && edge[b + 1] <= top * 1.0001f) b++;
    return b;
}

/* PITCH and COLOR keep the noise's level: what the filter takes from this
 * table at this pitch, and what the copy read leaves out, is made up, up
 * to 60 dB. Turning them changes the colour and not the balance of the
 * pad, and no setting falls silent. Worked out again only when COLOR, the
 * pitch or the table moves. */
static float matched(noise_voice_t *v, const float *p, float rate, int *filt, float *fc) {
    *filt = color_of(p, fc);
    int t;
    const nt_table_t *tb = source(v, &t);
    /* near enough: a modulated COLOR moves a little every 32 samples, and
     * a level a hundredth of a turn stale is not heard, where working it
     * out anew each time cost forty tangents */
    if (fabsf(v->match_c - p[P_N_COLOR]) < 0.01f && (v->match_c == 0.0f) == (p[P_N_COLOR] == 0.0f)
        && fabsf(v->match_r - rate) < 0.003f * rate && v->match_t == t) return v->match;
    double kept = 0;
    for (int b = 0, n = bands_kept(rate); b < n; b++)
        kept += tb->band[b] * power_at(*filt, *fc, sqrtf(edge[b] * edge[b + 1]) * rate);
    v->match = kept > 0 ? fminf(sqrtf((float)(tb->var / kept)), v->smp ? SM_MATCH : MATCH_MAX) : 1.0f;
    v->match_c = p[P_N_COLOR], v->match_r = rate, v->match_t = t;
    return v->match;
}

/* START as a one-shot's read position. */
static uint32_t start_of(const smp_t *sm, const float *p) {
    return (uint32_t)(fminf(fmaxf(p[P_N_START], 0.0f), 0.999f) * (float)sm->len) << SM_FRAC;
}

/* Resynth's length: DECAY's centre the sample's own, a quarter to four
 * times it, and Pad DECAY scales it as it does every fall. */
static float stretch_of(const float *p) {
    return fminf(fmaxf(powf(4.0f, 2.0f * p[P_N_DECAY] - 1.0f) * powf(4.0f, p[P_DECAY]), 1.0f / 16), 16.0f);
}

/* LOOP's slice: from START, 2 ms long to all that is left, in level-0
 * samples; all that is left (no loop) when fully right. */
static float slice_of(const smp_t *sm, const float *p, float start) {
    const float rest = (float)sm->len - start, shortest = 0.002f * NT_SR;
    return p[P_N_LOOP] < 0.999f ? shortest * powf(fmaxf(rest / shortest, 1.0f), fmaxf(p[P_N_LOOP], 0.0f)) : rest;
}

static void rs_start(noise_voice_t *v, const float *p, const smp_t *sm, int fresh, uint32_t seed, float amp) {
    noise_bank_t *k = v->bank;
    if (fresh) {
        /* each sine from its own phase, so they do not all peak together */
        float c[RS_SLOTS], s[RS_SLOTS];
        for (int i = 0; i < RS_SLOTS; i++) {
            seed = seed * 1664525u + 1013904223u;
            const int ph = (int)(seed >> 20);
            c[i] = sine[ph + PHASES / 4], s[i] = sine[ph];
        }
        memcpy(k->c, c, sizeof(c)), memcpy(k->s, s, sizeof(s));
        memset(k->a, 0, sizeof(k->a));
        k->vel = amp;
        k->seed = seed;
        v->s1 = v->s2 = 0.0f;
    }
    k->vel_to = amp;
    k->over = 0;
    const uint32_t start = start_of(sm, p);
    k->att = -1, k->held = 0, k->cut = !fresh;
    const float hit = fmaxf((float)(start >> SM_FRAC), (float)sm->onset) + RS_ATTACK * 2.0f * sm->speed;
    k->hold_end = hit >= (float)sm->len ? sm->len << SM_FRAC : (uint32_t)hit << SM_FRAC;
    v->phase = start;       /* the attack's read position */
    k->t = (float)(start >> SM_FRAC) / RS_HOP;
    /* the leftover noise: a grain at its height from START, the next rising */
    k->pos[0] = k->pos[1] = start;
    k->age[0] = RS_GRAIN / 2, k->age[1] = 0;
    v->level = -1;
    v->old_n = 0;
    v->env = 1.0f;          /* the hit's strength is in the bank, where it glides */
}

void noise_start(noise_voice_t *v, const float *p, const struct smp *smp, uint32_t seed, float amp) {
    const int t = (int)p[P_N_TABLE];
    const int sounding = v->env >= 1e-4f;
    if (t >= NT_TABLES) {
        if (!smp) { v->env = 0.0f; return; }   /* not loaded yet: nothing, never a stale sound */
        /* MODE as made so far: Sample until the loader has made the rest */
        int mode = (int)p[P_N_MODE];
        if (smp->cycle || mode < 0 || mode > SM_NOISE || !(__atomic_load_n(&smp->has, __ATOMIC_ACQUIRE) & 1 << mode))
            mode = SM_SAMPLE;
        const int fresh = !sounding || v->smp != smp || v->mode != mode;
        v->smp = smp, v->mode = mode;
        if (mode == SM_RESYNTH) { rs_start(v, p, smp, fresh, seed, amp); return; }
        if (fresh) {
            v->s1 = v->s2 = 0.0f;
            v->level = -1;
            v->old_n = 0;
            v->phase = smp->cycle ? (uint32_t)(fminf(fmaxf(p[P_N_START], 0.0f), 1.0f) * 4294967295.0f) : start_of(smp, p);
        } else if (!smp->cycle) {
            /* a sampler restarts; the note cut fades over 256 samples */
            v->old = v->phase, v->old_g = v->env, v->old_n = 256;
            v->phase = start_of(smp, p);
        }   /* a cycle runs on, as an oscillator does */
        v->env = amp;
        return;
    }
    if (!sounding || v->smp) {
        v->phase = seed * 2654435761u;
        v->s1 = v->s2 = 0.0f;
        v->level = -1;
        v->env = 0.0f;
    }
    v->smp = NULL, v->tab = t, v->old_n = 0, v->mode = SM_SAMPLE;
    v->env = sqrtf(v->env * v->env + amp * amp);
}

/* Sine and cosine of w in 0..pi, four at once: their series about pi/2,
 * to within 1e-7, where the library's took a call each. */
static inline void sincos4(nt_f4 w, nt_f4 *sn, nt_f4 *cs) {
    const nt_f4 x = w - 1.57079633f, x2 = x * x;
    *sn = 1.0f + x2 * (-1.0f / 2 + x2 * (1.0f / 24 + x2 * (-1.0f / 720 + x2 * (1.0f / 40320 + x2 * (-1.0f / 3628800 + x2 * (1.0f / 479001600))))));
    *cs = -x * (1.0f + x2 * (-1.0f / 6 + x2 * (1.0f / 120 + x2 * (-1.0f / 5040 + x2 * (1.0f / 362880 + x2 * (-1.0f / 39916800))))));
}

/* Resynth's block, into b->buf (McAulay and Quatieri; Serra and Smith):
 * the sample's sine waves, each a pointer turned a step a sample, at the
 * pitch PITCH gives and the level the frame reached gives; and the noise
 * left over, read in overlapping grains from where the frames have got
 * to, so its time follows DECAY and not PITCH. The hit, to 10 ms past
 * where it begins, is the sample itself, crossing over to that by power:
 * frames 46 ms long blur a drum's attack, and sines of a guessed phase
 * summed there peaked up to 6 dB over the recording. */
static void rs_block(noise_voice_t *v, const float *p, int frames, int hold, float rate, noise_block_t *b) {
    if (frames <= 0) return;        /* a FLAM hit landing on a block's edge */
    noise_bank_t *k = v->bank;
    const smp_t *sm = v->smp;
    const float inv = 1.0f / (float)frames;
    /* where the frames get to by the block's end: LOOP holds them in a
     * slice, and with no loop the sound ends at the last */
    /* through the attack, the frames keep up with the sample itself */
    const float t0 = k->t, dt = hold ? 0.0f
                                     : k->att < RS_GRAIN / 2 ? 2.0f * rate / RS_HOP
                                                             : 2.0f * sm->speed / ((float)RS_HOP * stretch_of(p));
    const float start = (float)(start_of(sm, p) >> SM_FRAC), slice = slice_of(sm, p, start);
    const int loops = slice < (float)sm->len - start;
    const float ts = start / RS_HOP, tl = slice / RS_HOP, last = (float)(sm->frames - 1);
    float t1 = t0 + dt * (float)frames;
    if (loops && t1 >= ts + tl) t1 = ts + fmodf(t1 - ts, tl);
    const int ends = !loops && t1 >= last;
    if (ends) t1 = last;
    k->t = t1;
    int f = (int)t1;
    if (f > sm->frames - 1) f = sm->frames - 1;
    const int f1 = f + 1 < sm->frames ? f + 1 : f;
    const float u = t1 - (float)f;
    const float *q0 = sm->fq + (size_t)f * RS_SLOTS, *q1 = sm->fq + (size_t)f1 * RS_SLOTS;
    const float *a0 = sm->am + (size_t)f * RS_SLOTS, *a1 = sm->am + (size_t)f1 * RS_SLOTS;
    const float vel = ends ? 0.0f : k->vel_to;
    /* a level-0 cycle a sample is 2 rate cycles of the output's */
    const float wk = 4.0f * (float)PI * rate, wtop = 2.0f * (float)PI * 0.45f;
    nt_f4 acc[NT_BLOCK];
    memset(acc, 0, sizeof(nt_f4) * (size_t)frames);
    /* each sine's step and where its level is going, four to a vector */
    nt_f4 cr[RS_SLOTS / 4], ci[RS_SLOTS / 4], ta[RS_SLOTS / 4];
    int live[RS_SLOTS / 4];
    for (int g = 0; g < RS_SLOTS / 4; g++) {
        nt_f4 w;
        for (int j = 0; j < 4; j++) {
            const int i = 4 * g + j;
            w[j] = wk * (q0[i] + u * (q1[i] - q0[i]));
            ta[g][j] = w[j] < wtop ? (a0[i] + u * (a1[i] - a0[i])) * vel : 0.0f;   /* over 19.8 kHz: none */
            w[j] = fminf(w[j], wtop);
        }
        const nt_f4 a = k->a[g];
        live[g] = a[0] || a[1] || a[2] || a[3] || ta[g][0] || ta[g][1] || ta[g][2] || ta[g][3];
        sincos4(w, &ci[g], &cr[g]);
        const nt_f4 c = k->c[g], sn = k->s[g], m = 1.5f - 0.5f * (c * c + sn * sn);     /* kept on the circle */
        k->c[g] = c * m, k->s[g] = sn * m;
    }
    /* four vectors at a time, side by side: one pointer's turn waits on
     * its last, so turning four at once keeps the processor busy; the
     * slots fill from the first, so a quiet sound's last four rest */
    for (int h = 0; h < RS_SLOTS / 4; h += 4) {
        if (!(live[h] | live[h + 1] | live[h + 2] | live[h + 3])) continue;
        nt_f4 c0 = k->c[h], c1 = k->c[h + 1], c2 = k->c[h + 2], c3 = k->c[h + 3];
        nt_f4 s0 = k->s[h], s1 = k->s[h + 1], s2 = k->s[h + 2], s3 = k->s[h + 3];
        nt_f4 g0 = k->a[h], g1 = k->a[h + 1], g2 = k->a[h + 2], g3 = k->a[h + 3];
        const nt_f4 d0 = (ta[h] - g0) * inv, d1 = (ta[h + 1] - g1) * inv, d2 = (ta[h + 2] - g2) * inv, d3 = (ta[h + 3] - g3) * inv;
        for (int n = 0; n < frames; n++) {
            nt_f4 t;
            t = c0 * cr[h] - s0 * ci[h], s0 = s0 * cr[h] + c0 * ci[h], c0 = t;
            t = c1 * cr[h + 1] - s1 * ci[h + 1], s1 = s1 * cr[h + 1] + c1 * ci[h + 1], c1 = t;
            t = c2 * cr[h + 2] - s2 * ci[h + 2], s2 = s2 * cr[h + 2] + c2 * ci[h + 2], c2 = t;
            t = c3 * cr[h + 3] - s3 * ci[h + 3], s3 = s3 * cr[h + 3] + c3 * ci[h + 3], c3 = t;
            g0 += d0, g1 += d1, g2 += d2, g3 += d3;
            acc[n] += (g0 * s0 + g1 * s1) + (g2 * s2 + g3 * s3);
        }
        k->c[h] = c0, k->c[h + 1] = c1, k->c[h + 2] = c2, k->c[h + 3] = c3;
        k->s[h] = s0, k->s[h + 1] = s1, k->s[h + 2] = s2, k->s[h + 3] = s3;
        for (int j = 0; j < 4; j++) k->a[h + j] = ta[h + j];
    }
    /* the leftover noise, two grains half a grain apart */
    const nt_table_t *rt = &sm->rest;
    const int l = level_of(rate);
    const int16_t *tab = rt->level[l];
    const int shift = SM_FRAC + l;
    const float fx = 1.0f / (float)(1u << shift);
    const uint32_t inc = (uint32_t)(2.0f * rate * (float)(1u << SM_FRAC) + 0.5f), end = sm->len << SM_FRAC;
    /* a grain starts so its middle is where the frames will be then */
    const float lead = (float)(RS_GRAIN / 2) * (dt * RS_HOP - 2.0f * rate);
    float gv = k->vel;
    const float dg = (vel - gv) * inv;
    const int16_t *own = sm->t.level[l];
    for (int n = 0; n < frames; n++) {
        float r = 0.0f;
        for (int j = 0; j < 2; j++) {
            if (k->pos[j] < end) {
                /* noise needs no finer reading than a line: stored at
                 * twice the rate, the line's error is 50 dB under it at
                 * 5 kHz and 23 dB at 20, and is only more noise */
                const uint32_t i = k->pos[j] >> shift;
                const float x = (float)(k->pos[j] & ((1u << shift) - 1u)) * fx;
                r += grain_w[k->age[j]] * ((float)tab[i] + (float)(tab[i + 1] - tab[i]) * x);
            }
            k->pos[j] += inc;
            if (++k->age[j] == RS_GRAIN) {
                float tn = t0 + dt * (float)n;
                if (loops && tn >= ts + tl) tn = ts + fmodf(tn - ts, tl);
                k->seed = k->seed * 1664525u + 1013904223u;
                const float at = tn * RS_HOP + lead + (float)(k->seed >> 24);     /* and a little apart, so the two never match */
                k->pos[j] = at <= 0.0f ? 0u : at >= (float)sm->len ? end : (uint32_t)at << SM_FRAC;
                k->age[j] = 0;
            }
        }
        gv += dg;
        float y = (acc[n][0] + acc[n][1]) + (acc[n][2] + acc[n][3]) + r * gv;
        if (k->att < RS_GRAIN / 2) {
            if (k->att < 0 && v->phase >= k->hold_end) k->att = 0;
            const int i = k->att;
            const float o = v->phase < end ? nt_read(own, v->phase, shift, fx) * gv : 0.0f;
            v->phase += inc;
            /* a note the hit cut fades out over its first 64 samples */
            const float wr = i >= 0 ? grain_w[i] : k->cut && k->held < 64 ? (float)(64 - k->held) * (1.0f / 64) : 0.0f;
            y = y * wr + o * (i >= 0 ? grain_w[i + RS_GRAIN / 2] : 1.0f);
            if (i >= 0) k->att++;
            else k->held++;
        }
        b->buf[n] = y;
    }
    k->vel = vel;
    if (ends) k->over = 1;
}

int noise_block(noise_voice_t *v, const float *p, int frames, int hold, noise_block_t *b) {
    /* a noise table follows TABLE as it turns; a sample plays out the note */
    const int t = (int)p[P_N_TABLE];
    if (!v->smp && t < NT_TABLES) v->tab = t;
    const smp_t *sm = v->smp;
    const float rate = speed_of(v, p);
    const int level = level_of(rate);
    b->cross = v->level >= 0 && v->level != level;
    b->dx = 1.0f / (float)frames;
    const int lv[2] = { level, b->cross ? v->level : level };
    v->level = level;
    int key;
    const nt_table_t *tb = source(v, &key), *rd = sm && v->mode == SM_NOISE ? &sm->noise : tb;
    const int frac = !sm ? NT_FRAC : sm->cycle ? CY_FRAC : SM_FRAC;
    for (int l = 0; l < 2; l++) {
        b->t[l] = rd->level[lv[l]];
        b->shift[l] = frac + lv[l];
        b->fx[l] = 1.0f / (float)(1u << b->shift[l]);
    }
    /* the loop runs at twice the output's rate: two of its samples a sample */
    b->inc = (uint32_t)(2.0f * rate * (float)(1u << frac) + 0.5f);
    b->rs = sm && v->mode == SM_RESYNTH;
    b->shot = sm && !sm->cycle && !b->rs;
    if (b->rs) {
        if (v->bank->over) v->env = 0.0f;
        else rs_block(v, p, frames, hold, rate, b);
    }
    if (b->shot) {
        /* LOOP fully right plays to the end; lower, it repeats a slice from
         * START, 2 ms long to all that is left, crossing back over its last
         * 4 ms (or half of it, or as much as lies before it) */
        const uint32_t start = start_of(sm, p), len = sm->len << SM_FRAC;
        const float rest = (float)((len - start) >> SM_FRAC), shortest = 0.002f * NT_SR;
        const float ls = p[P_N_LOOP] < 0.999f ? shortest * powf(fmaxf(rest / shortest, 1.0f), fmaxf(p[P_N_LOOP], 0.0f)) : rest;
        if (ls < rest) {
            b->loop = (uint32_t)ls << SM_FRAC;
            b->end = start + b->loop;
            uint32_t xf = (uint32_t)(0.004f * NT_SR) << SM_FRAC;
            if (xf > b->loop / 2) xf = b->loop / 2;
            if (xf > start) xf = start;
            b->xf_at = xf ? b->end - xf : UINT32_MAX;
            b->xk = xf ? 1.0f / (float)xf : 0.0f;
        } else {
            b->loop = 0, b->end = len, b->xf_at = UINT32_MAX, b->xk = 0.0f;
        }
    }
    float fc;
    const float m = matched(v, p, rate, &b->filt, &fc);
    if (b->filt) b->f = svf(fc, COLOR_K);
    else v->s1 = v->s2 = 0.0f;      /* so turning COLOR off the centre starts clean */
    b->g = m * (1.0f / 32767.0f) * (sm && !sm->cycle ? SM_GAIN : 1.0f);
    /* a one-shot with DECAY fully right plays to its end, unfaded */
    const int whole = (b->shot && p[P_N_DECAY] >= 0.999f) || b->rs;     /* Resynth: DECAY is its length */
    v->decay = hold || whole ? 1.0f : expf(-6.9078f / (noise_t60(p) * STRUT_SR));
    (void)key;
    return v->env > 1e-5f;          /* 100 dB under a full hit */
}

void noise_skip(noise_voice_t *v, const float *p, int frames, int hold) {
    v->level = -1;
    if (v->smp && v->mode == SM_RESYNTH) return;    /* unheard, it waits where it is */
    v->decay = hold ? 1.0f : expf(-6.9078f / (noise_t60(p) * STRUT_SR));
    v->env *= powf(v->decay, (float)frames);
}

/* The strike's power gathered from x = 0 to x, at x radians a sample from
 * the resonance: the integral of 1 / |1 - d e^{-jx}|^2, the window
 * d^n's spectrum, in closed form. */
static float gathered(float x, float d) {
    const float turn = 2 * (float)PI / (1 - d * d);
    float k = 0;
    while (x > (float)PI) x -= 2 * (float)PI, k += turn;
    while (x < -(float)PI) x += 2 * (float)PI, k -= turn;
    return k + 2 / (1 - d * d) * atanf((1 + d) / (1 - d) * tanf(x / 2));
}

/* Noise's expected drive of a resonance at hz through Skin's strike (the
 * window d^n): each band's power, spread across its width, gathered through
 * the window's spectrum around hz. A short strike hears a wide stretch of
 * the colour; a long one only what is near hz, so Metal's lines drive a
 * nearby pitch through a short hit and not through a long one, as they
 * really would. Never under the noise's own level, so a colour with nothing
 * near hz rings Skin quietly rather than blowing its hit up. Each band edge
 * is gathered once, for the band below it and the one above. */
float noise_strike(noise_voice_t *v, const float *p, float hz, float d, int len) {
    (void)len;      /* the window, d^n, is down 43 dB by its end: taken as endless */
    const float rate = speed_of(v, p);
    int filt, key;
    float fc;
    const float m = matched(v, p, rate, &filt, &fc);
    const nt_table_t *tb = source(v, &key);
    const float w = 2 * (float)PI * hz / STRUT_SR;
    float drive = 0, power = 0, lo = 0, glo = 0;
    for (int b = 0, n = bands_kept(rate); b <= n; b++) {
        const float e = fminf(2 * (float)PI * heard(edge[b] * rate) / STRUT_SR, (float)PI);
        /* the band and its mirror below zero, from 0 up to this edge */
        const float g = gathered(w + e, d) - gathered(w - e, d);
        if (b > 0) {
            const float pw = tb->band[b - 1] * power_at(filt, fc, sqrtf(edge[b - 1] * edge[b]) * rate);
            power += pw;
            if (e - lo > 1e-6f) drive += pw / (2 * (e - lo)) * (g - glo);
        }
        lo = e, glo = g;
    }
    return m * sqrtf(fmaxf(drive, power));
}
