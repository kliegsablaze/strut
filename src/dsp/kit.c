/*
 * The kit's effects (kit.h). Each is skipped while its knob is at zero, and
 * the room once its tail has died, so a dry kit costs nothing here.
 */
#include <math.h>

#include "kit.h"
#include "strut.h"

#define PI 3.14159265f
/* Dattorro's delays are given at 29761 Hz. */
#define D(n) ((int)((n) * ((float)STRUT_SR / 29761.0f) + 0.5f))

/* A one-pole low-pass's coefficient at hz, trapezoidal (as finish.c's). */
static float pole(float hz) {
    const float g = tanf(PI * fminf(hz, 0.45f * STRUT_SR) / STRUT_SR);
    return g / (1.0f + g);
}

/* ---- GLUE ---- */

/* A compressor for the whole kit, one knob: further round, the threshold
 * falls (-8 to -24 dB) and the ratio rises (1 to 4), over a soft knee. It
 * is slow to grab, 10 ms, so each hit's attack passes and only what follows
 * is held down: the drums keep their snap and their tails come up to meet
 * them. Its level is the larger of two followers: a fast one that lets go
 * in 100 ms, after a single hit, and a slow one that rises over 400 ms and
 * lets go over 1.5 s, which holds the squeeze steady through a busy groove
 * instead of pumping with each hit. Made up by about what it takes at a
 * typical kit's level, so turning it changes the feel more than the
 * loudness. */
static void glue(kit_t *k, float c0, float c1, float *l, float *r, int n) {
    const float att = 1.0f - expf(-1.0f / (0.010f * STRUT_SR));
    const float fr = 1.0f - expf(-1.0f / (0.100f * STRUT_SR));
    const float sa = 1.0f - expf(-1.0f / (0.400f * STRUT_SR));
    const float sr = 1.0f - expf(-1.0f / (1.500f * STRUT_SR));
    const float knee = 6.0f;
    float fast = k->fast, slow = k->slow;
    for (int i = 0; i < n; i++) {
        const float c = c0 + (c1 - c0) * (float)(i + 1) / (float)n;
        const float x = fmaxf(fabsf(l[i]), fabsf(r[i]));
        fast += (x - fast) * (x > fast ? att : fr);
        slow += (x - slow) * (x > slow ? sa : sr);
        const float lv = 6.0206f * log2f(fmaxf(fast, slow) + 1e-9f);
        const float t = -8.0f - 16.0f * c, s = 1.0f - 1.0f / (1.0f + 3.0f * c);
        const float over = lv - t;
        float red = 0.0f;
        if (over >= 0.5f * knee) red = s * over;
        else if (over > -0.5f * knee) red = s * (over + 0.5f * knee) * (over + 0.5f * knee) / (2.0f * knee);
        const float up = 0.5f * s * (-15.0f - t);   /* what it takes at -15 dB, halved */
        const float g = exp2f((up - red) * 0.16610f);
        l[i] *= g, r[i] *= g;
    }
    k->fast = fast, k->slow = slow;
}

/* ---- WARM ---- */

/* DRIVE's cubic (finish.c), and the area under it. */
static inline float curve(float u) {
    return u >= 1.0f ? 1.0f : u <= -1.0f ? -1.0f : u * (1.5f - 0.5f * u * u);
}

static inline float area(float u) {
    const float a = fabsf(u);
    return a >= 1.0f ? a - 0.375f : a * a * (0.75f - 0.125f * a * a);
}

/* Saturation of the whole kit, as tape or a warm desk does it: pushed up to
 * 12 dB into DRIVE's soft curve, off-centre, so it adds even harmonics
 * (which thicken) as well as odd ones (which bite), with the top rolled
 * from 20 kHz down to 8 kHz. Brought back to about the same level at a
 * typical peak. The curve is smoothed the same way as DRIVE's (Parker et
 * al., DAFx 2016), and the offset's own level is taken back out, and any
 * slow drift with it by a high-pass at 10 Hz. */
