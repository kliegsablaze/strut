/*
 * Wave's tables (DESIGN.md, How Wave works): single cycles, built once when
 * Strut loads and shared by every instance. Each cycle is kept at ten
 * brightnesses, an octave apart, so a note reads one with nothing above
 * the audio band (a mipmap). No data files.
 */
#ifndef STRUT_TABLES_H
#define STRUT_TABLES_H

#define WT_H 512            /* harmonics in the brightest copy */
#define WT_LEVELS 10        /* copies, each with half the harmonics of the last */
#define WT_FRAMES 16        /* frames in a table, swept by the WAVE knob */
#define WT_TABLES 8         /* TABLE's options (params.c) */
#define WT_SINE_N 1024      /* the ring modulator's sine */

enum { WT_ANALOG, WT_SYNC, WT_FOLD, WT_SWEEP, WT_VOWEL, WT_HOLLOW, WT_METAL, WT_GLASS };
enum { WT_A_SINE, WT_A_TRI, WT_A_SAW };    /* Analog's three frames */

typedef struct {
    const float *level[WT_LEVELS];  /* WT_N(l) samples each, plus one to wrap */
    float ca[WT_H + 1], sb[WT_H + 1];   /* its harmonics, cos and sin, for Skin's strike */
} wt_frame_t;

/* Samples in level l: four a harmonic, never under 256. */
#define WT_N(l) ((4 * (WT_H >> (l))) < 256 ? 256 : 4 * (WT_H >> (l)))

/* Builds the tables, Wave's and Noise's (noise.h), once per process; safe
 * to call from every instance. */
void wt_build(void);

/* Processor time each part of the build took, in seconds, for the bench:
 * Wave's tables; Noise's spectra, transforms, and storing. The Move loads
 * far slower than a laptop would say (DESIGN.md, Tables are built at load). */
enum { WT_P_WAVE, WT_P_SPECTRA, WT_P_FFT, WT_P_STORE, WT_P_COUNT };
extern double wt_profile[WT_P_COUNT];

/* Frame f of table t. Analog has its three frames (WT_A_*); the others
 * have WT_FRAMES. */
const wt_frame_t *wt_frame(int t, int f);
extern float wt_sine[WT_SINE_N + 1];

#endif
