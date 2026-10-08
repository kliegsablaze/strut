/*
 * DICE's rolls (dice.h): each role's sound, rolled inside ranges that stay
 * playable. Each role says which engines it uses, how often, and over what
 * ranges; the levels are set so every roll lands near the same loudness
 * (tests/test_strut.c, dice()).
 */
#include <math.h>
#include <string.h>

#include "dice.h"
#include "levels.h"
#include "mod.h"
#include "samples.h"
#include "strut.h"
#include "tables.h"

/* The kit's layout (DESIGN.md): 1 kick, 2 snare, 3 closed hat, 4 open hat,
 * 5 a second kick, 6 clap, 7 rim, 8 cymbal, 9 to 11 toms, low to high,
 * 12 and 13 percussion, 14 bell, 15 bass, 16 an effect. */
static const dice_role_t ROLES[STRUT_PADS] = {
    ROLE_KICK, ROLE_SNARE, ROLE_HAT, ROLE_OPEN, ROLE_KICK, ROLE_CLAP, ROLE_RIM, ROLE_CYMBAL,
    ROLE_TOM, ROLE_TOM, ROLE_TOM, ROLE_PERC, ROLE_PERC, ROLE_BELL, ROLE_BASS, ROLE_FX,
};

static const char *const NAMES[ROLE_COUNT] = {
    "Kick", "Snare", "Clap", "Hat", "Open", "Cymbal", "Rim", "Tom", "Perc", "Bell", "Bass", "FX",
};

dice_role_t dice_role(int pad) { return ROLES[pad & (STRUT_PADS - 1)]; }
const char *dice_role_name(dice_role_t r) { return NAMES[r]; }

/* ---- chances and ranges ---- */

static float uni(uint32_t *r) {
    *r ^= *r << 13, *r ^= *r >> 17, *r ^= *r << 5;
    return (float)(*r >> 8) * (1.0f / 16777216.0f);
}
static float in(uint32_t *r, float lo, float hi) { return lo + (hi - lo) * uni(r); }
static int one_of(uint32_t *r, int n) { int i = (int)(uni(r) * (float)n); return i < n ? i : n - 1; }
static int chance(uint32_t *r, float p) { return uni(r) < p; }

/* The knob that gives a fall of t seconds (RING, DECAY: 15 ms to 4 s). */
static float secs(float t) { return logf(t / 0.015f) / logf(4.0f / 0.015f); }
/* SNAP's knob for a strike of ms milliseconds (0.2 to 50). */
static float snap(float ms) { return logf(ms / 0.2f) / logf(250.0f); }
/* RATE's free half for a time of t seconds (5 ms to 4 s). */
static float rate(float t) { return -logf(t / 0.005f) / logf(800.0f); }

/* A sample of the library's own kind ("Kick"), as TABLE's option; -1 none.
 * The library names each file for its folder: "Kick 001". */
static int sample(uint32_t *r, const char *kind) {
    const size_t n = strlen(kind);
    int count = 0;
    for (int i = 0; i < smp_count(); i++) {
        const char *s = smp_name(i);
        if (!strncmp(s, kind, n) && s[n] == ' ') count++;
    }
    if (!count) return -1;
    int k = one_of(r, count);
    for (int i = 0; i < smp_count(); i++) {
        const char *s = smp_name(i);
        if (!strncmp(s, kind, n) && s[n] == ' ' && !k--) return NT_TABLES + i;
    }
    return -1;
}

/* The fader the library is measured at (levels.h). */
#define SAMPLE 0.62f

/* Noise plays a sample of `kind` as recorded, its fader set so its loudest
 * 400 ms come out near db, as the role's own rolls do; 0 if the library
 * has none. */
