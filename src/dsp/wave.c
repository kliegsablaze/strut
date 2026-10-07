/*
 * Wave (wave.h). The oscillator reads two frames of a table and mixes them by
 * WAVE's position between them. It reads the brightest copy whose top
 * harmonic stays under 45% of the sample rate at the highest frequency the
 * block can reach, FM and the ring's sidebands included, so nothing folds
 * back from above the audio band. When a sweep crosses into another copy,
 * that block fades from the old to the new, so the change is not heard as a
 * step. (Rejected: always mixing two copies by the octave's fraction. It
 * read the tables twice as often, and to stay clean had to cut the top
 * harmonic to a quarter of the rate, an octave of brightness lost.)
 */
#include <math.h>

#include "strut.h"
#include "wave.h"

_Static_assert(STRUT_MAX_BLOCK <= WAVE_SUB * WAVE_SUB, "a block's pitch steps fit wave_block_t");

#define TOP (0.45f * STRUT_SR)

/* BEND: up to four octaves, finer near the centre; right starts high and
 * falls, left starts low and rises. */
static float bend_start(const float *p) {
    const float b = p[P_W_BEND];
    return 48.0f * b * fabsf(b);
}

float wave_hz(const float *p) {
    const float hz = 55.0f * powf(2.0f, (p[P_W_PITCH] + p[P_TUNE]) / 12.0f);
    return fminf(fmaxf(hz, 1.0f), TOP);
}

/* DECAY from 15 ms to 4 s, as Skin's RING; the Pad page's DECAY scales it.
 * Never under two and a half cycles, as Skin: less is a tick, not a note. */
float wave_t60(const float *p) {
    const float t = 0.015f * powf(4.0f / 0.015f, p[P_W_DECAY]) * powf(4.0f, p[P_DECAY]);
    return fminf(fmaxf(t, 2.5f / wave_hz(p)), 12.0f);
}

/* The sweep falls back to the pitch with a twentieth of the note's length:
 * a quick drop on a short kick (25 ms on a half-second one), a slow dive on
 * a long zap. An eighth left a long note audibly sharp half a second in. */
static float bend_step(const float *p, int samples) {
    const float tau = fminf(fmaxf(wave_t60(p) / 20.0f, 0.003f), 0.3f);
    return expf(-(float)samples / (tau * STRUT_SR));
}

static float hz_at(const float *p, float bend) {
    const float hz = 55.0f * powf(2.0f, (p[P_W_PITCH] + p[P_TUNE] + bend) / 12.0f);
    return fminf(fmaxf(hz, 1.0f), TOP);
}

void wave_start(wave_voice_t *w, const float *p) {
    *w = (wave_voice_t){ 0 };
    w->env = 1.0f;
    w->level = -1;
    w->bend = bend_start(p);
    w->rphase = 0.25f;      /* the ring's sine at its peak, so a slow one starts open */
}

/* FM: a squared knob, up to a swing of four times Wave's own frequency. */
static float fm_index(const float *p) { return 4.0f * p[P_W_FM] * p[P_W_FM]; }

/* RING: off at the centre, fading in over the first tenth of a turn; then a
 * sine from two octaves below Wave's pitch to two above. */
static void ring(const float *p, float *ratio, float *mix) {
    const float r = p[P_W_RING];
    *ratio = powf(2.0f, 2.0f * r);
    *mix = fminf(fabsf(r) * 10.0f, 1.0f);
}

/* WAVE on Analog: sine to triangle to saw to square, then the pulse
 * narrows. Square and pulse are a saw less a shifted saw, kept at the
 * saw's loudness (a saw's correlation with itself shifted by w is
 * 1 - 6w(1-w)). */
typedef struct { int a, b; float ca, cb, off; } mix_t;

static mix_t analog(float x) {
    if (x < 0.25f) return (mix_t){ WT_A_SINE, WT_A_TRI, 1 - x * 4, x * 4, 0 };
    if (x < 0.5f) return (mix_t){ WT_A_TRI, WT_A_SAW, 1 - (x - 0.25f) * 4, (x - 0.25f) * 4, 0 };
    const float k = x < 0.75f ? (x - 0.5f) * 4 : 1.0f;
    const float pw = x < 0.75f ? 0.5f : 0.5f - 0.45f * (x - 0.75f) * 4;
    const float rho = 1 - 6 * pw * (1 - pw);
    const float g = 1.0f / sqrtf(1 + k * k - 2 * k * rho);
    return (mix_t){ WT_A_SAW, WT_A_SAW, g, -k * g, pw };
}

static mix_t frames_at(const float *p) {
    const float x = fminf(fmaxf(p[P_W_WAVE], 0.0f), 1.0f);
    const int t = (int)p[P_W_TABLE];
    if (t == WT_ANALOG) return analog(x);
    const float f = x * (WT_FRAMES - 1);
    const int i = f >= WT_FRAMES - 1 ? WT_FRAMES - 2 : (int)f;
    return (mix_t){ i, i + 1, 1 - (f - i), f - i, 0 };
}

