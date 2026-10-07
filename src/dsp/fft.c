/*
 * The FFT (fft.h). Its turns come from one table, worked out on first use, so
 * a long transform costs no sine or cosine of its own: the 256k-point noise
 * loops would otherwise spend most of their build on them. Kept as floats,
 * 2 MB, as the transform itself is (fft.h).
 *
 * Built for a small cache: the Move took 2.8 s to load 0.2.0, nearly all of
 * it waiting on memory (a simulation of its caches counted 1.6 million
 * misses a transform). Each stage's turns lie side by side, where one table
 * read with a stride cost a whole cache line for every turn; and the stages
 * up to BLOCK points are done a block at a time, in the cache, so only the
 * last few sweep the whole array.
 */
#include <math.h>

#include "fft.h"

#define BLOCK (1 << 14)     /* 256 KB of points, inside the Move's 1 MB cache */

/* stage len's turns at len / 2 .. len - 1 */
static float tc[FFT_MAX], ts[FFT_MAX];
static int ready;

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

static void stages(float *re, float *im, int from, int to, int lo, int hi, int sign) {
    for (int len = lo; len <= hi; len <<= 1) {
        const int h = len / 2;
        if (len == 2) {         /* turns of one: no need of a row */
            for (int i = from; i < to; i += 2) {
                const float xr = re[i + 1], xi = im[i + 1];
                re[i + 1] = re[i] - xr, im[i + 1] = im[i] - xi;
                re[i] += xr, im[i] += xi;
            }
            continue;
        }
        for (int i = from; i < to; i += len)
            row(re + i, im + i, re + i + h, im + i + h, tc + h, ts + h, h, (float)sign);
    }
}

void fft_reversed(float *re, float *im, int n, int sign) {
    if (!ready) {           /* only ever called from the one build (tables.c) */
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
    const int b = n < BLOCK ? n : BLOCK;
    for (int i = 0; i < n; i += b) stages(re, im, i, i + b, 2, b, sign);
    stages(re, im, 0, n, 2 * b, n, sign);
}

void fft(float *re, float *im, int n, int sign) {
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    fft_reversed(re, im, n, sign);
}
