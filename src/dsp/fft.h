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

/* Gives back what long transforms kept between them; call when a build
 * is done. */
void fft_done(void);

/* Long transforms in four steps (fft.c), or stage by stage: the bench
 * builds both ways on the Move, whose memory decides which is quicker. */
extern int fft_four_steps;

#endif