static void warm(kit_t *k, float w0, float w1, float *l, float *r, int n) {
    const float lg0 = pole(20000.0f * powf(0.4f, w0)), lg1 = pole(20000.0f * powf(0.4f, w1));
    const float dc = 1.0f - 2.0f * PI * 10.0f / STRUT_SR;
    float *x[2] = { l, r };
    for (int ch = 0; ch < 2; ch++) {
        float wu = k->wu[ch], wF = k->wF[ch], lp = k->wlp[ch], dy = k->wdc[ch], dx = k->wdx[ch];
        for (int i = 0; i < n; i++) {
            const float t = (float)(i + 1) / (float)n, w = w0 + (w1 - w0) * t;
            const float g = exp2f(1.9932f * w), b = 0.15f * w, cb = curve(b);
            const float out = 0.15f / (curve(0.15f * g + b) - cb);
            const float u = g * x[ch][i] + b, F = area(u), du = u - wu;
            const float c = fabsf(du) > 1e-4f ? (F - wF) / du : curve(0.5f * (u + wu));
            wu = u, wF = F;
            float y = (c - cb) * out;
            const float hp = y - dx + dc * dy;
            dx = y, dy = hp;
            const float v = (hp - lp) * (lg0 + (lg1 - lg0) * t), lo = v + lp;
            lp = lo + v;
            x[ch][i] = lo;
        }
        k->wu[ch] = wu, k->wF[ch] = wF, k->wlp[ch] = lp, k->wdc[ch] = dy, k->wdx[ch] = dx;
    }
}

/* ---- the room ---- */

static inline float allpass(float *buf, int mask, int w, int delay, float g, float x) {
    const float d = buf[(w - delay) & mask], v = x - g * d;
    buf[w & mask] = v;
    return d + g * v;
}

static inline float allpass_mod(float *buf, int mask, int w, float delay, float g, float x) {
    const int i = (int)delay;
    const float f = delay - (float)i;
    const float d = buf[(w - i) & mask] * (1.0f - f) + buf[(w - i - 1) & mask] * f, v = x - g * d;
    buf[w & mask] = v;
    return d + g * v;
}

static inline float tap(const float *buf, int mask, int w, int k) { return buf[(w - k) & mask]; }

/* SIZE: how long the room rings, falling 60 dB in under a second (a small
 * room) to about four (a hall), evenly by ear; a little darker as it grows
 * (the tank's damping from 9 kHz to 4.5); and longer before the first
 * reflection, 2 to 32 ms. The tank's decay is set for 0.3 to 3 s by its
 * loops alone (about 0.32 / -log10(decay) seconds, measured); its
 * all-passes add their own ring, most at the short end. (Quilt's straight
 * 0.2 to 0.93 rang for ten seconds at the top, too long for drums.) */
static float decay_of(float s) { return exp2f(-1.0697f / (0.3f * exp2f(3.3219f * s))); }
static float damp_of(float s) { return 1.0f - expf(-2.0f * PI * 9000.0f * exp2f(-s) / STRUT_SR); }
static float pre_of(float s) { return (0.002f + 0.030f * s) * STRUT_SR; }

/* Dattorro's plate (JAES 1997), Quilt's: a band-limited input through four
 * diffusing all-passes into two cross-fed loops, each a slowly wandering
 * all-pass, a delay, damping and another all-pass and delay; the two sides
 * tapped at seven points each. SPACE is the send of the whole kit to it,
 * after a high-pass at 150 Hz so the kicks do not boom in it. */
