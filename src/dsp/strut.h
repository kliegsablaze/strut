/*
 * Strut: a drum instrument for Schwung. Sixteen pads, each its own sound,
 * built from three engines (DESIGN.md).
 *
 * Every key the design proposes is declared and kept. What sounds so far is
 * the three engines (build steps 3 to 5), mixed by SKIN, WAVE, NOISE, TUNE,
 * DECAY and LEVEL; each pad's finish (step 6: COLOR and the Finish page);
 * the modulators (step 7); and the Kit page (step 8). Samples, SOUND and
 * DICE follow in DESIGN.md's Build order.
 */
#ifndef STRUT_H
#define STRUT_H

#include <stdint.h>

#include "dice.h"
#include "finish.h"
#include "kit.h"
#include "mod.h"
#include "noise.h"
#include "samples.h"
#include "skin.h"
#include "wave.h"

#define STRUT_VERSION "0.8.0"  /* module.json's; logged on load (tests/run.sh) */
#define STRUT_SR 44100
#define STRUT_PADS 16
#define STRUT_NOTE0 36          /* pad 1 plays C1, as a Move drum track sends */
#define STRUT_MAX_BLOCK 256
#define STRUT_DECLICK 256       /* samples a restarted Wave note fades out over */
#define STRUT_SHAPE 8           /* the noise shaper's memory (strut.c) */
#define STRUT_CHOKE 220         /* samples a choked pad fades out over, 5 ms */
#define STRUT_FLAMS 3           /* hits FLAM makes of one */
#define STRUT_PRESS_WINDOW 0.05f   /* a pad press and its note pair within 50 ms */

typedef enum { PK_FLOAT, PK_INT, PK_ENUM } param_kind_t;

typedef struct {
    const char *key, *cell, *name;
    param_kind_t kind;
    float min, max, def;        /* an enum's are option indices */
    const char *unit;           /* NULL: 0..1 or -1..1 shown as a percentage */
    const char *const *options;
    int noptions;
    int library;                /* TABLE: the sample library follows the options */
} param_def_t;

/* An enum's options, the sample library's included. */
int param_noptions(const param_def_t *d);
const char *param_option(const param_def_t *d, int i);

/* One drum's knobs. Each is declared ONCE, by its bare key; the host
 * multiplies it into p01_<key> ... p16_<key> through the rack's template
 * (DESIGN.md, Pads and focus). Grouped by page, in cell order. */
typedef enum {
    /* Pad */
    P_SOUND, P_TUNE, P_DECAY, P_COLOR, P_SKIN, P_WAVE, P_NOISE, P_LEVEL,
    /* Skin, sound view, then mod view */
    P_S_PITCH, P_S_RING, P_S_HIT, P_S_SNAP, P_S_METAL, P_S_TONE, P_S_MODE,
    P_S_KIND, P_S_RATE, P_S_CURVE, P_S_AIM1, P_S_DEPTH1, P_S_AIM2, P_S_DEPTH2,
    /* Wave */
    P_W_PITCH, P_W_BEND, P_W_DECAY, P_W_TABLE, P_W_WAVE, P_W_FM, P_W_RING,
    P_W_KIND, P_W_RATE, P_W_CURVE, P_W_AIM1, P_W_DEPTH1, P_W_AIM2, P_W_DEPTH2,
    /* Noise */
    P_N_PITCH, P_N_MODE, P_N_DECAY, P_N_TABLE, P_N_COLOR, P_N_START, P_N_LOOP,
    P_N_KIND, P_N_RATE, P_N_CURVE, P_N_AIM1, P_N_DEPTH1, P_N_AIM2, P_N_DEPTH2,
    /* Finish */
    P_PAN, P_CHOKE, P_FLAM, P_DRIVE, P_CRUSH, P_LOW, P_HIGH, P_DICE,
    P_COUNT
} pad_param_t;

/* The kit's own knobs, and the three MOD switches. The switches are UI
 * state: one per engine page, shared by every pad, never saved. */