static int noise_sample(float *p, uint32_t *r, const char *kind, float db) {
    const int t = sample(r, kind);
    if (t < 0) return 0;
    p[P_N_TABLE] = (float)t;
    p[P_N_MODE] = SM_SAMPLE;
    p[P_N_DECAY] = 1.0f;        /* to its end, at its own length */
    p[P_N_PITCH] = roundf(in(r, -2.0f, 2.0f));
    /* the fader moves 30 dB end to end; a short, spiky sound is quiet by
     * the 400 ms but loud at its peak, so its peak is kept under -3 dB */
    float pk;
    const float at = levels_of(smp_name(t - NT_TABLES), &pk);
    const float most = SAMPLE + (-3.0f - pk) / 30.0f;
    p[P_NOISE] = fminf(fmaxf(fminf(SAMPLE + (db - at) / 30.0f, most), 0.35f), 0.9f);
    return 1;
}

/* Noise as a table, falling over t seconds. */
static void noise_table(float *p, int table, float t, float color, float g) {
    p[P_N_TABLE] = (float)table;
    p[P_N_DECAY] = secs(t);
    p[P_N_COLOR] = color;
    p[P_NOISE] = g;
}

/* An engine's Envelope on its PITCH: a drop from d up, over t seconds. */
static void pitch_drop(float *p, int kind_at, float d, float t) {
    p[kind_at] = KIND_ENVELOPE;
    p[kind_at + 1] = rate(t);
    p[kind_at + 3] = 0;         /* AIM: Pitch, first in every engine's list */
    p[kind_at + 4] = d;
}

float levels_of(const char *name, float *pk) {
    for (int i = 0; i < LEVELS_N; i++)
        if (!strcmp(LEVELS[i].name, name)) {
            if (pk) *pk = LEVELS[i].pk;
            return LEVELS[i].db;
        }
    if (pk) *pk = -6.0f;
    return LEVELS_MEDIAN;
}

float levels_measure(int t, float *l, float *r, float *pk) {
    static strut_t s;               /* too big for a stack */
    strut_init(&s);
    float *p = s.pad[0].p;
    p[P_SKIN] = 0.0f, p[P_N_TABLE] = (float)t, p[P_N_MODE] = SM_SAMPLE, p[P_N_DECAY] = 1.0f, p[P_NOISE] = SAMPLE;
    strut_render(&s, l, r, 128);    /* the pad says what it wants */
    smp_service(&s.lib, 0);
    strut_note_on(&s, STRUT_NOTE0, 100);
    const int n = 4 * STRUT_SR, w = STRUT_SR * 2 / 5;
    for (int k = 0; k < n; k += 128) strut_render(&s, l + k, r + k, n - k < 128 ? n - k : 128);  /* l and r hold n */
    double best = 0.0, top = 0.0;
    for (int k = 0; k < n; k++) {   /* not fmax: gcc 12 crashes on it (noise.c) */
        if (fabs(l[k]) > top) top = fabs(l[k]);
        if (fabs(r[k]) > top) top = fabs(r[k]);
    }
    *pk = (float)(20.0 * log10(fmax(top, 1e-9)));
    for (int at = 0; at + w <= n; at += STRUT_SR / 20) {
        double a = 0.0;
        for (int k = at; k < at + w; k++) a += 0.25 * ((double)l[k] + r[k]) * ((double)l[k] + r[k]);
        best = fmax(best, a / w);
    }
    smp_stop(&s.lib);
    return (float)(10.0 * log10(fmax(best, 1e-18)));
}

/* ---- the roles ---- */

