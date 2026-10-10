/*
 * Every knob Strut proposes (DESIGN.md, Control surface), and the pages they
 * sit on. Ranges and option lists are provisional until each engine's build
 * step: the probe needs their shape and size, not their sound.
 */
#include <stddef.h>

#include "strut.h"
#include "dice.h"
#include "noise.h"
#include "samples.h"
#include "tables.h"

#define N(a) (int)(sizeof(a) / sizeof((a)[0]))
#define ENUM(key, cell, name, opts) { key, cell, name, PK_ENUM, 0, N(opts) - 1, 0, NULL, opts, N(opts), 0 }
#define UNI(key, cell, name, def) { key, cell, name, PK_FLOAT, 0.0f, 1.0f, def, NULL, NULL, 0, 0 }
#define BI(key, cell, name) { key, cell, name, PK_FLOAT, -1.0f, 1.0f, 0.0f, NULL, NULL, 0, 0 }
#define SEMI(key, cell, name, lo, hi) { key, cell, name, PK_INT, lo, hi, 0, "st", NULL, 0, 0 }

static const char *const HITS[] = { "Click", "Soft", "Burst", "Wave", "Noise" };
static const char *const MODES[] = { "Low", "Band", "High" };
static const char *const KINDS[] = { "Envelope", "LFO", "Random", "Velocity" };
static const char *const SKIN_CURVES[] = { "Natural", "Ping", "Soft", "Hold" };
static const char *const CURVES[] = { "Natural", "Ping", "Soft", "Hold", "Swell" };
static const char *const SKIN_AIMS[] = { "Pitch", "Ring", "Snap", "Metal", "Tone", "Level" };
static const char *const WAVE_AIMS[] = { "Pitch", "Wave", "FM", "Ring", "Level" };
static const char *const NOISE_AIMS[] = { "Pitch", "Color", "Start", "Loop", "Level" };
/* Wave's tables, in tables.h's order (DESIGN.md, How Wave works). */
static const char *const WAVE_TABLES[] = { "Analog", "Sync", "Fold", "Sweep", "Vowel", "Hollow", "Metal", "Glass" };
_Static_assert(N(WAVE_TABLES) == WT_TABLES, "one TABLE option for each table");
/* Noise's tables, in noise.h's order (DESIGN.md, How Noise works). */
static const char *const NOISE_TABLES[] = { "White", "Pink", "Brown", "Hiss", "Wires", "Metal", "Crackle", "Grit" };
_Static_assert(N(NOISE_TABLES) == NT_TABLES, "one TABLE option for each noise table");
/* How Noise plays one of your samples (DESIGN.md, Noise is also a sampler). */
static const char *const SAMPLE_MODES[] = { "Sample", "Resynth", "Noise" };
static const char *const CHOKES[] = { "Off", "A", "B", "C", "D" };
const char *const STRUT_VIEW_OPTIONS[2] = { "Sound", "Mod" };
const char *const STRUT_DICE_OPTIONS[2] = { "Back", "Roll" };   /* dice.h's order */

int param_noptions(const param_def_t *d) { return d->noptions + (d->library ? smp_count() : 0); }

const char *param_option(const param_def_t *d, int i) {
    return i < d->noptions ? d->options[i] : smp_name(i - d->noptions);
}

