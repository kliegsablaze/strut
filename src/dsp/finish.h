/*
 * A pad's finish (DESIGN.md, Finish): what happens to its three engines once
 * mixed. Pad COLOR, then DRIVE, CRUSH, LOW and HIGH, then PAN into stereo.
 * CHOKE and FLAM are about hits, not sound, and live in strut.c.
 */
#ifndef STRUT_FINISH_H
#define STRUT_FINISH_H

#include "skin.h"

/* What each pad keeps from block to block. */
typedef struct {
    float c1, c2;           /* COLOR's filter */
    float du, dF;           /* DRIVE: the last input, and its curve's area there */
    float dy;               /* the last sample into DRIVE, run or not */
    float dg, dout, dw;     /* DRIVE's push, level and share as the last block left them; dg 0 none yet */
    float held, ph;         /* CRUSH: the sample held, and how far to the next */
    float lo, hi;           /* LOW's and HIGH's one-pole filters */
} finish_t;

/* One block's settings; each effect off is skipped. */
typedef struct {
    int color;              /* 0 none, 1 low-pass, 2 high-pass */
    svf_t cf;
    int drive;
    float g, out;           /* DRIVE's push in, and its level back out */
    float w;                /* how much of the pad goes through DRIVE: it fades in over the knob's first 5% */
    int crush;
    float step, q;          /* CRUSH: samples held, a fraction each; levels a side */
    int shelf;              /* 1 LOW, 2 HIGH, each run only when moved */
    float la, lg, ha, hg;   /* LOW's and HIGH's gain less one, and their filters' */
    float pl, pr;           /* PAN */
} finish_block_t;

void finish_block(const float *p, finish_block_t *b);
/* Runs x (n samples, one pad) through the finish and adds it to l and r. */
void finish_run(finish_t *f, const finish_block_t *b, float *x, float *l, float *r, int n);

#endif