static void kick(float *p, uint32_t *r) {
    if (chance(r, 0.2f) && noise_sample(p, r, "Kick", -23.0f)) {
        p[P_N_DECAY] = secs(in(r, 0.6f, 1.2f));     /* some of the files ring for seconds */
        p[P_SKIN] = chance(r, 0.5f) ? in(r, 0.5f, 0.65f) : 0.0f;   /* under it, a body */
        p[P_S_PITCH] = roundf(in(r, -5.0f, 2.0f));
        p[P_S_RING] = secs(in(r, 0.2f, 0.5f));
        p[P_S_HIT] = HIT_SOFT;
        p[P_S_MODE] = MODE_LOW;
        return;
    }
    p[P_SKIN] = 0.8f;
    p[P_S_PITCH] = roundf(in(r, -5.0f, 3.0f));
    p[P_S_RING] = secs(in(r, 0.25f, 0.9f));
    p[P_S_HIT] = chance(r, 0.6f) ? HIT_CLICK : HIT_SOFT;
    p[P_S_SNAP] = snap(in(r, 0.5f, 5.0f));
    p[P_S_METAL] = chance(r, 0.3f) ? in(r, 0.0f, 0.2f) : 0.0f;
    p[P_S_TONE] = in(r, 0.3f, 0.7f);
    p[P_S_MODE] = MODE_LOW;
    pitch_drop(p, P_S_KIND, in(r, 0.45f, 0.8f), in(r, 0.03f, 0.15f));
    if (chance(r, 0.3f)) {      /* a tone under it, bent down into place */
        p[P_W_PITCH] = p[P_S_PITCH];
        p[P_W_TABLE] = WT_ANALOG;
        p[P_W_WAVE] = in(r, 0.0f, 0.3f);
        p[P_W_BEND] = in(r, 0.2f, 0.5f);
        p[P_W_DECAY] = secs(in(r, 0.2f, 0.6f));
        p[P_WAVE] = in(r, 0.55f, 0.7f);
    }
    if (chance(r, 0.15f)) noise_table(p, chance(r, 0.5f) ? NT_WHITE : NT_HISS, in(r, 0.02f, 0.06f), in(r, 0.2f, 0.6f), in(r, 0.45f, 0.6f));
    if (chance(r, 0.2f)) p[P_DRIVE] = in(r, 0.1f, 0.4f);
}

static void snare(float *p, uint32_t *r) {
    p[P_SKIN] = in(r, 0.8f, 0.9f);
    p[P_S_PITCH] = roundf(in(r, 16.0f, 26.0f));
    p[P_S_RING] = secs(in(r, 0.12f, 0.3f));
    p[P_S_HIT] = chance(r, 0.5f) ? HIT_BURST : HIT_CLICK;
    p[P_S_SNAP] = snap(in(r, 1.0f, 6.0f));
    p[P_S_METAL] = in(r, 0.0f, 0.3f);
    p[P_S_TONE] = in(r, 0.5f, 0.85f);
    p[P_S_MODE] = chance(r, 0.7f) ? MODE_LOW : MODE_BAND;
    if (chance(r, 0.4f)) pitch_drop(p, P_S_KIND, in(r, 0.2f, 0.4f), in(r, 0.02f, 0.06f));
    if (chance(r, 0.25f) && noise_sample(p, r, "Snare", -26.0f)) return;
    static const int T[] = { NT_WIRES, NT_WHITE, NT_HISS, NT_PINK };
    noise_table(p, T[one_of(r, 4)], in(r, 0.15f, 0.35f), in(r, -0.1f, 0.5f), in(r, 0.85f, 0.92f));
}

static void clap(float *p, uint32_t *r) {
    p[P_SKIN] = 0.0f;
    p[P_FLAM] = in(r, 0.3f, 0.55f);
    if (chance(r, 0.3f) && noise_sample(p, r, "Clap", -31.0f)) {
        p[P_FLAM] = chance(r, 0.5f) ? 0.0f : p[P_FLAM];
        return;
    }
    static const int T[] = { NT_WHITE, NT_PINK, NT_HISS };
    noise_table(p, T[one_of(r, 3)], in(r, 0.12f, 0.3f), in(r, 0.1f, 0.45f), 0.82f);
    if (chance(r, 0.4f)) {      /* a little body */
        p[P_SKIN] = in(r, 0.45f, 0.6f);
        p[P_S_PITCH] = roundf(in(r, 28.0f, 38.0f));
        p[P_S_RING] = secs(in(r, 0.04f, 0.1f));
        p[P_S_HIT] = HIT_BURST;
        p[P_S_MODE] = MODE_BAND;
    }
}

/* Hats, closed or open: t is how long they fall, db how loud a sample is
 * aimed (measured whole, so a hat cut short is aimed higher). */
