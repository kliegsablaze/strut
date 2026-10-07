/*
 * The modulators and CURVE (mod.h).
 */
#include <math.h>

#include "mod.h"
#include "strut.h"

#define LEVEL (-1)

/* Each engine's Mod view: KIND, RATE, CURVE, AIM, DEPTH, AIM, DEPTH, in a
 * row of STRUT_PAD_PARAMS; and its AIM's options as knobs (params.c). */
static const int FIRST[ENG_COUNT] = { P_S_KIND, P_W_KIND, P_N_KIND };
static const int AIMS[ENG_COUNT][6] = {
    { P_S_PITCH, P_S_RING, P_S_SNAP, P_S_METAL, P_S_TONE, LEVEL },
    { P_W_PITCH, P_W_WAVE, P_W_FM, P_W_RING, LEVEL, LEVEL },
    { P_N_PITCH, P_N_COLOR, P_N_START, P_N_LOOP, LEVEL, LEVEL },
};
enum { M_KIND, M_RATE, M_CURVE, M_AIM1, M_DEPTH1, M_AIM2, M_DEPTH2 };

void mod_hit(mod_t *m, float vel, uint32_t seed) {
    m->t = 0.0f;
    m->vel = vel;
    for (int e = 0; e < ENG_COUNT; e++) {
        seed ^= seed << 13, seed ^= seed >> 17, seed ^= seed << 5;
        m->rnd[e] = (float)(int32_t)seed * (1.0f / 2147483648.0f);
    }
}

int mod_any(const float *p) {
    for (int e = 0; e < ENG_COUNT; e++) {
        const float *k = p + FIRST[e];
        if (k[M_DEPTH1] != 0.0f || k[M_DEPTH2] != 0.0f || k[M_CURVE] != CURVE_NATURAL) return 1;
    }
    return 0;
}

/* RATE as a time: left of centre free, 5 ms at the centre to 4 s fully
 * left; right of centre a note value at the tempo, from a 64th to four
 * bars. An Envelope's length, an LFO's cycle. */
static float rate_time(float r, float bpm) {
    if (r <= 0.0f) return 0.005f * powf(800.0f, -r);
    static const float BEATS[] = { 1.0f / 16, 1.0f / 8, 1.0f / 6, 1.0f / 4, 1.0f / 3, 1.0f / 2,
                                   2.0f / 3, 1, 2, 4, 8, 16 };
    const int n = (int)(sizeof(BEATS) / sizeof(BEATS[0]));
    const int i = (int)(r * (float)(n - 1) + 0.5f);
    return BEATS[i] * 60.0f / bpm;
}

/* The modulator's value now: Envelope 1 falling to 0, LFO a sine from 0,
 * Random this hit's own, Velocity the hit's. */
static float value(const float *k, const mod_t *m, int e, float bpm) {
    switch ((int)k[M_KIND]) {
    case KIND_ENVELOPE: return expf(-6.9078f * m->t / rate_time(k[M_RATE], bpm));
    case KIND_LFO: return sinf(6.2831853f * m->t / rate_time(k[M_RATE], bpm));
    case KIND_RANDOM: return m->rnd[e];
    default: return m->vel;
    }
}

/* A destination moved by depth times the value: a pitch by up to four
 * octaves (finer near the centre: 48 d |d| semitones), Level by a factor
 * from off to twice, any other knob across its whole range. */
static void move(float *q, float *level, int aim, float d, float v) {
    if (d == 0.0f) return;
    if (aim == LEVEL) {
        *level *= fminf(fmaxf(1.0f + d * v, 0.0f), 2.0f);
        return;
    }
    const param_def_t *pd = &STRUT_PAD_PARAMS[aim];
    if (pd->unit && pd->unit[0] == 's') {      /* semitones */
        q[aim] += 48.0f * d * fabsf(d) * v;
        return;
    }
    q[aim] = fminf(fmaxf(q[aim] + d * v * (pd->max - pd->min), pd->min), pd->max);
}

/* CURVE: the engine's gain at t, its fall T long (its RING or DECAY).
 * Natural is the engine's own fall. Ping falls twice as fast. Soft rises
 * over a tenth of it (2 to 80 ms). Hold stays full for half of it, its own
 * fall paused, then lets go over a tenth (at least 5 ms). Swell rises from
 * 60 dB down to full over it, its own fall paused, then lets go in 10 ms:
 * a sound played backwards. */
static float curve(int c, float t, float T, int *hold, int *over) {
    *hold = *over = 0;
    switch (c) {
    case CURVE_PING: return expf(-6.9078f * t / T);
    case CURVE_SOFT: {
        const float a = fminf(fmaxf(0.1f * T, 0.002f), 0.08f);
        return t >= a ? 1.0f : 0.5f - 0.5f * cosf(3.14159265f * t / a);
    }
    case CURVE_HOLD: {
        const float h = 0.5f * T, r = fmaxf(0.1f * T, 0.005f);
        if (t < h) { *hold = 1; return 1.0f; }
        const float g = expf(-6.9078f * (t - h) / r);
        *over = g < 1e-5f;
        return g;
    }
    case CURVE_SWELL: {
        *hold = 1;
        if (t < T) return powf(10.0f, -3.0f * (1.0f - t / T));
        const float g = expf(-6.9078f * (t - T) / 0.01f);
        *over = g < 1e-5f;
        return g;
    }
    default: return 1.0f;
    }
}

void mod_apply(const float *p, const mod_t *m, float bpm, float *q, mod_out_t *o) {
    for (int k = 0; k < P_COUNT; k++) q[k] = p[k];
    for (int e = 0; e < ENG_COUNT; e++) {
        const float *k = p + FIRST[e];
        o->gain[e] = 1.0f;
        if (k[M_DEPTH1] == 0.0f && k[M_DEPTH2] == 0.0f) continue;
        const float v = value(k, m, e, bpm);
        move(q, &o->gain[e], AIMS[e][(int)k[M_AIM1]], k[M_DEPTH1], v);
        move(q, &o->gain[e], AIMS[e][(int)k[M_AIM2]], k[M_DEPTH2], v);
    }
    /* CURVE after the knobs have moved, so it follows a moved RING or DECAY */
    const float T[ENG_COUNT] = { skin_t60(q), wave_t60(q), noise_t60(q) };
    for (int e = 0; e < ENG_COUNT; e++)
        o->gain[e] *= curve((int)p[FIRST[e] + M_CURVE], m->t, T[e], &o->hold[e], &o->over[e]);
}
