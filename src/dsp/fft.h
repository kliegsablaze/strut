/*
 * An FFT for building the tables at load (Wave's and Noise's), never on the
 * audio thread.
 */
#ifndef STRUT_FFT_H
#define STRUT_FFT_H

#define FFT_MAX (1 << 18)   /* the longest, Noise's loop */

/* In place, radix 2, n a power of two up to FFT_MAX: sign -1 forward, +1
 * inverse, unscaled. */
void fft(double *re, double *im, int n, int sign);

#endif
