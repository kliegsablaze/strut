/*
 * Skin (skin.h). A hit drives three phasor resonators: the body at PITCH,
 * and METAL's two partials above it. A phasor (Mathews and Smith, SMAC 2003)
 * is one complex multiply a sample, z = p z + x, with p = r e^{jw}: it rings
 * at w, dies away by r, and stays well behaved when either moves mid-note.
 * Then MODE shapes the hit and the ring together, and TONE darkens them.
 *
 * The hit is scaled so the body always rings at about one, whatever the hit,
 * its length or the pitch: the hit's own spectrum at PITCH is worked out when
 * the note starts, and divided out. A soft or long hit still sounds softer,
 * because its partials and its click are weaker. Striking at a null of the
 * hit's spectrum rings quietly, as striking a drum at a node does.
 *
 * It runs a sample at a time (skin.h) beside Wave, so Wave can strike it
 * and its ring can bend Wave's pitch (strut.c).
 */
#include <math.h>

#include "skin.h"
#include "strut.h"

#define TWO_PI 6.2831853f
#define NYQ_SAFE (0.45f * STRUT_SR)

/* METAL moves the two partials from a drumhead's ratios (the circular
 * membrane's (1,1) and (2,1) modes over its (0,1)) to a free bar's (Rossing,
 * Science of Percussion Instruments, chs. 2 and 6). */
static const float DRUM_RATIO[2] = { 1.594f, 2.136f };
static const float BAR_RATIO[2] = { 2.756f, 5.404f };

float skin_hz(const float *p) {
    const float hz = 55.0f * powf(2.0f, (p[P_S_PITCH] + p[P_TUNE]) / 12.0f);
    return fminf(fmaxf(hz, 20.0f), NYQ_SAFE);
}

/* RING from 15 ms to 4 s; the Pad page's DECAY scales it by a quarter to
 * four. Never under two and a half cycles: a low drum rung shorter than its
 * own period is a click with no pitch, not a short drum. */
float skin_t60(const float *p) {
    const float t = 0.015f * powf(4.0f / 0.015f, p[P_S_RING]) * powf(4.0f, p[P_DECAY]);
    return fminf(fmaxf(t, 2.5f / skin_hz(p)), 12.0f);
}

/* SNAP: how long the hit lasts, 0.2 ms to 50 ms. */
static int hit_len(const float *p) {
    return (int)(0.0002f * powf(250.0f, p[P_S_SNAP]) * STRUT_SR) + 1;
}

void skin_resize(skin_voice_t *v, float norm) {
    /* the resonators hold the ring before scaling, so rescale them, or a
     * new hit's scale would jump what still rings */
    if (v->norm > 0.0f)
        for (int k = 0; k < SKIN_PARTIALS; k++) {
            v->zr[k] *= v->norm / norm;
            v->zi[k] *= v->norm / norm;
        }
    v->norm = norm;
}

