/*
 * The FFT (fft.h). Its turns come from one table, worked out on first use, so
 * a long transform costs no sine or cosine of its own: the 256k-point noise
 * loops would otherwise spend most of their build on them. Kept as floats,
 * 2 MB, as the transform itself is (fft.h).
 *
 * Built for the Move, whose memory is slow to reach anywhere but in order,
 * and whose small cache it shares with the Move's own sound. Loading 0.2.0
 * took 2.8 s there and 0.26 s on a laptop. So:
 *  - each stage's turns lie side by side (one table read with a stride
 *    cost a whole cache line for every turn);
 *  - each row of butterflies is a function of its own, which GCC runs four
 *    at a time;
 *  - a long transform is done in four steps (Bailey, "FFTs in external or
 *    hierarchical memory", 1990): as a grid, a short transform down each
 *    column, a turn for each point, a short transform along each row, a
 *    strip at a time. Every pass then reads and writes in order, in runs,
 *    and nothing is scattered across megabytes: no bit-reversed reordering
 *    of the whole array, which on the Move was a cache miss a point.
 *    Measured there, building Noise's tables: 540 to 640 ms this way, 660
 *    to 700 stage by stage (2026-10-07). A laptop, whose caches hide its
 *    memory, finds it the other way round.
 */
#include <math.h>
#include <stdlib.h>

#include "fft.h"

#define SHORT (1 << 12)     /* longer than this, four steps */
#define STRIP 16            /* columns or rows moved together: 64 bytes a run */
#define SIDE 512            /* the longest side of a grid, FFT_MAX's square root */
#define BLOCK (1 << 14)     /* stages up to this done a block at a time, in the cache */

static float *grid_r, *grid_i;      /* the grid, kept from one long transform to the next */

/* stage len's turns at len / 2 .. len - 1 */
static float tc[FFT_MAX], ts[FFT_MAX];
static int ready;

static void turns(void) {
    /* the longest stage's, then every other one's taken from them */
    for (int k = 0; k < FFT_MAX / 2; k++) {
        tc[FFT_MAX / 2 + k] = (float)cos(2 * 3.14159265358979 * k / FFT_MAX);
        ts[FFT_MAX / 2 + k] = (float)sin(2 * 3.14159265358979 * k / FFT_MAX);
    }
    for (int len = FFT_MAX / 2; len >= 2; len >>= 1)
        for (int k = 0; k < len / 2; k++) {
            tc[len / 2 + k] = tc[len + 2 * k];
            ts[len / 2 + k] = ts[len + 2 * k];
        }
    ready = 1;
}

/* One row of butterflies, h of them. A function of its own, so the
 * compiler takes the halves' word that they never overlap, and runs it four
 * at a time; inside the loops below, GCC would not. */
static __attribute__((noinline)) void row(float *restrict pr, float *restrict pi, float *restrict qr,
                                          float *restrict qi, const float *c0, const float *s0, int h, float sg) {
    for (int k = 0; k < h; k++) {
        const float c = c0[k], s = sg * s0[k];
        const float xr = qr[k] * c - qi[k] * s, xi = qr[k] * s + qi[k] * c;
        qr[k] = pr[k] - xr; qi[k] = pi[k] - xi;
        pr[k] += xr; pi[k] += xi;
    }
}

/* A transform stage by stage, after reordering; the stages up to BLOCK
 * points a block at a time. All in the cache when it is short. */
static void short_fft(float *re, float *im, int n, int sign) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int i = 0; i < n; i += 2) {        /* turns of one: no need of a row */
        const float xr = re[i + 1], xi = im[i + 1];
        re[i + 1] = re[i] - xr, im[i + 1] = im[i] - xi;
        re[i] += xr, im[i] += xi;
    }
    const int b = n < BLOCK ? n : BLOCK;
    for (int i0 = 0; i0 < n; i0 += b)
        for (int len = 4; len <= b; len <<= 1) {
            const int h = len / 2;
            for (int i = i0; i < i0 + b; i += len)
                row(re + i, im + i, re + i + h, im + i + h, tc + h, ts + h, h, (float)sign);
        }
    for (int len = 2 * b; len <= n; len <<= 1) {
        const int h = len / 2;
        for (int i = 0; i < n; i += len)
            row(re + i, im + i, re + i + h, im + i + h, tc + h, ts + h, h, (float)sign);
    }
}

/* A long one in four steps, through the grid tr, ti. Input point
 * r * n2 + c is row r, column c; output k1 + n1 * k2 comes from column
 * transform k1, then row transform k2. */
static void long_fft(float *re, float *im, int n, int sign, float *tr, float *ti) {
    static float br[STRIP][SIDE], bi[STRIP][SIDE];
    int bits = 0;
    while ((1 << bits) < n) bits++;
    const int n1 = 1 << (bits / 2), n2 = n >> (bits / 2);  /* n1 down a column, n2 along a row */

    /* down each column, n1 long, a strip of columns at a time; then each
     * point turned by e^(sign j 2 pi c k1 / n), and laid in rows */
    for (int c0 = 0; c0 < n2; c0 += STRIP) {
        for (int r = 0; r < n1; r++)
            for (int g = 0; g < STRIP; g++) br[g][r] = re[r * n2 + c0 + g], bi[g][r] = im[r * n2 + c0 + g];
        for (int g = 0; g < STRIP; g++) {
            short_fft(br[g], bi[g], n1, sign);
            const double a = sign * 2 * 3.14159265358979 * (c0 + g) / n;
            const double sc = cos(a), ss = sin(a);
            double wc = 1, ws = 0;
            for (int k = 0; k < n1; k++) {
                const float xr = br[g][k], xi = bi[g][k];
                br[g][k] = (float)(xr * wc - xi * ws), bi[g][k] = (float)(xr * ws + xi * wc);
                const double t = wc * sc - ws * ss;
                ws = wc * ss + ws * sc, wc = t;
            }
        }
        for (int k = 0; k < n1; k++)
            for (int g = 0; g < STRIP; g++) tr[k * n2 + c0 + g] = br[g][k], ti[k * n2 + c0 + g] = bi[g][k];
    }
    /* along each row, n2 long, in place; then out, a strip of rows at a time */
    for (int k0 = 0; k0 < n1; k0 += STRIP) {
        for (int g = 0; g < STRIP; g++) short_fft(tr + (k0 + g) * n2, ti + (k0 + g) * n2, n2, sign);
        for (int k2 = 0; k2 < n2; k2++)
            for (int g = 0; g < STRIP; g++) {
                re[k0 + g + n1 * k2] = tr[(k0 + g) * n2 + k2];
                im[k0 + g + n1 * k2] = ti[(k0 + g) * n2 + k2];
            }
    }
}

void fft(float *re, float *im, int n, int sign) {
    if (!ready) turns();    /* only ever called from the one build (tables.c) */
    if (n > SHORT && !grid_r) {
        grid_r = malloc(sizeof(float) * FFT_MAX);
        grid_i = malloc(sizeof(float) * FFT_MAX);
    }
    if (n > SHORT && grid_r && grid_i) long_fft(re, im, n, sign, grid_r, grid_i);
    else short_fft(re, im, n, sign);        /* slower, never wrong */
}

void fft_done(void) {
    free(grid_r), free(grid_i);
    grid_r = grid_i = NULL;
}
