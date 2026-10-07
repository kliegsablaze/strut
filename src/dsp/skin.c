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
 */
#include <math.h>

#include "skin.h"
#include "strut.h"

#define TWO_PI 6.2831853f
#define NYQ_SAFE (0.45f * STRUT_SR)

enum { HIT_CLICK, HIT_SOFT, HIT_BURST, HIT_WAVE, HIT_NOISE };
enum { MODE_LOW, MODE_BAND, MODE_HIGH };

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

static uint32_t rng(uint32_t *s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

/* The hit's next sample. Wave and Noise strike Skin with those engines once
 * they exist (build steps 4 and 5); until then they strike with a burst. */
static float hit_next(skin_voice_t *v) {
    if (v->n >= v->len) return 0.0f;
    float x;
    switch (v->kind) {
    case HIT_CLICK:
        x = v->env;
        v->env *= v->decay;
        break;
    case HIT_SOFT:
        x = 0.5f - 0.5f * cosf(TWO_PI * (float)v->n / (float)v->len);
        break;
    default:
        x = ((float)(int32_t)rng(&v->seed) * (1.0f / 2147483648.0f)) * v->env;
        v->env *= v->decay;
        break;
    }
    v->n++;
    return x;
}

void skin_start(skin_voice_t *v, const float *p, uint32_t seed) {
    *v = (skin_voice_t){ 0 };
    v->kind = (int)p[P_S_HIT];
    v->seed = seed | 1u;
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
         * variance is a third). Each burst then rings a little differently. */
        const float d2 = v->decay * v->decay;
        mag = sqrtf((1.0f - powf(d2, (float)v->len)) / (1.0f - d2) / 3.0f);
        sum = 0.5f * (1.0f - powf(v->decay, (float)v->len)) / (1.0f - v->decay);
    }
    v->norm = 1.0f / fmaxf(fmaxf(mag, 0.01f * sum), 1e-6f);
}

/* One step of a trapezoidal state-variable filter, all three outputs. */
typedef struct { float a1, a2, a3, k; } svf_t;

static svf_t svf(float hz, float k) {
    const float g = tanf(3.14159265f * fminf(fmaxf(hz, 10.0f), NYQ_SAFE) / STRUT_SR);
    svf_t f = { 0, 0, 0, k };
    f.a1 = 1.0f / (1.0f + g * (g + k));
    f.a2 = g * f.a1;
    f.a3 = g * f.a2;
    return f;
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

int skin_render(skin_voice_t *v, const float *p, float gain, float *out, int frames) {
    const float hz = skin_hz(p), t60 = skin_t60(p), metal = p[P_S_METAL];
    float pr[SKIN_PARTIALS], pi[SKIN_PARTIALS], amp[SKIN_PARTIALS];
    for (int k = 0; k < SKIN_PARTIALS; k++) {
        const float ratio = k ? DRUM_RATIO[k - 1] + metal * (BAR_RATIO[k - 1] - DRUM_RATIO[k - 1]) : 1.0f;
        const float f = hz * ratio;
        /* the partials die sooner, less so as they turn to metal */
        const float t = k ? t60 * (0.35f + 0.65f * metal) : t60;
        const float r = expf(-6.9078f / (t * STRUT_SR));
        const float w = TWO_PI * f / STRUT_SR;
        pr[k] = r * cosf(w);
        pi[k] = r * sinf(w);
        amp[k] = k ? (f < NYQ_SAFE ? 0.7f * metal : 0.0f) : 1.0f;
    }
    const int mode = (int)p[P_S_MODE];
    const svf_t mf = mode == MODE_LOW ? svf(2.0f * hz, 1.4142f)
                   : mode == MODE_HIGH ? svf(0.5f * hz, 1.4142f) : svf(hz, 1.0f);
    const svf_t tf = svf(150.0f * powf(120.0f, p[P_S_TONE]), 1.4142f);
    const float g = gain * v->norm;

    for (int n = 0; n < frames; n++) {
        const float x = hit_next(v);
        float y = 0.0f;
        for (int k = 0; k < SKIN_PARTIALS; k++) {
            const float zr = v->zr[k] * pr[k] - v->zi[k] * pi[k] + x;
            v->zi[k] = v->zr[k] * pi[k] + v->zi[k] * pr[k];
            v->zr[k] = zr;
            y += amp[k] * v->zi[k];
        }
        y = 0.7f * y + 0.5f * x;    /* the ring, and the hit itself */
        float lp, bp, hp;
        svf_step(&mf, &v->m1, &v->m2, y, &lp, &bp, &hp);
        y = mode == MODE_LOW ? lp : mode == MODE_HIGH ? hp : mf.k * bp;
        svf_step(&tf, &v->t1, &v->t2, y, &lp, &bp, &hp);
        out[n] += g * lp;
    }

    if (v->n < v->len) return 1;
    float e = 0.0f;
    for (int k = 0; k < SKIN_PARTIALS; k++) e += v->zr[k] * v->zr[k] + v->zi[k] * v->zi[k];
    /* the hit is over and the ring is 100 dB down: done */
    return e * v->norm * v->norm > 1e-10f;
}
