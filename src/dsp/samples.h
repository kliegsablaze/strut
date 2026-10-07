/*
 * The sample library (DESIGN.md, The sample library): the list TABLE offers
 * past the noise tables, and the samples themselves, loaded on demand.
 *
 * Every host call runs on the audio thread, so files are read on a thread of
 * Strut's own, one an instance (plugin_api_v1.h: demoted to ordinary
 * priority, off the audio's core). It talks to the audio thread through
 * three arrays of words, each written by one side only:
 *
 *   want   the audio thread: the entry each pad's TABLE names, -1 none,
 *          and its MODE
 *   ready  the loader: the sample each pad may now play, NULL none
 *   used   the audio thread: the sample each pad's voice is playing
 *
 * and a count of blocks rendered. A sample no pad has ready is freed only
 * once two blocks have passed and no voice is using it, so the audio thread
 * never reads one being freed (a pointer it read before the swap is in
 * `used` by the end of that block).
 */
#ifndef STRUT_SAMPLES_H
#define STRUT_SAMPLES_H

#include <stddef.h>
#include <stdint.h>

#include "noise.h"

#define SM_PADS 16
#define SM_FILES 1024           /* the library and your own, at most */
#define SM_NAME 32
#define SM_FRAC 11              /* a one-shot's read position: fraction bits */
#define SM_MAX (1 << 20)        /* its longest, at NT_SR: 11.9 s */
#define CY_N 4096               /* a cycle at NT_SR: one period of 2048 at 44.1 kHz */
#define CY_FRAC 20              /* its read position wraps the cycle by itself */
#define CY_HZ ((float)NT_SR / CY_N)     /* a cycle's pitch at its own rate, 21.53 Hz */

enum { SM_SAMPLE, SM_RESYNTH, SM_NOISE };   /* MODE */

/* A loaded sample: its copies an octave apart, its bands and its power, in
 * the form of a noise table, so PITCH, COLOR, their level match and Skin's
 * strike treat it as they treat noise. */
/* MODE's other two ways of playing a one-shot are made from it when a pad
 * first asks for them (smp_build), and flagged in `has` once made; the
 * audio thread reads them only after seeing the flag. */
typedef struct smp {
    nt_table_t t;
    int entry;              /* in the list */
    int cycle;              /* a single cycle: looped, pitched from A1 */
    uint32_t len;           /* level 0's samples, at NT_SR */
    float speed;            /* its file's rate against 44.1 kHz */
    int16_t *data;
    size_t bytes;
    struct smp *next;       /* the loader's own list */
    unsigned dead;          /* the block count when no pad had it ready; 0 while one does */
    int has, tried;         /* MODE's ways made (1 << mode), and tried by the loader */
    /* Noise: the sample's colour as it changes, every pitch taken out */
    nt_table_t noise;
    int16_t *ndata;
    /* Resynth: its sine waves, RS_SLOTS a frame, RS_HOP apart (each a
     * frequency in cycles a level-0 sample and a level in level 0's units),
     * and the noise left over once they are taken out */
    int frames;
    float *fq, *am;
    uint32_t onset;         /* where its hit begins, in level-0 samples: its first sound within 20 dB of its peak */
    nt_table_t rest;
    int16_t *rdata;
} smp_t;

/* The list: built once, when the first instance is made, from module_dir's
 * samples/ and, on the Move, your own folder. Read-only after. */
void smp_catalogue(const char *module_dir);
int smp_count(void);
const char *smp_name(int i);
const char *smp_path(int i);

/* A file read to mono floats at its own rate; the caller frees *x. */
int smp_read_wav(const char *path, float **x, int *frames, int *rate);
smp_t *smp_load(int entry);
/* Makes MODE's way m (SM_RESYNTH or SM_NOISE) of s; 0 if it could not. */
int smp_build(smp_t *s, int m);
void smp_free(smp_t *s);

typedef struct {
    int want[SM_PADS];
    int mode[SM_PADS];      /* the audio thread: each pad's MODE */
    const smp_t *ready[SM_PADS];
    const smp_t *used[SM_PADS];
    unsigned blocks;
    /* the loader's own */
    smp_t *cache;
    size_t cached;          /* bytes held */
    int seen[SM_PADS], still[SM_PADS];
    void *thread;
    int quit;
} smp_lib_t;

/* One pass of the loader: loads what pads want once it has stood still for
 * `patience` passes, and frees what nothing uses. The thread runs it every
 * 10 ms; the tests run it by hand. */
void smp_service(smp_lib_t *lib, int patience);
void smp_start(smp_lib_t *lib);     /* from create_instance */
void smp_stop(smp_lib_t *lib);      /* from destroy_instance: joins, and frees all */

#endif
