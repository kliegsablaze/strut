/*
 * How loud each library sample plays (levels.c, made by tools/levels.c),
 * so DICE can set a sample's fader to land near the others. The library's
 * files differ by up to 20 dB as recorded.
 */
#ifndef STRUT_LEVELS_H
#define STRUT_LEVELS_H

typedef struct {
    const char *name;
    float db;               /* its loudest 400 ms, in dB, at DICE's fader */
    float pk;               /* its peak there, in dB */
} level_t;

extern const level_t LEVELS[];
extern const int LEVELS_N;
extern const float LEVELS_MEDIAN;

/* A sample's level, in dB; the library's middle if not listed (your own).
 * Its peak, in dB, into *pk if pk is not NULL; -6 if not listed. */
float levels_of(const char *name, float *pk);

/* Measures TABLE entry t as DICE plays it, rendering into l and r (4 s
 * each): its level, and its peak into *pk. For tools/levels.c and the
 * tests; never on the audio thread. */
float levels_measure(int t, float *l, float *r, float *pk);

#endif
