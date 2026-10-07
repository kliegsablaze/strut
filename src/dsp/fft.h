/*
 * An FFT for building the tables at load (Wave's and Noise's), never on the
 * audio thread.
 */
#ifndef STRUT_FFT_H
#define STRUT_FFT_H

#define FFT_MAX (1 << 18)   /* the longest, Noise's loop */

/* In place, radix 2, n a power of two up to FFT_MAX: sign -1 forward, +1
 * inverse, unscaled. In single precision: its error, about 120 dB down, is
 * far under the 16 bits a noise table keeps, and it moves half the memory
 * and does twice the sums an instruction of double precision. */
void fft(float *re, float *im, int n, int sign);

/* The same, for input already put in bit-reversed order (fft_rev): a long
 * transform's reordering is a pass of scattered reads and writes, slow on
 * the Move, which whoever fills the input can do for free. */
void fft_reversed(float *re, float *im, int n, int sign);

/* i with its lowest `bits` bits reversed. */
static inline int fft_rev(int i, int bits) {
    unsigned x = (unsigned)i;
    x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
    x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
    x = ((x >> 4) & 0x0F0F0F0Fu) | ((x & 0x0F0F0F0Fu) << 4);
    x = ((x >> 8) & 0x00FF00FFu) | ((x & 0x00FF00FFu) << 8);
    x = (x >> 16) | (x << 16);
    return (int)(x >> (32 - bits));
}

#endif
