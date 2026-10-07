/*
 * Wave, the oscillator (DESIGN.md, Wave): a table of single cycles swept by
 * WAVE, a pitch sweep at the start (BEND), its own fall (DECAY), Skin's ring
 * bending its frequency (FM), and a ring modulator (RING).
 */
#ifndef STRUT_WAVE_H
#define STRUT_WAVE_H

#include <math.h>

#include "tables.h"

#define WAVE_SUB 16         /* samples between pitch updates, glided across */
#define WAVE_FADE 32        /* samples the start fades in over, against a click */

typedef struct {
    float phase, rphase;    /* the oscillator's and the ring's, 0..1 */
    float env, decay;       /* the fall, and its step a sample */
    float bend;             /* the pitch sweep, in semitones */
    int n;                  /* samples played */
    int level;              /* the brightness last read; -1 none yet */
} wave_voice_t;

/* One block's settings: what to read, how fast, and how much. */
typedef struct {
    const float *ta[2], *tb[2];     /* two frames, at this block's brightness [0] */
    int size[2];                    /* and, crossing into it, the last one's [1] */
    int cross;                      /* fading from [1] to [0] across the block */
    float dx;                       /* the fade's step */
    float ca, cb, off;              /* ta * ca + tb(phase + off) * cb */
    float inc[WAVE_SUB + 1], dinc;  /* phase steps at sub-block edges */
    int sub;                        /* sub-blocks in this block */
    float fm, ratio, rmix;          /* FM index, ring ratio and amount */
} wave_block_t;

void wave_start(wave_voice_t *w, const float *p);
/* Sets up a block of frames; returns 0 once the fall has died away. */
int wave_block(wave_voice_t *w, const float *p, int frames, wave_block_t *b);
/* Moves on a block without sounding, so turning WAVE up mid-note joins it. */
void wave_skip(wave_voice_t *w, const float *p, int frames);
/* Wave as Skin's hit: how long it lasts, at least one of Wave's cycles
 * (SNAP's len samples, or more), and how strongly it drives a resonance at
 * hz (skin.c's strike, e^(-5n/len), decay d a sample). */
int wave_strike_len(const float *p, int len);
float wave_strike(const float *p, float hz, float d, int len);
float wave_hz(const float *p);
float wave_t60(const float *p);

static inline float wt_read(const float *t, int n, float ph) {
    const float x = ph * (float)n;
    int i = (int)x;
    if (i >= n) i = n - 1;      /* a phase rounded up to exactly 1 */
    return t[i] + (x - (float)i) * (t[i + 1] - t[i]);
}

/* The next sample: raw is the oscillator (Skin's hit), the return is it
 * faded in and through the fall. fm is Skin's ring, about -1..1. n counts the block. */
static inline float wave_step(wave_voice_t *w, const wave_block_t *b, int n, float fm, float *raw) {
    const int k = n / WAVE_SUB;
    const float inc = b->inc[k] + (b->inc[k + 1] - b->inc[k]) * (float)(n % WAVE_SUB) * (1.0f / WAVE_SUB);
    float pb = w->phase + b->off;
    pb -= floorf(pb);
    float x = b->ca * wt_read(b->ta[0], b->size[0], w->phase) + b->cb * wt_read(b->tb[0], b->size[0], pb);
    if (b->cross) {
        const float old = b->ca * wt_read(b->ta[1], b->size[1], w->phase) + b->cb * wt_read(b->tb[1], b->size[1], pb);
        x = old + (x - old) * b->dx * (float)(n + 1);
    }
    if (b->rmix > 0.0f)
        x *= 1.0f - b->rmix + b->rmix * wt_read(wt_sine, WT_SINE_N, w->rphase);
    *raw = x;
    if (w->n < WAVE_FADE) x *= (float)w->n * (1.0f / WAVE_FADE);
    w->n++;
    w->phase += inc * (1.0f + b->fm * fm);     /* through zero is fine */
    w->phase -= floorf(w->phase);
    w->rphase += inc * b->ratio;
    w->rphase -= floorf(w->rphase);
    const float y = x * w->env;
    w->env *= w->decay;
    return y;
}

#endif
