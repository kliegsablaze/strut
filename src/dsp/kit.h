/*
 * The kit's effects (DESIGN.md, How the Kit page works): the sixteen pads
 * mixed, then GLUE, WARM, and the room (SPACE and SIZE) after them. VOL, the
 * limiter and the dither follow in strut.c.
 */
#ifndef STRUT_KIT_H
#define STRUT_KIT_H

#define KIT_LEN 8192            /* the room's longest delays, a power of two */
#define KIT_PRE 2048            /* its pre-delay, up to 46 ms */

typedef struct {
    /* the knobs as last block left them, so a turn glides */
    float size, glue, warm;
    int started;
    /* GLUE: a fast and a slow follower of the kit's level */
    float fast, slow;
    /* WARM, each side: its curve's last input and area, its low-pass, and
     * the offset taken back out */
    float wu[2], wF[2], wlp[2], wdc[2], wdx[2];
    /* the room: Dattorro's plate, as Quilt's (src/dsp/fx.c) */
    float send_hp, bw;
    float pre[KIT_PRE];
    float ap_in[4][1024];
    float tank_ap1[2][2048], tank_d1[2][KIT_LEN];
    float tank_ap2[2][4096], tank_d2[2][KIT_LEN];
    float damp[2], fb[2], lfo, lfo_s;
    int w;
    int fed;                /* samples the room still listens after its last input */
} kit_t;

/* g is the kit's knobs (STRUT_GLOBALS). Runs l and r, n samples, through
 * GLUE, WARM and the room, in place. Returns 1 while the room still rings. */
int kit_run(kit_t *k, const float *g, const float *send, float *l, float *r, int n);

#endif