typedef enum {
    G_SKIN_VIEW, G_WAVE_VIEW, G_NOISE_VIEW,
    G_SPACE, G_SIZE, G_GLUE, G_WARM, G_VOL, G_DICE,
    G_COUNT
} global_param_t;

extern const param_def_t STRUT_PAD_PARAMS[P_COUNT];
extern const param_def_t STRUT_GLOBALS[G_COUNT];

/* A page of the contract: its level, its title, and its cells. An engine
 * page lists its sound view's seven, its mod view's seven, then MOD. */
typedef struct {
    const char *level, *label;
    int per_pad;                /* a rack level, keyed through the template */
    int view;                   /* the MOD switch (G_*_VIEW), or -1 */
    int first, count;           /* a run of STRUT_PAD_PARAMS or STRUT_GLOBALS */
} page_def_t;

#define STRUT_NPAGES 6
extern const page_def_t STRUT_PAGES[STRUT_NPAGES];
extern const char *const STRUT_VIEW_OPTIONS[2];   /* "Sound", "Mod" */
extern const char *const STRUT_DICE_OPTIONS[2];   /* "Back", "Roll" */

/* A pad's drum: the three engines together, a sample at a time. One a pad:
 * a new hit strikes the same Skin again, restarts Wave and adds to Noise
 * (DESIGN.md, One voice a pad). */
typedef struct {
    int active;
    float gs, gw, gn;           /* the last block's levels, so a turn glides; -1 none yet */
    skin_voice_t skin;
    wave_voice_t wave;
    noise_voice_t noise;
    noise_bank_t bank;          /* Noise's Resynth, kept out of the sample loop's copy of the voice */
    wave_voice_t old;           /* the Wave note a hit cut, fading out */
    int old_n;                  /* samples of that fade left */
    finish_t fx;
    mod_t mod;
    int choke_n;                /* choked: samples of its fade left; 0 not choked */
} voice_t;

typedef struct {
    float p[P_COUNT];
    voice_t voice;
    int flams;                  /* FLAM's hits still to come */
    int flam_in;                /* samples to the next */
    float flam_amp;             /* the hit's strength, as played */
    dice_hist_t dice;           /* Finish > DICE's rolls of this pad */
    float rolls[DICE_SLOTS][P_COUNT];
} pad_t;

typedef struct {
    pad_t pad[STRUT_PADS];
    float g[G_COUNT];
    int focus;                  /* 0..15, the pad the pages edit */
    unsigned focus_count;       /* bumped on every focus move, see DESIGN.md */
    double now;                 /* seconds rendered */
    double press_at, note_at;   /* last vouch from the host, last note-on */
    int note_pad;
    uint32_t seed;              /* the noise behind every hit */
    uint32_t dither;            /* the output's dither, and how deep it is */
    float dither_g, vol_g;
    float shape[2][STRUT_SHAPE];  /* each channel's last rounding errors */
    float bpm;                  /* the host's tempo, for RATE's synced half */
    int sounding;               /* voices that rendered this block, and the room */
    kit_t kit;
    smp_lib_t lib;              /* the samples the pads name, loaded off the audio thread */
    dice_hist_t dice;           /* Kit > DICE's rolls of every pad */
    float rolls[DICE_SLOTS][STRUT_PADS][P_COUNT];
    uint32_t dice_rng;
} strut_t;

void strut_init(strut_t *s);
void strut_note_on(strut_t *s, int note, int vel);
void strut_render(strut_t *s, float *l, float *r, int frames);
/* The Kit page's effects on the mixed pads (kit.h), before strut_output. */
void strut_kit(strut_t *s, float *l, float *r, int frames);
void strut_output(strut_t *s, const float *l, const float *r, int16_t *out, int frames);
float strut_fader(float x);
void strut_press(strut_t *s);   /* the host's "a finger did that" */
/* A turn of Finish > DICE on pad (0..15), or of Kit > DICE with pad -1:
 * DICE_ROLL or DICE_BACK (dice.h). */
void strut_dice(strut_t *s, int pad, int way);

int strut_contract_hierarchy(char *buf, int len);
int strut_contract_params(char *buf, int len);

#endif