static void hat(float *p, uint32_t *r, float t0, float t1, float db) {
    p[P_SKIN] = 0.0f;
    if (chance(r, 0.25f) && noise_sample(p, r, "Hat", db)) {
        p[P_N_DECAY] = secs(in(r, t0, t1) * 1.5f);   /* no long tails, whatever the file */
        return;
    }
    static const int T[] = { NT_METAL, NT_HISS, NT_WHITE, NT_WIRES };
    noise_table(p, T[one_of(r, 4)], in(r, t0, t1), in(r, 0.4f, 0.8f), 0.9f);
    if (chance(r, 0.2f)) {      /* a metallic ring through it */
        p[P_W_PITCH] = roundf(in(r, 48.0f, 60.0f));
        p[P_W_TABLE] = chance(r, 0.5f) ? WT_METAL : WT_GLASS;
        p[P_W_WAVE] = in(r, 0.3f, 1.0f);
        p[P_W_DECAY] = secs(in(r, t0, t1));
        p[P_WAVE] = in(r, 0.4f, 0.55f);
    }
}

static void cymbal(float *p, uint32_t *r) {
    p[P_SKIN] = 0.0f;
    if (chance(r, 0.3f) && noise_sample(p, r, "Cymbal", -28.0f)) {
        p[P_N_DECAY] = secs(in(r, 1.0f, 2.5f));
        return;
    }
    static const int T[] = { NT_METAL, NT_HISS, NT_WHITE };
    noise_table(p, T[one_of(r, 3)], in(r, 1.0f, 2.5f), in(r, 0.2f, 0.6f), 0.8f);
    if (chance(r, 0.4f)) {
        p[P_W_PITCH] = roundf(in(r, 40.0f, 55.0f));
        p[P_W_TABLE] = chance(r, 0.5f) ? WT_METAL : WT_GLASS;
        p[P_W_WAVE] = in(r, 0.4f, 1.0f);
        p[P_W_FM] = in(r, 0.0f, 0.3f);
        p[P_W_DECAY] = secs(in(r, 0.8f, 2.0f));
        p[P_WAVE] = in(r, 0.4f, 0.55f);
    }
}

static void rim(float *p, uint32_t *r) {
    if (chance(r, 0.3f) && noise_sample(p, r, "Rim", -28.0f)) {
        p[P_SKIN] = 0.0f;
        return;
    }
    p[P_SKIN] = 1.0f;
    p[P_S_PITCH] = roundf(in(r, 34.0f, 46.0f));
    p[P_S_RING] = secs(in(r, 0.04f, 0.1f));
    p[P_S_HIT] = HIT_CLICK;
    p[P_S_SNAP] = snap(in(r, 0.2f, 1.0f));
    p[P_S_METAL] = in(r, 0.2f, 0.7f);
    p[P_S_TONE] = in(r, 0.6f, 0.95f);
    p[P_S_MODE] = chance(r, 0.5f) ? MODE_BAND : MODE_HIGH;
}

/* Toms by place, low to high: the kit's 9th, 10th and 11th pads. */
static void tom(float *p, uint32_t *r, int pad) {
    const float lo = 2.0f + 6.0f * (float)(pad - 8);
    if (chance(r, 0.2f) && noise_sample(p, r, "Tom", -24.0f)) {
        p[P_SKIN] = 0.0f;
        p[P_N_PITCH] = (float)(4 * (pad - 9)) + roundf(in(r, -1.0f, 1.0f));
        return;
    }
    p[P_SKIN] = 0.8f;
    p[P_S_PITCH] = roundf(in(r, lo, lo + 5.0f));
    p[P_S_RING] = secs(in(r, 0.3f, 0.8f));
    p[P_S_HIT] = chance(r, 0.5f) ? HIT_SOFT : HIT_CLICK;
    p[P_S_SNAP] = snap(in(r, 1.0f, 8.0f));
    p[P_S_METAL] = in(r, 0.0f, 0.2f);
    p[P_S_TONE] = in(r, 0.4f, 0.8f);
    p[P_S_MODE] = MODE_LOW;
    pitch_drop(p, P_S_KIND, in(r, 0.15f, 0.35f), in(r, 0.05f, 0.2f));
}