void skin_strike(skin_voice_t *v, const float *p, uint32_t seed, float amp) {
    v->kind = (int)p[P_S_HIT];
    v->seed = seed | 1u;
    v->amp = amp;
    v->n = 0;
    const int len = hit_len(p);
    if (v->kind == HIT_CLICK) {
        /* an exponential pulse whose time constant is a fifth of SNAP */
        const float tau = 0.2f * (float)len;
        v->decay = expf(-1.0f / tau);
        v->len = (int)(9.2f * tau) + 1;     /* down 80 dB */
    } else {
        v->decay = expf(-5.0f / (float)len);
        v->len = len;
    }
    v->env = 1.0f;

    /* The hit's size at PITCH, worked out rather than measured, so starting
     * sixteen hits in one block costs nothing to speak of. */
    const float f = skin_hz(p);
    float mag, sum;
    if (v->kind == HIT_CLICK) {
        /* a geometric series: |1 / (1 - d e^{-jw})| */
        const float w = TWO_PI * f / STRUT_SR, d = v->decay;
        mag = 1.0f / sqrtf(1.0f - 2.0f * d * cosf(w) + d * d);
        sum = 1.0f / (1.0f - d);
    } else if (v->kind == HIT_SOFT) {
        /* a raised cosine of length T: (T/2) |sinc(fT) / (1 - (fT)^2)| */
        const float ft = f * (float)v->len / STRUT_SR, x = 3.14159265f * ft;
        const float sinc = ft < 1e-4f ? 1.0f : sinf(x) / x;
        const float den = 1.0f - ft * ft;
        mag = 0.5f * (float)v->len * (fabsf(den) < 1e-3f ? 0.5f : fabsf(sinc / den));
        sum = 0.5f * (float)v->len;
    } else {
        /* noise: its expected size, from its energy (a uniform sample's
         * variance is a third). Each burst then rings a little differently.
         * A Wave or Noise hit is sized again by the caller (strut.c). */
        const float d2 = v->decay * v->decay;
        mag = sqrtf((1.0f - powf(d2, (float)v->len)) / (1.0f - d2) / 3.0f);
        sum = 0.5f * (1.0f - powf(v->decay, (float)v->len)) / (1.0f - v->decay);
    }
    skin_resize(v, 1.0f / fmaxf(fmaxf(mag, 0.01f * sum), 1e-6f));
}

svf_t svf(float hz, float k) {
    const float g = tanf(3.14159265f * fminf(fmaxf(hz, 10.0f), NYQ_SAFE) / STRUT_SR);
    svf_t f = { 0, 0, 0, k };
    f.a1 = 1.0f / (1.0f + g * (g + k));
    f.a2 = g * f.a1;
    f.a3 = g * f.a2;
    return f;
}

void skin_block(skin_voice_t *v, const float *p, int hold, skin_block_t *b) {
    const float hz = skin_hz(p), t60 = hold ? 12.0f : skin_t60(p), metal = p[P_S_METAL];
    for (int k = 0; k < SKIN_PARTIALS; k++) {
        const float ratio = k ? DRUM_RATIO[k - 1] + metal * (BAR_RATIO[k - 1] - DRUM_RATIO[k - 1]) : 1.0f;
        const float f = hz * ratio;
        /* the partials die sooner, less so as they turn to metal */
        const float t = k ? t60 * (0.35f + 0.65f * metal) : t60;
        const float r = expf(-6.9078f / (t * STRUT_SR));
        const float w = TWO_PI * f / STRUT_SR;
        b->pr[k] = r * cosf(w);
        b->pi[k] = r * sinf(w);
        b->amp[k] = k ? (f < NYQ_SAFE ? 0.7f * metal : 0.0f) : 1.0f;
    }
    /* METAL at zero: the body alone, a third of the work. The partials are
     * cleared, so turning METAL up again starts them from the next hit's
     * ring, not a stale one. Nothing heard changes. */
    b->np = metal > 0.0f ? SKIN_PARTIALS : 1;
    if (b->np == 1)
        for (int k = 1; k < SKIN_PARTIALS; k++) v->zr[k] = v->zi[k] = 0.0f;
    b->mode = (int)p[P_S_MODE];
    b->mf = b->mode == MODE_LOW ? svf(2.0f * hz, 1.4142f)
          : b->mode == MODE_HIGH ? svf(0.5f * hz, 1.4142f) : svf(hz, 1.0f);
    b->tf = svf(150.0f * powf(120.0f, p[P_S_TONE]), 1.4142f);
}

int skin_alive(const skin_voice_t *v) {
    if (v->n < v->len) return 1;
    float e = 0.0f;
    for (int k = 0; k < SKIN_PARTIALS; k++) e += v->zr[k] * v->zr[k] + v->zi[k] * v->zi[k];
    /* the hit is over and the ring is 100 dB down: done */
    return e * v->norm * v->norm > 1e-10f;
}