const param_def_t STRUT_PAD_PARAMS[P_COUNT] = {
    [P_SOUND] = ENUM("sound", "Sound", "Sound", DICE_SOUND_NAMES),
    [P_TUNE] = SEMI("tune", "Tune", "Tune", -24, 24),
    [P_DECAY] = BI("decay", "Decay", "Decay"),
    [P_COLOR] = BI("color", "Color", "Color"),
    [P_SKIN] = UNI("skin", "Skin", "Skin Level", 0.8f),
    [P_WAVE] = UNI("wave", "Wave", "Wave Level", 0.0f),
    [P_NOISE] = UNI("noise", "Noise", "Noise Level", 0.0f),
    [P_SPACE] = UNI("space", "Space", "Room Send", 0.0f),

    [P_S_PITCH] = SEMI("s_pitch", "Pitch", "Skin Pitch", -12, 60),
    [P_S_RING] = UNI("s_ring", "Ring", "Skin Ring", 0.5f),
    [P_S_HIT] = ENUM("s_hit", "Hit", "Skin Hit", HITS),
    [P_S_SNAP] = UNI("s_snap", "Snap", "Skin Snap", 0.2f),
    [P_S_METAL] = UNI("s_metal", "Metal", "Skin Metal", 0.0f),
    [P_S_TONE] = UNI("s_tone", "Tone", "Skin Tone", 0.7f),
    [P_S_MODE] = ENUM("s_mode", "Mode", "Skin Mode", MODES),
    [P_S_KIND] = ENUM("s_kind", "Kind", "Skin Mod Kind", KINDS),
    [P_S_RATE] = BI("s_rate", "Rate", "Skin Mod Rate"),
    [P_S_CURVE] = ENUM("s_curve", "Curve", "Skin Curve", SKIN_CURVES),
    [P_S_AIM1] = ENUM("s_aim1", "Aim", "Skin Mod Aim 1", SKIN_AIMS),
    [P_S_DEPTH1] = BI("s_depth1", "Depth", "Skin Mod Depth 1"),
    [P_S_AIM2] = ENUM("s_aim2", "Aim", "Skin Mod Aim 2", SKIN_AIMS),
    [P_S_DEPTH2] = BI("s_depth2", "Depth", "Skin Mod Depth 2"),

    [P_W_PITCH] = SEMI("w_pitch", "Pitch", "Wave Pitch", -12, 60),
    [P_W_BEND] = BI("w_bend", "Bend", "Wave Bend"),
    [P_W_DECAY] = UNI("w_decay", "Decay", "Wave Decay", 0.4f),
    [P_W_TABLE] = ENUM("w_table", "Table", "Wave Table", WAVE_TABLES),
    [P_W_WAVE] = UNI("w_wave", "Wave", "Wave Position", 0.0f),
    [P_W_FM] = UNI("w_fm", "FM", "Wave FM", 0.0f),
    [P_W_RING] = BI("w_ring", "Ring", "Wave Ring Mod"),
    [P_W_KIND] = ENUM("w_kind", "Kind", "Wave Mod Kind", KINDS),
    [P_W_RATE] = BI("w_rate", "Rate", "Wave Mod Rate"),
    [P_W_CURVE] = ENUM("w_curve", "Curve", "Wave Curve", CURVES),
    [P_W_AIM1] = ENUM("w_aim1", "Aim", "Wave Mod Aim 1", WAVE_AIMS),
    [P_W_DEPTH1] = BI("w_depth1", "Depth", "Wave Mod Depth 1"),
    [P_W_AIM2] = ENUM("w_aim2", "Aim", "Wave Mod Aim 2", WAVE_AIMS),
    [P_W_DEPTH2] = BI("w_depth2", "Depth", "Wave Mod Depth 2"),

    [P_N_PITCH] = SEMI("n_pitch", "Pitch", "Noise Pitch", -48, 48),
    [P_N_MODE] = ENUM("n_mode", "Mode", "Sample Mode", SAMPLE_MODES),
    [P_N_DECAY] = UNI("n_decay", "Decay", "Noise Decay", 0.3f),
    /* the noise tables, then the sample library (samples.c) */
    [P_N_TABLE] = { "n_table", "Table", "Noise Table", PK_ENUM, 0, N(NOISE_TABLES) - 1, 0, NULL, NOISE_TABLES, N(NOISE_TABLES), 1 },
    [P_N_COLOR] = BI("n_color", "Color", "Noise Color"),
    [P_N_START] = UNI("n_start", "Start", "Sample Start", 0.0f),
    [P_N_LOOP] = UNI("n_loop", "Loop", "Sample Loop", 1.0f),
    [P_N_KIND] = ENUM("n_kind", "Kind", "Noise Mod Kind", KINDS),
    [P_N_RATE] = BI("n_rate", "Rate", "Noise Mod Rate"),
    [P_N_CURVE] = ENUM("n_curve", "Curve", "Noise Curve", CURVES),
    [P_N_AIM1] = ENUM("n_aim1", "Aim", "Noise Mod Aim 1", NOISE_AIMS),
    [P_N_DEPTH1] = BI("n_depth1", "Depth", "Noise Mod Depth 1"),
    [P_N_AIM2] = ENUM("n_aim2", "Aim", "Noise Mod Aim 2", NOISE_AIMS),
    [P_N_DEPTH2] = BI("n_depth2", "Depth", "Noise Mod Depth 2"),

    [P_LEVEL] = UNI("level", "Level", "Level", 0.8f),
    [P_PAN] = BI("pan", "Pan", "Pan"),
    [P_FLAM] = UNI("flam", "Flam", "Flam", 0.0f),
    [P_DRIVE] = UNI("drive", "Drive", "Drive", 0.0f),
    [P_CRUSH] = UNI("crush", "Crush", "Crush", 0.0f),
    [P_LOW] = { "low", "Low", "Low Shelf", PK_FLOAT, -18.0f, 18.0f, 0.0f, "dB", NULL, 0, 0 },
    [P_HIGH] = { "high", "High", "High Shelf", PK_FLOAT, -18.0f, 18.0f, 0.0f, "dB", NULL, 0, 0 },
    /* a turn right rolls, left steps back (dice.h); never saved */
    [P_CHOKE] = ENUM("choke", "Choke", "Choke Group", CHOKES),
    [P_DICE] = { "dice", "Dice", "Dice", PK_ENUM, 0, 1, 1, NULL, STRUT_DICE_OPTIONS, 2, 0 },
};