static void perc(float *p, uint32_t *r) {
    const int k = one_of(r, 3);
    if (k == 0 && noise_sample(p, r, "Percussion", -27.0f)) {   /* shaker, block, conga */
        p[P_SKIN] = 0.0f;
        return;
    }
    if (k <= 1) {               /* a struck object */
        p[P_SKIN] = 0.9f;
        p[P_S_PITCH] = roundf(in(r, 24.0f, 50.0f));
        p[P_S_RING] = secs(in(r, 0.06f, 0.3f));
        /* Click or Burst through Low or Band: Soft through High, high up,
         * peaks over full scale (DESIGN.md, the voicing pass) */
        p[P_S_HIT] = chance(r, 0.5f) ? HIT_CLICK : HIT_BURST;
        p[P_S_SNAP] = snap(in(r, 0.3f, 4.0f));
        p[P_S_METAL] = in(r, 0.0f, 0.8f);
        p[P_S_TONE] = in(r, 0.5f, 0.95f);
        p[P_S_MODE] = chance(r, 0.5f) ? MODE_LOW : MODE_BAND;
    } else {                    /* a short, bent tone */
        p[P_SKIN] = 0.0f;
        p[P_W_PITCH] = roundf(in(r, 24.0f, 48.0f));
        p[P_W_TABLE] = chance(r, 0.5f) ? WT_HOLLOW : WT_VOWEL;
        p[P_W_WAVE] = in(r, 0.0f, 1.0f);
        p[P_W_BEND] = in(r, -0.4f, 0.4f);
        p[P_W_DECAY] = secs(in(r, 0.06f, 0.25f));
        p[P_WAVE] = 0.95f;
    }
}

static void bell(float *p, uint32_t *r) {
    if (chance(r, 0.3f) && noise_sample(p, r, chance(r, 0.5f) ? "Bell" : "Mallet", -24.0f)) {
        p[P_SKIN] = 0.0f;
        p[P_N_DECAY] = secs(in(r, 1.0f, 2.5f));
        return;
    }
    p[P_W_PITCH] = roundf(in(r, 30.0f, 52.0f));
    p[P_W_TABLE] = (int[]){ WT_GLASS, WT_METAL, WT_HOLLOW }[one_of(r, 3)];
    p[P_W_WAVE] = in(r, 0.0f, 0.8f);
    p[P_W_FM] = chance(r, 0.4f) ? in(r, 0.05f, 0.3f) : 0.0f;
    p[P_W_DECAY] = secs(in(r, 0.5f, 1.2f));
    p[P_WAVE] = 0.65f;
    /* struck: the same pitch, ringing as a bar */
    p[P_SKIN] = in(r, 0.45f, 0.6f);
    p[P_S_PITCH] = p[P_W_PITCH];
    p[P_S_RING] = secs(in(r, 0.4f, 1.2f));
    p[P_S_HIT] = HIT_CLICK;
    p[P_S_SNAP] = snap(in(r, 0.3f, 2.0f));
    p[P_S_METAL] = in(r, 0.6f, 1.0f);
    p[P_S_MODE] = MODE_BAND;
}

static void bass(float *p, uint32_t *r) {
    if (chance(r, 0.25f) && noise_sample(p, r, "Bass", -21.0f)) {     /* aimed high: cut short */
        p[P_SKIN] = 0.0f;
        p[P_N_DECAY] = secs(in(r, 0.4f, 1.0f));
        return;
    }
    p[P_SKIN] = 0.0f;
    p[P_W_PITCH] = roundf(in(r, -5.0f, 7.0f));
    p[P_W_TABLE] = (int[]){ WT_ANALOG, WT_HOLLOW, WT_SWEEP, WT_FOLD }[one_of(r, 4)];
    p[P_W_WAVE] = in(r, 0.1f, 0.7f);
    p[P_W_BEND] = chance(r, 0.5f) ? in(r, 0.05f, 0.25f) : 0.0f;
    p[P_W_DECAY] = secs(in(r, 0.3f, 0.9f));
    p[P_WAVE] = 0.8f;
    if (chance(r, 0.4f)) {      /* a sweep down through the table */
        p[P_W_KIND] = KIND_ENVELOPE;
        p[P_W_RATE] = rate(in(r, 0.08f, 0.3f));
        p[P_W_AIM1] = 1;        /* Wave */
        p[P_W_DEPTH1] = in(r, 0.2f, 0.5f);
    }
}

