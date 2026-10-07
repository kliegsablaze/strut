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
#include "strut.h"
#include "tables.h"

#define PI 3.14159265358979
#define LOUD 0.16           /* every table's A-weighted RMS */
#define TOP (0.45 * STRUT_SR)   /* the brightest copy's top, 19.8 kHz */
#define SPAN (2 * NT_N + 7 * NT_LEVELS)
#define COLOR_K 1.0f        /* COLOR's filter, a touch of peak at its corner */
#define MATCH_MAX 1000.0f   /* PITCH and COLOR make up at most 60 dB; the tables keep 78 dB under that */

static int16_t pool[NT_TABLES][SPAN];
static nt_table_t tables[NT_TABLES];
static float edge[NT_BANDS + 1];   /* the bands' edges, Hz */

const nt_table_t *nt_table(int t) { return &tables[t < 0 ? 0 : t >= NT_TABLES ? NT_TABLES - 1 : t]; }

/* ---- the tables ---- */

static uint32_t rs = 0x2545F491u;

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

/* A drawn colour's level at f. */
static double colour(int t, double f) {
    switch (t) {
    case NT_PINK: return 1 / sqrt(f / 1000);
    case NT_BROWN: return 1000 / sqrt(f * f + 60 * 60);     /* rumble, flat under 60 Hz */
    case NT_HISS: return 0.25 + 0.75 * (f / 5000) * (f / 5000) / (1 + (f / 5000) * (f / 5000));
    case NT_WIRES: {
        double r = 0.4;
        for (int i = 0; i < RATTLES; i++) {
            const double x = (f - rattle_f[i]) / (rattle_f[i] / 60);
            r += rattle_a[i] / (1 + x * x);
        }
        return 0.1 + hp2(f, 1500) / (1 + (f / 10000) * (f / 10000)) * r;
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

/* Table t's spectrum, bins KMIN to KMAX, into sre and sim; re and im are
 * scratch. Returns the scale that sets it to the same loudness to the ear. */
static double spectrum(int t, float *re, float *im, float *sre, float *sim) {
    rs = 0x2545F491u ^ (0x9E3779B9u * (uint32_t)(t + 1));     /* each its own noise, whatever the others draw */
    memset(sre, 0, sizeof(float) * (NT_N / 2 + 1));
    memset(sim, 0, sizeof(float) * (NT_N / 2 + 1));
    if (t == NT_CRACKLE || t == NT_GRIT) {
        double y = 0;
        int hold = 0;
        for (int n = 0; n < NT_N; n++) {
            if (t == NT_CRACKLE) {     /* 3000 clicks a second, of near one size, so its peaks leave room for its level */
                if (uni() < 3000.0 / NT_SR) {
                    const double sign = uni() < 0.5 ? -1 : 1;
                    y += sign * (0.6 + 0.4 * uni());
                }
                re[fft_rev(n, NT_BITS)] = (float)y;
                y *= 0.7;
            } else {                    /* eight levels, each held 1 to 20 of the output's samples */
                if (hold-- <= 0) y = ((int)(uni() * 8) - 3.5) / 3.5, hold = (int)(uni() * 40) + 1;
                re[fft_rev(n, NT_BITS)] = (float)y;
            }
        }
        memset(im, 0, sizeof(float) * NT_N);
        fft_reversed(re, im, NT_N, -1);    /* drawn in reversed order, above */
        for (int k = KMIN; k <= KMAX; k++) sre[k] = re[k], sim[k] = im[k];
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
        for (int k = KMIN; k <= KMAX; k++) {
            /* a random phase: a point picked evenly in the unit disc, made
             * unit length, without a sine or cosine */
            double x, y, r;
            do x = 2 * uni() - 1, y = 2 * uni() - 1, r = x * x + y * y; while (r > 1 || r < 1e-6);
            const double m = colour(t, (double)k * NT_SR / NT_N) / sqrt(r);
            sre[k] = (float)(m * x), sim[k] = (float)(m * y);
        }
    }
    double wa = 0;
    for (int k = KMIN; k <= KMAX; k++) wa += ((double)sre[k] * sre[k] + (double)sim[k] * sim[k]) * aweight((double)k * NT_SR / NT_N);
    return LOUD / sqrt(2 * wa);
}

static void store(int16_t *q, const float *x, double scale, int n) {
    const float g = (float)(scale * 32767);
    for (int i = 0; i < n; i++) q[i] = (int16_t)(int32_t)rintf(x[i] * g);
    q[-2] = q[n - 2], q[-1] = q[n - 1];
    for (int i = 0; i < 5; i++) q[n + i] = q[i];
}

/* Two tables at once, a and a + 1: one inverse FFT of A + jB gives a in
 * its real part and b in its imaginary, since both are real; half the
 * work, and the build is most of Strut's load. */
static void build_pair(int a, float *re, float *im, float *sp[4]) {
    double scale[2];
    clock_t t = clock();
    for (int j = 0; j < 2; j++) scale[j] = spectrum(a + j, re, im, sp[2 * j], sp[2 * j + 1]);
    wt_profile[WT_P_SPECTRA] += (double)(clock() - t) / CLOCKS_PER_SEC;
    const float *ar = sp[0], *ai = sp[1], *br = sp[2], *bi = sp[3];
    for (int l = 0; l < NT_LEVELS; l++) {
        const int n = NT_N >> l, top = KMAX >> l;
        memset(re, 0, sizeof(float) * n);
        memset(im, 0, sizeof(float) * n);
        const int bits = NT_BITS - l;
        for (int k = KMIN; k <= top; k++) {     /* straight into the transform's order */
            const int p = fft_rev(k, bits), q = fft_rev(n - k, bits);
            re[p] = ar[k] - bi[k], im[p] = ai[k] + br[k];
            re[q] = ar[k] + bi[k], im[q] = br[k] - ai[k];
        }
        t = clock();
        fft_reversed(re, im, n, 1);
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
    rs = 0x5BD1E995u;
    for (int i = 0; i < RATTLES; i++) {
        rattle_f[i] = 2000 * pow(4.5, uni());
        rattle_a[i] = 0.5 + uni();
    }
    /* scratch for the build only: 4 MB, given back */
    float *re = malloc(sizeof(float) * NT_N), *im = malloc(sizeof(float) * NT_N), *sp[4];
    int ok = re && im;
    for (int j = 0; j < 4; j++) ok &= (sp[j] = malloc(sizeof(float) * (NT_N / 2 + 1))) != NULL;
    if (ok)
        for (int t = 0; t < NT_TABLES; t += 2) build_pair(t, re, im, sp);
    free(re), free(im);
    for (int j = 0; j < 4; j++) free(sp[j]);
}

/* ---- playing ---- */

/* PITCH and TUNE as a speed, at most six octaves either way. */
static float rate_of(const float *p) {
    return fminf(fmaxf(exp2f((p[P_N_PITCH] + p[P_TUNE]) / 12.0f), 1.0f / 64), 64.0f);
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
    const int t = (int)p[P_N_TABLE];
    if (v->match_c == p[P_N_COLOR] && v->match_r == rate && v->match_t == t) return v->match;
    const nt_table_t *tb = nt_table(t);
    double kept = 0;
    for (int b = 0, n = bands_kept(rate); b < n; b++)
        kept += tb->band[b] * power_at(*filt, *fc, sqrtf(edge[b] * edge[b + 1]) * rate);
    v->match = kept > 0 ? fminf(sqrtf((float)(tb->var / kept)), MATCH_MAX) : 1.0f;
    v->match_c = p[P_N_COLOR], v->match_r = rate, v->match_t = t;
    return v->match;
}

void noise_start(noise_voice_t *v, uint32_t seed, float amp) {
    if (v->env < 1e-4f) {
        v->phase = seed * 2654435761u;
        v->s1 = v->s2 = 0.0f;
        v->level = -1;
    }
    v->env = sqrtf(v->env * v->env + amp * amp);
}

int noise_block(noise_voice_t *v, const float *p, int frames, noise_block_t *b) {
    const float rate = rate_of(p);
    const int level = level_of(rate);
    b->cross = v->level >= 0 && v->level != level;
    b->dx = 1.0f / (float)frames;
    const int lv[2] = { level, b->cross ? v->level : level };
    v->level = level;
    const nt_table_t *tb = nt_table((int)p[P_N_TABLE]);
    for (int l = 0; l < 2; l++) {
        b->t[l] = tb->level[lv[l]];
        b->shift[l] = NT_FRAC + lv[l];
        b->fx[l] = 1.0f / (float)(1u << b->shift[l]);
    }
    /* the loop runs at twice the output's rate: two of its samples a sample */
    b->inc = (uint32_t)(2.0f * rate * (float)(1u << NT_FRAC) + 0.5f);
    float fc;
    const float m = matched(v, p, rate, &b->filt, &fc);
    if (b->filt) b->f = svf(fc, COLOR_K);
    else v->s1 = v->s2 = 0.0f;      /* so turning COLOR off the centre starts clean */
    b->g = m * (1.0f / 32767.0f);
    v->decay = expf(-6.9078f / (noise_t60(p) * STRUT_SR));
    return v->env > 1e-5f;          /* 100 dB under a full hit */
}

void noise_skip(noise_voice_t *v, const float *p, int frames) {
    v->level = -1;
    v->decay = expf(-6.9078f / (noise_t60(p) * STRUT_SR));
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
    const float rate = rate_of(p);
    int filt;
    float fc;
    const float m = matched(v, p, rate, &filt, &fc);
    const nt_table_t *tb = nt_table((int)p[P_N_TABLE]);
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
