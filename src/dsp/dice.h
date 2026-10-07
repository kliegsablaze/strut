/*
 * DICE (DESIGN.md, Sounds, presets and the randomiser): a new sound for a
 * pad, or for the whole kit, rolled by each pad's place in the kit, with
 * eight steps back.
 *
 * Each DICE is a two-word switch, Back and Roll, that the host writes once
 * a turn: right is Roll, left is Back, however far the knob went. Roll goes
 * forward to the next roll kept, or rolls a new one past the newest; Back
 * goes to the one before, at most DICE_KEEP back. Moving saves what is
 * there first, so edits made after a roll survive a step back and forward.
 */
#ifndef STRUT_DICE_H
#define STRUT_DICE_H

#include <stdint.h>

#define DICE_KEEP 8             /* rolls a DICE can step back through */
#define DICE_SLOTS (DICE_KEEP + 1)

enum { DICE_BACK, DICE_ROLL };  /* the switch's two words, in params.c's order */

/* What a pad's place in the kit asks it to be. */
typedef enum {
    ROLE_KICK, ROLE_SNARE, ROLE_CLAP, ROLE_HAT, ROLE_OPEN, ROLE_CYMBAL, ROLE_RIM,
    ROLE_TOM, ROLE_PERC, ROLE_BELL, ROLE_BASS, ROLE_FX,
    ROLE_COUNT
} dice_role_t;

/* The rolls a DICE has made: roll `at` is the one on the pads, 0 the sound
 * before any roll; each kept roll is in slot (roll % DICE_SLOTS). */
typedef struct {
    int newest, at;
} dice_hist_t;

dice_role_t dice_role(int pad);
const char *dice_role_name(dice_role_t r);

/* A new sound for pad `pad` in p (its STRUT_PAD_PARAMS), by its role.
 * `kit` is set when the whole kit rolls: the pad's place in the mix (LEVEL,
 * PAN, CHOKE) is rolled too; a pad's own roll keeps it. */
void dice_roll(float *p, int pad, int kit, uint32_t *rng);

#endif