static void fx(float *p, uint32_t *r) {
    p[P_SKIN] = 0.0f;
    static const char *const K[] = { "Glitch", "Foley", "Toy", "Voice" };
    const int k = one_of(r, 3);
    if (k == 0 && noise_sample(p, r, K[one_of(r, 4)], -29.0f)) {
        p[P_N_DECAY] = secs(in(r, 0.3f, 1.2f));
        return;
    }
    if (k <= 1) {               /* a zap: a big bend, sometimes wobbling */
        p[P_W_PITCH] = roundf(in(r, 12.0f, 40.0f));
        p[P_W_TABLE] = (int[]){ WT_SYNC, WT_FOLD, WT_SWEEP, WT_VOWEL }[one_of(r, 4)];
        p[P_W_WAVE] = in(r, 0.0f, 1.0f);
        p[P_W_BEND] = chance(r, 0.7f) ? in(r, 0.5f, 0.9f) : in(r, -0.9f, -0.5f);
        p[P_W_DECAY] = secs(in(r, 0.15f, 0.7f));
        p[P_WAVE] = 0.75f;
        if (chance(r, 0.4f)) {
            p[P_W_KIND] = KIND_LFO;
            p[P_W_RATE] = rate(in(r, 0.03f, 0.15f));
            p[P_W_AIM1] = 0;
            p[P_W_DEPTH1] = in(r, 0.15f, 0.35f);
        }
    } else {                    /* noise, sputtering */
        const int crackle = chance(r, 0.5f);   /* its clicks are tall for their level */
        noise_table(p, crackle ? NT_CRACKLE : NT_GRIT, in(r, 0.2f, 0.8f), in(r, -0.5f, 0.5f), crackle ? 1.0f : 0.85f);
        p[P_N_KIND] = KIND_LFO;
        p[P_N_RATE] = rate(in(r, 0.02f, 0.2f));
        p[P_N_AIM1] = 1;        /* Color */
        p[P_N_DEPTH1] = in(r, -0.6f, 0.6f);
    }
}

/* ---- a roll ---- */

void dice_roll(float *p, int pad, int kit, uint32_t *rng) {
    const float level = p[P_LEVEL], pan = p[P_PAN], choke = p[P_CHOKE], space = p[P_SPACE];
    for (int k = 0; k < P_COUNT; k++)
        if (k != P_DICE) p[k] = STRUT_PAD_PARAMS[k].def;
    p[P_SKIN] = 0.0f;
    switch (dice_role(pad)) {
    case ROLE_KICK: kick(p, rng); break;
    case ROLE_SNARE: snare(p, rng); break;
    case ROLE_CLAP: clap(p, rng); break;
    case ROLE_HAT: hat(p, rng, 0.04f, 0.1f, -30.0f); break;
    case ROLE_OPEN: hat(p, rng, 0.35f, 0.8f, -28.0f); break;
    case ROLE_CYMBAL: cymbal(p, rng); break;
    case ROLE_RIM: rim(p, rng); break;
    case ROLE_TOM: tom(p, rng, pad); break;
    case ROLE_PERC: perc(p, rng); break;
    case ROLE_BELL: bell(p, rng); break;
    case ROLE_BASS: bass(p, rng); break;
    default: fx(p, rng); break;
    }
    if (!kit) {
        p[P_LEVEL] = level, p[P_PAN] = pan, p[P_CHOKE] = choke, p[P_SPACE] = space;
        return;
    }
    /* the kit's mix: hats choke each other, toms spread low to high, the
     * percussion either side */
    const dice_role_t role = dice_role(pad);
    p[P_CHOKE] = role == ROLE_HAT || role == ROLE_OPEN ? 1.0f : 0.0f;   /* group A */
    p[P_PAN] = role == ROLE_TOM ? 0.3f * (float)(pad - 9)
             : role == ROLE_PERC ? (pad == 11 ? -0.25f : 0.25f) : 0.0f;
}