/* send: the pads' sends, each by its SPACE (strut.c, pad_render) */
static int room(kit_t *k, const float *send, float z0, float z1, float *l, float *r, int n) {
    /* a resting tank with nothing coming in is skipped */
    if (k->fb[0] == 0.0f && k->fb[1] == 0.0f && k->fed <= 0) {
        int any = 0;
        for (int i = 0; i < n && !any; i++) any = send[i] != 0.0f;
        if (!any) return 0;
    }
    const float dec0 = decay_of(z0), dec1 = decay_of(z1), kp0 = damp_of(z0), kp1 = damp_of(z1);
    const float pr0 = pre_of(z0), pr1 = pre_of(z1);
    const float hg = pole(150.0f);
    const float exc = 16.0f * ((float)STRUT_SR / 29761.0f);
    const float cr = cosf(2.0f * PI * 0.8f / STRUT_SR), sr = sinf(2.0f * PI * 0.8f / STRUT_SR);
    static const int AP1[2] = { D(672), D(908) }, D1[2] = { D(4453), D(4217) };
    static const int AP2[2] = { D(1800), D(2656) }, D2[2] = { D(3720), D(3163) };
    const int M = KIT_LEN - 1;
    if (k->lfo == 0.0f && k->lfo_s == 0.0f) k->lfo = 1.0f;
    float quiet = 0.0f, fed = 0.0f;
    for (int i = 0; i < n; i++) {
        const int w = k->w++;
        const float t = (float)(i + 1) / (float)n;
        const float decay = dec0 + (dec1 - dec0) * t, keep = kp0 + (kp1 - kp0) * t;
        /* the send, high-passed */
        /* SPACE full up leaves the room about 6 dB under a beat's dry sound */
        const float x0 = 1.75f * send[i];
        const float v = (x0 - k->send_hp) * hg, lo = v + k->send_hp;
        k->send_hp = lo + v;
        k->pre[w & (KIT_PRE - 1)] = x0 - lo;
        fed += fabsf(x0);
        /* the pre-delay, read between samples so a turn of SIZE bends it */
        const float pre = pr0 + (pr1 - pr0) * t;
        const int pi = (int)pre;
        const float pf = pre - (float)pi;
        float x = k->pre[(w - pi) & (KIT_PRE - 1)] * (1.0f - pf) + k->pre[(w - pi - 1) & (KIT_PRE - 1)] * pf;
        k->bw += 0.9995f * (x - k->bw);
        x = allpass(k->ap_in[0], 1023, w, D(142), 0.75f, k->bw);
        x = allpass(k->ap_in[1], 1023, w, D(107), 0.75f, x);
        x = allpass(k->ap_in[2], 1023, w, D(379), 0.625f, x);
        x = allpass(k->ap_in[3], 1023, w, D(277), 0.625f, x);

        const float c = k->lfo * cr - k->lfo_s * sr;
        k->lfo_s = k->lfo_s * cr + k->lfo * sr;
        k->lfo = c;
        const float mod[2] = { exc * k->lfo_s, exc * k->lfo };
        const float in[2] = { x + k->fb[1], x + k->fb[0] };
        for (int s = 0; s < 2; s++) {
            const float a = allpass_mod(k->tank_ap1[s], 2047, w, (float)AP1[s] + exc + mod[s], -0.70f, in[s]);
            k->tank_d1[s][w & M] = a;
            const float b = tap(k->tank_d1[s], M, w, D1[s]);
            k->damp[s] += keep * (b - k->damp[s]);
            const float e = allpass(k->tank_ap2[s], 4095, w, AP2[s], 0.5f, k->damp[s] * decay);
            k->tank_d2[s][w & M] = e;
            k->fb[s] = tap(k->tank_d2[s], M, w, D2[s]) * decay;
        }
        float (*d1)[KIT_LEN] = k->tank_d1, (*d2)[KIT_LEN] = k->tank_d2, (*ap)[4096] = k->tank_ap2;
        const float yl = tap(d1[1], M, w, D(266)) + tap(d1[1], M, w, D(2974)) - tap(ap[1], 4095, w, D(1913)) +
                         tap(d2[1], M, w, D(1996)) - tap(d1[0], M, w, D(1990)) - tap(ap[0], 4095, w, D(187)) -
                         tap(d2[0], M, w, D(1066));
        const float yr = tap(d1[0], M, w, D(353)) + tap(d1[0], M, w, D(3627)) - tap(ap[0], 4095, w, D(1228)) +
                         tap(d2[0], M, w, D(2673)) - tap(d1[1], M, w, D(2111)) - tap(ap[1], 4095, w, D(335)) -
                         tap(d2[1], M, w, D(121));
        l[i] += 0.6f * yl, r[i] += 0.6f * yr;
        quiet += fabsf(k->fb[0]) + fabsf(k->fb[1]);
    }
    /* keep the wander on its circle */
    const float norm = 1.5f - 0.5f * (k->lfo * k->lfo + k->lfo_s * k->lfo_s);
    k->lfo *= norm, k->lfo_s *= norm;
    /* a silent tank is let go, so it can be skipped; what was just sent
     * is still on its way in, through the pre-delay and the diffusers */
    if (fed > 1e-7f * (float)n) k->fed = STRUT_SR / 2;
    else k->fed -= n;
    if (k->fed <= 0 && quiet < 3e-6f * (float)n) {
        k->fb[0] = k->fb[1] = 0.0f;
        return 0;
    }
    return 1;
}

int kit_run(kit_t *k, const float *g, const float *send, float *l, float *r, int n) {
    if (!k->started) {
        k->size = g[G_SIZE], k->glue = g[G_GLUE], k->warm = g[G_WARM];
        k->started = 1;
    }
    if (k->glue > 0.0f || g[G_GLUE] > 0.0f) glue(k, k->glue, g[G_GLUE], l, r, n);
    else k->fast = k->slow = 0.0f;
    if (k->warm > 0.0f || g[G_WARM] > 0.0f) warm(k, k->warm, g[G_WARM], l, r, n);
    else
        for (int ch = 0; ch < 2; ch++) k->wu[ch] = k->wF[ch] = k->wlp[ch] = k->wdc[ch] = k->wdx[ch] = 0.0f;
    const int rings = room(k, send, k->size, g[G_SIZE], l, r, n);
    k->size = g[G_SIZE], k->glue = g[G_GLUE], k->warm = g[G_WARM];
    return rings;
}
