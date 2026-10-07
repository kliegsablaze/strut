/*
 * Noise, the noise source (DESIGN.md, Noise): a three-second loop of noise
 * with its own colour, read at PITCH, falling away by DECAY, and filtered by
 * COLOR. Its samples are build step 9.
 */
#ifndef STRUT_NOISE_H
#define STRUT_NOISE_H

#include <stdint.h>
#include <string.h>

#include "skin.h"

#define NT_BITS 18
#define NT_N (1 << NT_BITS)     /* the loop: 3 s, at twice the output's rate */
#define NT_SR (2 * STRUT_SR)    /* the loop's own rate (below) */
#define NT_FRAC (32 - NT_BITS)  /* the read position's fraction bits */
#define NT_LEVELS 7             /* copies an octave duller each, for PITCH up to +72 */
#define NT_TABLES 8             /* TABLE's options (params.c) */
#define NT_BANDS 40             /* quarter octaves from 20 Hz, for the level and Skin's strike */

enum { NT_WHITE, NT_PINK, NT_BROWN, NT_HISS, NT_WIRES, NT_METAL, NT_CRACKLE, NT_GRIT };

typedef struct {
    const int16_t *level[NT_LEVELS];    /* NT_N >> l samples, two before and five after to wrap */
    float band[NT_BANDS];               /* each band's share of the power, as floats */
    float var;                          /* the whole power */
} nt_table_t;

/* Builds the tables; called once, from wt_build (tables.c). */
void nt_build(void);
const nt_table_t *nt_table(int t);

typedef struct {
    uint32_t phase;         /* the read position, NT_FRAC bits of fraction; wraps the loop itself */
    float env, decay;
    float s1, s2;           /* COLOR's filter */
    int level;              /* the copy last read; -1 none yet */
    float match;            /* PITCH's and COLOR's level match, and what it was worked out for */
    float match_c, match_r;
    int match_t;
} noise_voice_t;

typedef struct {
    const int16_t *t[2];    /* this block's copy [0], and the last one's [1] while crossing */
    int shift[2];
    float fx[2];            /* the position's fraction to 0..1 */
    int cross;
    float dx;
    uint32_t inc;
    int filt;               /* 0 none, 1 low-pass, 2 high-pass */
    svf_t f;
    float g;                /* to floats, and the level match */
} noise_block_t;

/* Starts a note at strength amp. A hit on noise still sounding adds to it
 * as two noises do, by power, and reads on from where it is; otherwise it
 * starts somewhere new in the loop (from seed), so no two hits match. */
void noise_start(noise_voice_t *v, uint32_t seed, float amp);
/* Sets up a block; returns 0 once the fall has died away. */
/* hold: CURVE's Hold or Swell, the fall paused for now */
int noise_block(noise_voice_t *v, const float *p, int frames, int hold, noise_block_t *b);
void noise_skip(noise_voice_t *v, const float *p, int frames, int hold);
float noise_t60(const float *p);
/* Noise as Skin's hit: how strongly it drives a resonance at hz, through
 * skin.c's strike (decay d a sample, len samples), as an expected size. */
float noise_strike(noise_voice_t *v, const float *p, float hz, float d, int len);

/* Six points through a fifth-order curve (Lagrange; Laakso et al. 1996).
 * The loop is stored at twice the output's rate with nothing over 20 kHz,
 * a quarter of its own rate, so the curve's images of it, which a slowed
 * noise would otherwise carry as hiss, stay 40 dB down. (A cubic through
 * four points, at the output's own rate, left them 4 dB down.) The points
 * are loaded and made floats four at a time, which the Move does in two
 * instructions where one at a time took two each. */
typedef int16_t nt_s4 __attribute__((vector_size(8)));
typedef float nt_f4 __attribute__((vector_size(16)));

static inline float nt_read(const int16_t *t, uint32_t ph, int shift, float fx) {
    const int32_t i = (int32_t)(ph >> shift);
    const float x = (float)(ph & ((1u << shift) - 1u)) * fx;
    const float a = x + 2.0f, b = x + 1.0f, c = x, d = x - 1.0f, e = x - 2.0f, f = x - 3.0f;
    const float ab = a * b, abc = ab * c, abcd = abc * d;
    const float ef = e * f, def = d * ef, cdef = c * def;
    const nt_f4 wl = { b * cdef * (-1.0f / 120), a * cdef * (1.0f / 24), ab * def * (-1.0f / 12), abc * ef * (1.0f / 12) };
    const nt_f4 wh = { abcd * f * (-1.0f / 24), abcd * e * (1.0f / 120), 0.0f, 0.0f };
    nt_s4 lo, hi;
    memcpy(&lo, t + i - 2, sizeof(lo));     /* t[i - 2] .. t[i + 1] */
    memcpy(&hi, t + i + 2, sizeof(hi));     /* t[i + 2] .. t[i + 5], the last two unused */
    const nt_f4 y = __builtin_convertvector(lo, nt_f4) * wl + __builtin_convertvector(hi, nt_f4) * wh;
    return (y[0] + y[1]) + (y[2] + y[3]);
}

/* The next sample: raw is the noise through COLOR (Skin's hit), the return
 * it through the fall. n counts the block. */
static inline float noise_step(noise_voice_t *v, const noise_block_t *b, int n, float *raw) {
    float x = nt_read(b->t[0], v->phase, b->shift[0], b->fx[0]);
    if (b->cross) {
        const float old = nt_read(b->t[1], v->phase, b->shift[1], b->fx[1]);
        x = old + (x - old) * b->dx * (float)(n + 1);
    }
    v->phase += b->inc;
    x *= b->g;
    if (b->filt) {
        float lp, bp, hp;
        svf_step(&b->f, &v->s1, &v->s2, x, &lp, &bp, &hp);
        x = b->filt == 1 ? lp : hp;
    }
    *raw = x;
    const float y = x * v->env;
    v->env *= v->decay;
    return y;
}

#endif