int wave_block(wave_voice_t *w, const float *p, int frames, wave_block_t *b) {
    b->sub = (frames + WAVE_SUB - 1) / WAVE_SUB;
    const float step = bend_step(p, WAVE_SUB);
    float hi = 0.0f;
    for (int k = 0; k <= b->sub; k++) {
        const float hz = hz_at(p, w->bend);
        hi = fmaxf(hi, hz);
        b->inc[k] = hz / STRUT_SR;
        if (k < b->sub) w->bend *= step;
    }
    b->fm = fm_index(p);
    ring(p, &b->ratio, &b->rmix);

    /* the brightness: the top harmonic under 45% of the rate, less the
     * ring's shift */
    const float shift = b->rmix > 0.0f ? hi * b->ratio : 0.0f;
    const float band = fmaxf(0.45f * STRUT_SR - shift, 0.02f * STRUT_SR);
    const float c = log2f(WT_H * hi * (1.0f + b->fm) / band);
    int level = c <= 0.0f ? 0 : (int)ceilf(c);
    if (level > WT_LEVELS - 1) level = WT_LEVELS - 1;
    b->cross = w->level >= 0 && w->level != level;
    b->dx = 1.0f / (float)frames;
    const int lv[2] = { level, b->cross ? w->level : level };
    w->level = level;

    const mix_t m = frames_at(p);
    const int t = (int)p[P_W_TABLE];
    const wt_frame_t *fa = wt_frame(t, m.a), *fb = wt_frame(t, m.b);
    for (int l = 0; l < 2; l++) {
        b->ta[l] = fa->level[lv[l]];
        b->tb[l] = fb->level[lv[l]];
        b->size[l] = WT_N(lv[l]);
    }
    b->ca = m.ca, b->cb = m.cb, b->off = m.off;

    w->decay = expf(-6.9078f / (wave_t60(p) * STRUT_SR));
    return w->env > 1e-5f;      /* 100 dB down */
}

void wave_skip(wave_voice_t *w, const float *p, int frames) {
    w->phase += hz_at(p, w->bend) / STRUT_SR * (float)frames;
    w->phase -= floorf(w->phase);
    w->bend *= bend_step(p, frames);
    w->level = -1;          /* rejoining starts on the right copy, no fade */
    w->decay = expf(-6.9078f / (wave_t60(p) * STRUT_SR));
    w->env *= powf(w->decay, (float)frames);
    w->n += frames;
}

int wave_strike_len(const float *p, int len) {
    const int cycle = (int)(STRUT_SR / hz_at(p, bend_start(p))) + 1;
    const int most = STRUT_SR / 20;     /* SNAP's longest, 50 ms */
    return len >= cycle ? len : cycle < most ? cycle : most;
}

/* A unit complex number, turned by another: the angles below step evenly
 * from one harmonic to the next, so each is the last one turned, not a
 * sine and cosine of its own. */
typedef struct { double c, s; } turn_t;

static turn_t turn_of(double a) { return (turn_t){ cos(a), sin(a) }; }
static turn_t turned(turn_t x, turn_t by) {
    return (turn_t){ x.c * by.c - x.s * by.s, x.s * by.c + x.c * by.s };
}

/* The strike's spectrum, sum d^n e^(-j dw n) over len samples, given
 * e^(-j dw) and e^(-j dw len). */
static void strike_at(turn_t one, turn_t all, double d, double dl, double *er, double *ei) {
    const double nr = 1 - dl * all.c, ni = -dl * all.s;
    const double dr = 1 - d * one.c, di = -d * one.s;
    const double m = fmax(dr * dr + di * di, 1e-24);
    *er = (nr * dr + ni * di) / m;
    *ei = (ni * dr - nr * di) / m;
}

/* Wave's opening, as it starts (BEND's first pitch, without FM or the
 * ring, which follow Skin), through the strike, at hz: each harmonic near
 * hz, with its phase, so a wave that starts at a zero strikes as weakly as
 * it really does. */
float wave_strike(const float *p, float hz, float d, int len) {
    const double f0 = hz_at(p, bend_start(p)), th = 2 * 3.14159265358979 * f0 / STRUT_SR;
    const double w = 2 * 3.14159265358979 * hz / STRUT_SR, dl = pow(d, len);
    const mix_t m = frames_at(p);
    const int t = (int)p[P_W_TABLE];
    const wt_frame_t *fa = wt_frame(t, m.a), *fb = wt_frame(t, m.b);
    const int c = (int)(hz / f0), h0 = c - 24 < 1 ? 1 : c - 24;
    /* at h0, then turned by one harmonic each step */
    const turn_t up = turn_of(th), upl = turn_of(th * len), shift = turn_of(2 * 3.14159265358979 * m.off);
    turn_t lo = turn_of(-(w - h0 * th)), lol = turn_of(-(w - h0 * th) * len);   /* e^(-j(w - h th)) */
    turn_t hi = turn_of(-(w + h0 * th)), hil = turn_of(-(w + h0 * th) * len);   /* e^(-j(w + h th)) */
    turn_t sh = turn_of(2 * 3.14159265358979 * h0 * m.off);
    const turn_t down = { up.c, -up.s }, downl = { upl.c, -upl.s };
    double xr = 0, xi = 0;
    for (int h = h0; h <= c + 25 && h <= WT_H && h * f0 < 0.5 * STRUT_SR; h++) {
        /* the harmonic as e^(j h th n) times (cr + j ci), and its mirror */
        const double br = 0.5 * fb->ca[h], bi = -0.5 * fb->sb[h];
        const double cr = m.ca * 0.5 * fa->ca[h] + m.cb * (br * sh.c - bi * sh.s);
        const double ci = -m.ca * 0.5 * fa->sb[h] + m.cb * (br * sh.s + bi * sh.c);
        double er, ei;
        strike_at(lo, lol, d, dl, &er, &ei);
        xr += cr * er - ci * ei, xi += cr * ei + ci * er;
        strike_at(hi, hil, d, dl, &er, &ei);
        xr += cr * er + ci * ei, xi += cr * ei - ci * er;
        lo = turned(lo, up), lol = turned(lol, upl);
        hi = turned(hi, down), hil = turned(hil, downl);
        sh = turned(sh, shift);
    }
    return (float)sqrt(xr * xr + xi * xi);
}
