/*
 * Skin, the resonator (DESIGN.md, Skin): a short hit sets a ringing filter
 * going, the way an analogue drum's kick and toms work.
 */
#ifndef STRUT_SKIN_H
#define STRUT_SKIN_H

#include <math.h>
#include <stdint.h>

#define SKIN_PARTIALS 3     /* the body, and METAL's two */

enum { HIT_CLICK, HIT_SOFT, HIT_BURST, HIT_WAVE, HIT_NOISE };
enum { MODE_LOW, MODE_BAND, MODE_HIGH };

typedef struct {
    /* the hit, made on the fly from a seed, or Wave's sound */
    int kind, n, len;
    uint32_t seed;
    float decay, env;
    float amp;              /* the hit's strength, from velocity */
    float norm;             /* scales the hit so the body rings at one */
    /* three phasor resonators (Mathews and Smith), real and imaginary */
    float zr[SKIN_PARTIALS], zi[SKIN_PARTIALS];
    /* MODE's and TONE's state-variable filters (Zavalishin) */
    float m1, m2, t1, t2;
} skin_voice_t;

/* A trapezoidal state-variable filter's coefficients. */
typedef struct { float a1, a2, a3, k; } svf_t;
/* At hz, damped by k (1.414 flat, less peaks), Zavalishin's form. */
svf_t svf(float hz, float k);

/* One block's settings. */
typedef struct {
    float pr[SKIN_PARTIALS], pi[SKIN_PARTIALS], amp[SKIN_PARTIALS];
    int np;                 /* partials to run: METAL's two only when heard */
    svf_t mf, tf;
    int mode;
} skin_block_t;

/* Strikes the drum with the pad's knobs p (STRUT_PAD_PARAMS order), at
 * strength amp: a new hit on whatever is still ringing, as a real drum is
 * struck again. A Wave hit's size is left to the caller, who knows Wave
 * (wave_strike), through skin_resize. */
void skin_strike(skin_voice_t *v, const float *p, uint32_t seed, float amp);
/* Sets the hit's scale, keeping what already rings at its level. */
void skin_resize(skin_voice_t *v, float norm);
/* hold: CURVE's Hold or Swell, the ring kept from falling for now */
void skin_block(skin_voice_t *v, const float *p, int hold, skin_block_t *b);
/* 0 once the hit is over and the ring has died away. */
int skin_alive(const skin_voice_t *v);

/* What the knobs mean in physical units, shared with Wave and the tests. */
float skin_hz(const float *p);
float skin_t60(const float *p);

static inline uint32_t skin_rng(uint32_t *s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

/* The hit's next sample. ext is the other engine's sound, for a Wave or
 * Noise hit. */
static inline float skin_hit(skin_voice_t *v, float ext) {
    if (v->n >= v->len) return 0.0f;
    float x;
    switch (v->kind) {
    case HIT_CLICK:
        x = v->env;
        v->env *= v->decay;
        break;
    case HIT_SOFT:
        x = 0.5f - 0.5f * cosf(6.2831853f * (float)v->n / (float)v->len);
        break;
    case HIT_WAVE:
    case HIT_NOISE:
        x = ext * v->env;
        v->env *= v->decay;
        break;
    default:
        x = ((float)(int32_t)skin_rng(&v->seed) * (1.0f / 2147483648.0f)) * v->env;
        v->env *= v->decay;
        break;
    }
    v->n++;
    return x * v->amp;
}

static inline void svf_step(const svf_t *f, float *s1, float *s2, float x, float *lp, float *bp, float *hp) {
    const float v3 = x - *s2;
    const float v1 = f->a1 * *s1 + f->a2 * v3;
    const float v2 = *s2 + f->a2 * *s1 + f->a3 * v3;
    *s1 = 2.0f * v1 - *s1;
    *s2 = 2.0f * v2 - *s2;
    *lp = v2;
    *bp = v1;
    *hp = x - f->k * v1 - v2;
}

/* The next sample, before any level. np is b->np, passed as a constant so
 * the partials unroll and the voice's state stays in registers. */
static inline float skin_step(skin_voice_t *v, const skin_block_t *b, float ext, const int np) {
    const float x = skin_hit(v, ext);
    float y = 0.0f;
    for (int k = 0; k < np; k++) {
        const float zr = v->zr[k] * b->pr[k] - v->zi[k] * b->pi[k] + x;
        v->zi[k] = v->zr[k] * b->pi[k] + v->zi[k] * b->pr[k];
        v->zr[k] = zr;
        y += b->amp[k] * v->zi[k];
    }
    y = (0.7f * y + 0.5f * x) * v->norm;    /* the ring, and the hit itself */
    float lp, bp, hp;
    svf_step(&b->mf, &v->m1, &v->m2, y, &lp, &bp, &hp);
    y = b->mode == MODE_LOW ? lp : b->mode == MODE_HIGH ? hp : b->mf.k * bp;
    svf_step(&b->tf, &v->t1, &v->t2, y, &lp, &bp, &hp);
    return lp;
}

/* The body's ring, about -1..1 at the hit: what Wave's FM follows. */
static inline float skin_body(const skin_voice_t *v) { return v->zi[0] * v->norm; }

#endif
