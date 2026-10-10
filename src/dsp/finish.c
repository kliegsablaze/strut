/*
 * The finish (finish.h). Each effect is a few sums a sample, and skipped when
 * at rest: a pad that uses none costs only its PAN.
 */
#include <math.h>

#include "finish.h"
#include "strut.h"

#define PI 3.14159265f

/* COLOR, for the whole pad: a low-pass from 20 kHz down to 200 Hz to the
 * left, a high-pass from 20 Hz up to 4 kHz to the right, nothing at the
 * centre. Narrower than Noise's COLOR: at its ends a pad is dark or thin,
 * not gone. */
static int color(float c, svf_t *f) {
    if (c < 0.0f) { *f = svf(20000.0f * exp2f(6.64f * c), 1.2f); return 1; }
    if (c > 0.0f) { *f = svf(20.0f * exp2f(7.64f * c), 1.2f); return 2; }
    return 0;
}

/* DRIVE's curve: a cubic that rises as a straight line through zero and
 * levels off smoothly at one, and the area under it. (Rejected: tanh, whose
 * area, log cosh, cost an exponential and a logarithm a sample a pad, a
 * third of what all three engines cost.) */
static inline float curve(float u) {
    return u >= 1.0f ? 1.0f : u <= -1.0f ? -1.0f : u * (1.5f - 0.5f * u * u);
}

static inline float area(float u) {
    const float a = fabsf(u);
    return a >= 1.0f ? a - 0.375f : a * a * (0.75f - 0.125f * a * a);
}

/* A one-pole low-pass's coefficient at hz, trapezoidal. */
static float pole(float hz) {
    const float g = tanf(PI * hz / STRUT_SR);
    return g / (1.0f + g);
}

void finish_block(const float *p, finish_block_t *b) {
    b->color = color(p[P_COLOR], &b->cf);

    /* DRIVE: pushed up to 40 dB into a soft curve, and brought back so a
     * loud hit (0.3) stays about as loud at first; further round, its
     * ceiling rises 2.5 dB, to about 2 dB under full scale after the
     * make-up (strut.h), so a hard kick gets louder as well as squarer. A
     * pad pushed to the ceiling ignores its engines' faders: only LEVEL,
     * after the finish, turns it down. (Rejected: the level kept all the way with no
     * make-up, as to 0.11.1: every driven pad was held under -10.7 dB, and
     * the user heard the kicks as too soft, 2026-10-09.) */
    const float d = p[P_DRIVE];
    b->drive = d > 0.0f;
    b->g = powf(10.0f, 2.0f * d);
    b->out = 0.3f * powf(10.0f, 0.125f * d * d) / curve(0.3f * b->g);
    b->w = fminf(1.0f, 20.0f * d);

    /* CRUSH: from 16 bits to 4, and from every sample held to every 16th */
    const float c = p[P_CRUSH];
    b->crush = c > 0.0f;
    b->step = 1.0f / (1.0f + 15.0f * c * c);
    b->q = exp2f(15.0f - 12.0f * c);

    /* LOW below about 200 Hz, HIGH above about 4 kHz, gentle shelves. The
     * filter's corner moves with the gain, by its square root, so a cut is
     * a lift turned upside down; with it fixed, a cut of 18 dB reached
     * only 12 at 60 Hz. */
    const float al = powf(10.0f, p[P_LOW] / 20.0f), ah = powf(10.0f, p[P_HIGH] / 20.0f);
    b->la = al - 1.0f, b->ha = ah - 1.0f;
    b->lg = pole(200.0f / sqrtf(al)), b->hg = pole(fminf(4000.0f * sqrtf(ah), 0.45f * STRUT_SR));
    b->shelf = (p[P_LOW] != 0.0f) | (p[P_HIGH] != 0.0f) << 1;

    /* PAN: as loud anywhere, and as now at the centre */
    const float a = (p[P_PAN] + 1.0f) * (PI / 4);
    b->pl = 1.41421356f * cosf(a), b->pr = 1.41421356f * sinf(a);
}

/* One loop for every effect, a sample at a time. Each of COLOR, LOW and
 * HIGH is a filter whose next sample waits on its last; run one after
 * another, a block each, the processor waited on each in turn (0.3.0: every
 * effect on every pad took 6.8 % of the Move). In one loop it works on all
 * of them at once. The tests of which are on cost nothing to speak of:
 * they come out the same every sample. */
void finish_run(finish_t *f, const finish_block_t *b, float *x, float *l, float *r, int n) {
    finish_t s = *f;
    if (!b->color) s.c1 = s.c2 = 0.0f;
    /* DRIVE glides from the last block's setting to this one's, a sample
     * at a time, and keeps running until it has faded all the way out.
     * Stepped once a block, a turn while a pad rang crackled (the user,
     * 2026-10-09): the push spans 40 dB, so each step of the knob jumped a
     * quiet tail by a fraction of a dB, and switching the curve in or out
     * jumped by its bend and its half-sample delay. */
    if (s.dg == 0.0f) s.dg = b->g, s.dout = b->out, s.dw = b->w;
    const int drive = b->drive || s.dw > 0.0f;
    if (drive && !(s.dw > 0.0f)) s.du = s.dg * s.dy, s.dF = area(s.du);
    const float kn = n > 0 ? 1.0f / (float)n : 0.0f;
    const float dg = (b->g - s.dg) * kn, dout = (b->out - s.dout) * kn, dw = (b->w - s.dw) * kn;
    if (!(b->shelf & 1)) s.lo = 0.0f;
    if (!(b->shelf & 2)) s.hi = 0.0f;
    for (int i = 0; i < n; i++) {
        float y = x[i];
        if (b->color) {
            float lp, bp, hp;
            svf_step(&b->cf, &s.c1, &s.c2, y, &lp, &bp, &hp);
            y = b->color == 1 ? lp : hp;
        }
        if (drive) {
            /* the curve, with its fold-back smoothed: its area between
             * this sample and the last, over the step (Parker, Zavalishin
             * and Le Bivic, DAFx 2016). A plain curve pushed 30 dB folds a
             * high whine down under a kick. */
            s.dg += dg, s.dout += dout, s.dw += dw;
            const float u = s.dg * y, F = area(u), du = u - s.du;
            const float c = fabsf(du) > 1e-4f ? (F - s.dF) / du : curve(0.5f * (u + s.du));
            s.du = u, s.dF = F, s.dy = y;
            y += s.dw * (c * s.dout - y);
        } else s.dy = y;
        if (b->crush) {
            s.ph += b->step;
            if (s.ph >= 1.0f) s.ph -= 1.0f, s.held = y;
            y = rintf(s.held * b->q) / b->q;
        }
        if (b->shelf & 1) {
            const float v = (y - s.lo) * b->lg, lp = v + s.lo;
            s.lo = lp + v;
            y += b->la * lp;
        }
        if (b->shelf & 2) {
            const float v = (y - s.hi) * b->hg, lp = v + s.hi;
            s.hi = lp + v;
            y += b->ha * (y - lp);
        }
        l[i] += b->pl * y, r[i] += b->pr * y;
    }
    /* exactly where this block aimed, whatever the sums lost on the way */
    s.dg = b->g, s.dout = b->out, s.dw = b->w;
    *f = s;
}