const param_def_t STRUT_GLOBALS[G_COUNT] = {
    [G_SKIN_VIEW] = ENUM("skin_view", "Mod", "Skin Page", STRUT_VIEW_OPTIONS),
    [G_WAVE_VIEW] = ENUM("wave_view", "Mod", "Wave Page", STRUT_VIEW_OPTIONS),
    [G_NOISE_VIEW] = ENUM("noise_view", "Mod", "Noise Page", STRUT_VIEW_OPTIONS),
    [G_SIZE] = UNI("size", "Size", "Room Size", 0.4f),
    [G_GLUE] = UNI("glue", "Glue", "Glue", 0.0f),
    [G_WARM] = UNI("warm", "Warm", "Warmth", 0.0f),
    [G_VOL] = { "vol", "Vol", "Kit Volume", PK_FLOAT, -60.0f, 6.0f, 0.0f, "dB", NULL, 0, 0 },
    [G_CHOKE] = ENUM("kit_choke", "Choke", "Choke Group", CHOKES),
    [G_KIT] = ENUM("kit", "Kit", "Factory Kit", DICE_KIT_NAMES),
    [G_DICE] = { "kit_dice", "Dice", "Kit Dice", PK_ENUM, 0, 1, 1, NULL, STRUT_DICE_OPTIONS, 2, 0 },
};

const page_def_t STRUT_PAGES[STRUT_NPAGES] = {
    { "root", "Pad", 1, -1, P_SOUND, 8 },
    { "skin", "Skin", 1, G_SKIN_VIEW, P_S_PITCH, 14 },
    { "wave", "Wave", 1, G_WAVE_VIEW, P_W_PITCH, 14 },
    { "noise", "Noise", 1, G_NOISE_VIEW, P_N_PITCH, 14 },
    { "finish", "Finish", 1, -1, P_LEVEL, 8 },
    { "kit", "Kit", 0, -1, G_SIZE, 7 },
};
