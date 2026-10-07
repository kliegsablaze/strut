/*
 * Strut: a drum instrument for Schwung. Sixteen pads, each its own sound,
 * built from three engines (DESIGN.md).
 *
 * SCAFFOLD. Each pad is a placeholder: one pitched, decaying sine with a
 * pitch drop, so the module builds, loads, follows the pads and makes a
 * sound. The engines, their modulation and the pages replace it, in the
 * order DESIGN.md's Build order gives.
 */
#ifndef STRUT_H
#define STRUT_H

#include <stdint.h>

#define STRUT_SR 44100
#define STRUT_PADS 16
#define STRUT_NOTE0 36          /* pad 1 plays C1, as a Move drum track sends */
#define STRUT_MAX_BLOCK 256
#define STRUT_PRESS_WINDOW 0.05f   /* a pad press and its note pair within 50 ms */

typedef enum { P_TUNE, P_DECAY, P_LEVEL, P_COUNT } pad_param_t;

typedef struct {
    const char *key, *cell, *name;
    float min, max, def;
} param_def_t;

extern const param_def_t STRUT_PAD_PARAMS[P_COUNT];

typedef struct {
    float p[P_COUNT];
    /* the placeholder voice */
    int active;
    double phase;
    float env, drop, vel;
} pad_t;

typedef struct {
    pad_t pad[STRUT_PADS];
    int focus;                  /* 0..15, the pad the pages edit */
    unsigned focus_count;       /* bumped on every focus move, see DESIGN.md */
    double now;                 /* seconds rendered */
    double press_at, note_at;   /* last vouch from the host, last note-on */
    int note_pad;
} strut_t;

void strut_init(strut_t *s);
void strut_note_on(strut_t *s, int note, int vel);
void strut_render(strut_t *s, float *l, float *r, int frames);
void strut_press(strut_t *s);   /* the host's "a finger did that" */

int strut_contract_hierarchy(char *buf, int len);
int strut_contract_params(char *buf, int len);

#endif
