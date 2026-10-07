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

    /* DRIVE: pushed up to 30 dB into a soft curve, and brought back so a
     * loud hit stays about as loud while its quiet parts come up */
    const float d = p[P_DRIVE];
    b->drive = d > 0.0f;
    b->g = powf(10.0f, 1.5f * d);
    b->out = 0.3f / curve(0.3f * b->g);

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

void finish_run(finish_t *f, const finish_block_t *b, float *x, float *l, float *r, int n) {
    if (b->color) {
        float lp, bp, hp;
        for (int i = 0; i < n; i++) {
            svf_step(&b->cf, &f->c1, &f->c2, x[i], &lp, &bp, &hp);
            x[i] = b->color == 1 ? lp : hp;
        }
    } else {
        f->c1 = f->c2 = 0.0f;
    }
    if (b->drive) {
        /* the curve, with its fold-back smoothed: its area between this
         * sample and the last, over the step (Parker, Zavalishin and Le
         * Bivic, DAFx 2016). A plain curve pushed 30 dB folds a high whine
         * down under a kick. */
        for (int i = 0; i < n; i++) {
            const float u = b->g * x[i], F = area(u), du = u - f->du;
            const float y = fabsf(du) > 1e-4f ? (F - f->dF) / du : curve(0.5f * (u + f->du));
            f->du = u, f->dF = F;
            x[i] = y * b->out;
        }
    } else {
        f->du = f->dF = 0.0f;
    }
    if (b->crush) {
        for (int i = 0; i < n; i++) {
            f->ph += b->step;
            if (f->ph >= 1.0f) f->ph -= 1.0f, f->held = x[i];
            x[i] = rintf(f->held * b->q) / b->q;
        }
    }
    if (b->shelf & 1) {
        for (int i = 0; i < n; i++) {
            const float v = (x[i] - f->lo) * b->lg, lp = v + f->lo;
            f->lo = lp + v;
            x[i] += b->la * lp;
        }
    } else {
        f->lo = 0.0f;
    }
    if (b->shelf & 2) {
        for (int i = 0; i < n; i++) {
            const float v = (x[i] - f->hi) * b->hg, lp = v + f->hi;
            f->hi = lp + v;
            x[i] += b->ha * (x[i] - lp);
        }
    } else {
        f->hi = 0.0f;
    }
    for (int i = 0; i < n; i++) l[i] += b->pl * x[i], r[i] += b->pr * x[i];
}
