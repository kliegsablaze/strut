/*
 * The FFT (fft.h). Its turns come from one table, worked out on first use, so
 * a long transform costs no sine or cosine of its own: the 256k-point noise
 * loops would otherwise spend most of their build on them. Kept as floats,
 * 1 MB: their error is 150 dB down, far under anything a table keeps.
 */
#include <math.h>

#include "fft.h"

static float tc[FFT_MAX / 2], ts[FFT_MAX / 2];
static int ready;

void fft(double *re, double *im, int n, int sign) {
    if (!ready) {           /* only ever called from the one build (tables.c) */
        for (int k = 0; k < FFT_MAX / 2; k++) {
            tc[k] = (float)cos(2 * 3.14159265358979 * k / FFT_MAX);
            ts[k] = (float)sin(2 * 3.14159265358979 * k / FFT_MAX);
        }
        ready = 1;
    }
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j |= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const int step = FFT_MAX / len;
        for (int i = 0; i < n; i += len)
            for (int k = 0; k < len / 2; k++) {
                const double c = tc[k * step], s = sign * ts[k * step];
                const int p = i + k, q = p + len / 2;
                const double xr = re[q] * c - im[q] * s, xi = re[q] * s + im[q] * c;
                re[q] = re[p] - xr; im[q] = im[p] - xi;
                re[p] += xr; im[p] += xi;
            }
    }
}
