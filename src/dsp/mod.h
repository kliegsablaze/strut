/*
 * The modulators (DESIGN.md, Modulation): one an engine, from its page's Mod
 * view. Each hit restarts them. Like Quilt's, they never write a knob: each
 * block, the pad's knobs are copied and the copy is moved, so a knob always
 * shows what was set.
 *
 * Each engine's CURVE, the shape of its own fall, lives here too: it is a
 * gain over time, and for Hold and Swell a pause in the engine's own fall.
 */
#ifndef STRUT_MOD_H
#define STRUT_MOD_H

#include <stdint.h>

enum { ENG_SKIN, ENG_WAVE, ENG_NOISE, ENG_COUNT };
enum { KIND_ENVELOPE, KIND_LFO, KIND_RANDOM, KIND_VELOCITY };
enum { CURVE_NATURAL, CURVE_PING, CURVE_SOFT, CURVE_HOLD, CURVE_SWELL };

#define MOD_SUB 64          /* samples between updates while anything moves: 1.45 ms */

/* What a hit leaves the modulators. */
typedef struct {
    float t;                /* seconds since the hit */
    float vel;              /* the hit's velocity, 0..1 */
    float rnd[ENG_COUNT];   /* Random's value for this hit, -1..1 */
} mod_t;

/* One moment of a voice: the moved knobs, and for each engine its gain
 * (CURVE, and Level as a destination), whether its own fall waits (Hold,
 * Swell), and whether CURVE has ended it. */
typedef struct {
    float gain[ENG_COUNT];
    int hold[ENG_COUNT];
    int over[ENG_COUNT];
    float T[ENG_COUNT];     /* each engine's fall, for CURVE: worked out once a block */
    int have_T;             /* clear it at a block's start */
} mod_out_t;

void mod_hit(mod_t *m, float vel, uint32_t seed);
/* 0 when nothing moves: no depth and every CURVE Natural, so the voice can
 * run a whole block plainly. */
int mod_any(const float *p);
/* The knobs p moved by the modulators at m's moment, into q. */
void mod_apply(const float *p, const mod_t *m, float bpm, float *q, mod_out_t *o);

#endif
