/*
 * Skin, the resonator (DESIGN.md, Skin): a short hit sets a ringing filter
 * going, the way an analogue drum's kick and toms work.
 */
#ifndef STRUT_SKIN_H
#define STRUT_SKIN_H

#include <stdint.h>

#define SKIN_PARTIALS 3     /* the body, and METAL's two */

typedef struct {
    /* the hit, made on the fly from a seed */
    int kind, n, len;
    uint32_t seed;
    float decay, env;
    float norm;             /* scales the hit so the body rings at one */
    float gain;             /* the last block's, so a level change glides */
    /* three phasor resonators (Mathews and Smith), real and imaginary */
    float zr[SKIN_PARTIALS], zi[SKIN_PARTIALS];
    /* MODE's and TONE's state-variable filters (Zavalishin) */
    float m1, m2, t1, t2;
} skin_voice_t;

/* Starts a hit with the pad's knobs p (STRUT_PAD_PARAMS order). */
void skin_start(skin_voice_t *v, const float *p, uint32_t seed);

/* Adds frames of Skin into out, times gain (gliding there from the last
 * block's across this one); returns 0 once it has died away. */
int skin_render(skin_voice_t *v, const float *p, float gain, float *out, int frames);

/* What the knobs mean in physical units, shared with the tests. */
float skin_hz(const float *p);
float skin_t60(const float *p);

#endif
