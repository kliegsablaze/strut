/*
 * The v2 plugin entry, the pads, their voices and their keys (strut.h).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "strut.h"

/* ---- the pads ---- */

void strut_init(strut_t *s) {
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) s->pad[i].p[k] = STRUT_PAD_PARAMS[k].def;
    for (int k = 0; k < G_COUNT; k++) s->g[k] = STRUT_GLOBALS[k].def;
    s->press_at = s->note_at = -1.0;
    s->dither = 0x9E3779B9u;
    s->vol_g = -1.0f;
}

static void focus(strut_t *s, int pad) {
    s->focus = pad;
    s->focus_count++;
}

/* A press and its note cross a process boundary and arrive a few
 * milliseconds apart, in either order. Pair them inside a short window;
 * a note with no press is a sequenced note and moves nothing. */
static void pair(strut_t *s) {
    if (s->press_at < 0 || s->note_at < 0) return;
    if (fabs(s->press_at - s->note_at) <= STRUT_PRESS_WINDOW) {
        focus(s, s->note_pad);
        s->press_at = s->note_at = -1.0;
    }
}

void strut_press(strut_t *s) {
    s->press_at = s->now;
    pair(s);
}

void strut_note_on(strut_t *s, int note, int vel) {
    const int i = note - STRUT_NOTE0;
    if (i < 0 || i >= STRUT_PADS || vel <= 0) return;
    pad_t *p = &s->pad[i];
    /* the other voice, so the last hit keeps ringing under this one */
    const int v = (p->last + 1) % STRUT_VOICES;
    p->last = v;
    p->active[v] = 1;
    p->vel[v] = powf((float)vel / 127.0f, 1.5f);
    s->seed = s->seed * 1664525u + 1013904223u;
    skin_start(&p->skin[v], p->p, s->seed);
    s->note_pad = i;
    s->note_at = s->now;
    pair(s);
}

/* A level knob, as a fader: off at zero, then 30 dB of travel to full, so
 * every detent is heard. 0.8 is -6 dB. */
float strut_fader(float x) {
    return x <= 0.0f ? 0.0f : powf(10.0f, 1.5f * (fminf(x, 1.0f) - 1.0f));
}

void strut_render(strut_t *s, float *l, float *r, int frames) {
    memset(l, 0, sizeof(float) * frames);
    s->sounding = 0;
    for (int i = 0; i < STRUT_PADS; i++) {
        pad_t *p = &s->pad[i];
        const float level = strut_fader(p->p[P_LEVEL]);
        const float skin = strut_fader(p->p[P_SKIN]);
        for (int v = 0; v < STRUT_VOICES; v++) {
            if (!p->active[v]) continue;
            s->sounding++;
            p->active[v] = skin_render(&p->skin[v], p->p, 2.4f * p->vel[v] * level * skin, l, frames);
        }
    }
    memcpy(r, l, sizeof(float) * frames);   /* PAN comes with Finish (step 6) */
    s->now += (double)frames / STRUT_SR;
}

/* ---- the output (Quilt's, src/dsp/quilt.c) ---- */

/* Triangular noise of one 16-bit step. */
static inline float dither(uint32_t *d) {
    *d ^= *d << 13, *d ^= *d >> 17, *d ^= *d << 5;
    const float a = (float)(*d & 0xFFFF), b = (float)(*d >> 16);
    return (a + b) * (1.0f / 65536.0f) - 1.0f;
}

/* Soft above half scale, so a stack of pads rounds off rather than clips. */
static inline float limit(float x) {
    float a = fabsf(x);
    if (a > 0.5f) a = 0.5f + 0.5f * tanhf((a - 0.5f) * 2.0f);
    return copysignf(a, x);
}

/* The rounding's noise, shaped to the ear. The error each rounding makes is
 * fed back through these taps, so the noise left is filtered by 1 + sum h z^-k:
 * pushed out of 1 to 6 kHz, where hearing is keenest, up towards 20 kHz.
 * Designed by tools/noise_shape.py: the threshold of hearing (Terhardt 1979)
 * as the target, zero mean log gain (Gerzon and Craven 1989), made minimum
 * phase and cut to nine taps. Heard about 11 dB quieter than plain dither,
 * though it carries about 8 dB more power, all of it high. */
static const float SHAPE[STRUT_SHAPE] = {
    -1.69743f, 1.28417f, -0.07158f, -0.47574f, 0.2754f, 0.26476f, -0.182f, -0.0499f,
};

/* One sample to 16 bits: dg steps of triangular dither, and the shaped
 * feedback of e, the last errors, newest first. */
static inline int16_t to16(float x, float dg, uint32_t *d, float *e) {
    float fb = 0.0f;
    for (int k = 0; k < STRUT_SHAPE; k++) fb += SHAPE[k] * e[k];
    const float v = x * 32000.0f + dg * fb;
    float q = rintf(v + dg * dither(d));
    q = fminf(fmaxf(q, -32768.0f), 32767.0f);
    memmove(e + 1, e, sizeof(float) * (STRUT_SHAPE - 1));
    e[0] = q - v;       /* so the noise is the error filtered by 1 + sum h z^-k */
    return (int16_t)q;
}

/* VOL, then 16 bits, rounded with a step of triangular dither. Without it a
 * fading tail's last few steps become a gritty distortion whose harmonics
 * fold back down (heard on Quilt, 2026-10-05). The dither stays at full depth
 * while any voice sounds, because the grit lives in those last steps (Quilt's
 * fade below eight steps left it 11 dB proud there), and fades out over a
 * block once all have ended, so the kit rests in true silence. VOL glides
 * across the block, as the pads' levels do. */
void strut_output(strut_t *s, const float *l, const float *r, int16_t *out, int frames) {
    const float vol = s->g[G_VOL];
    const float g1 = vol <= -59.9f ? 0.0f : powf(10.0f, vol / 20.0f);
    const float g0 = s->vol_g < 0.0f ? g1 : s->vol_g;
    s->vol_g = g1;
    const float d0 = s->dither_g, d1 = s->sounding && g1 > 0.0f ? 1.0f : 0.0f;
    s->dither_g = d1;
    /* resting: forget the shaper's errors, so silence is exactly zero */
    if (d0 == 0.0f && d1 == 0.0f) memset(s->shape, 0, sizeof(s->shape));
    for (int i = 0; i < frames; i++) {
        const float x = (float)(i + 1) / (float)frames;
        const float g = g0 + (g1 - g0) * x, dg = d0 + (d1 - d0) * x;
        out[2 * i] = to16(limit(l[i] * g), dg, &s->dither, s->shape[0]);
        out[2 * i + 1] = to16(limit(r[i] * g), dg, &s->dither, s->shape[1]);
    }
}

/* ---- keys ---- */

/* "p05_s_pitch" -> pad 4, P_S_PITCH; -1 if it is not a pad key. */
static int pad_key(const char *key, int *pad) {
    if (key[0] != 'p' || key[1] < '0' || key[1] > '9' || key[2] < '0' || key[2] > '9' || key[3] != '_') return -1;
    const int n = (key[1] - '0') * 10 + (key[2] - '0');
    if (n < 1 || n > STRUT_PADS) return -1;
    for (int k = 0; k < P_COUNT; k++)
        if (!strcmp(key + 4, STRUT_PAD_PARAMS[k].key)) { *pad = n - 1; return k; }
    return -1;
}

static int global_key(const char *key) {
    for (int k = 0; k < G_COUNT; k++)
        if (!strcmp(key, STRUT_GLOBALS[k].key)) return k;
    return -1;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* An enum arrives as its option name or its index; both are accepted.
 * Anything else leaves the value alone. */
static void write_value(const param_def_t *d, float *v, const char *val) {
    if (d->kind == PK_ENUM) {
        for (int i = 0; i < d->noptions; i++)
            if (!strcmp(val, d->options[i])) { *v = (float)i; return; }
        char *end;
        const long i = strtol(val, &end, 10);
        if (end != val && *end == '\0' && i >= 0 && i < d->noptions) *v = (float)i;
        return;
    }
    char *end;
    const float x = strtof(val, &end);
    if (end == val) return;
    *v = clampf(d->kind == PK_INT ? roundf(x) : x, d->min, d->max);
}

static int read_value(const param_def_t *d, float v, char *buf, int len) {
    if (d->kind == PK_ENUM) return snprintf(buf, len, "%s", d->options[(int)v]);
    if (d->kind == PK_INT) return snprintf(buf, len, "%d", (int)v);
    return snprintf(buf, len, "%.4f", (double)v);
}

/* ---- the v2 API ---- */

static const host_api_v1_t *g_host;

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir;
    (void)json_defaults;
    strut_t *s = calloc(1, sizeof(*s));
    if (s) strut_init(s);
    /* Reloading a module the host still holds hands back the old code (the
     * host opens the new synth before closing the old, and dlopen() matches
     * by path), so the log says which build is really playing. */
    if (g_host && g_host->log) g_host->log("strut " STRUT_VERSION " loaded");
    return s;
}

static void destroy_instance(void *instance) { free(instance); }

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    if (len < 3) return;
    if ((msg[0] & 0xF0) == 0x90) strut_note_on(instance, msg[1], msg[2]);
}

static void set_param(void *instance, const char *key, const char *val) {
    strut_t *s = instance;
    int pad, k;
    if (!strcmp(key, "pad")) {
        const int n = atoi(val);
        if (n >= 1 && n <= STRUT_PADS) focus(s, n - 1);
    } else if (!strcmp(key, "pad_press")) {
        strut_press(s);
    } else if ((k = pad_key(key, &pad)) >= 0) {
        write_value(&STRUT_PAD_PARAMS[k], &s->pad[pad].p[k], val);
    } else if ((k = global_key(key)) >= 0) {
        write_value(&STRUT_GLOBALS[k], &s->g[k], val);
    }
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    strut_t *s = instance;
    int pad, k;
    if (!strcmp(key, "ui_hierarchy")) return strut_contract_hierarchy(buf, buf_len);
    if (!strcmp(key, "chain_params")) return strut_contract_params(buf, buf_len);
    if (!strcmp(key, "pad")) return snprintf(buf, buf_len, "%d", s->focus + 1);
    if ((k = pad_key(key, &pad)) >= 0) return read_value(&STRUT_PAD_PARAMS[k], s->pad[pad].p[k], buf, buf_len);
    if ((k = global_key(key)) >= 0) return read_value(&STRUT_GLOBALS[k], s->g[k], buf, buf_len);
    return -1;
}

static int get_error(void *instance, char *buf, int buf_len) {
    (void)instance;
    (void)buf;
    (void)buf_len;
    return 0;
}

static void render_block(void *instance, int16_t *out, int frames) {
    strut_t *s = instance;
#if defined(__aarch64__)
    /* Flush denormals to zero while Strut renders, so a long tail never falls
     * into slow subnormal arithmetic; the host's own mode is put back after. */
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr | (1ull << 24)));
#endif
    float l[STRUT_MAX_BLOCK], r[STRUT_MAX_BLOCK];
    for (int done = 0; done < frames;) {
        const int n = frames - done < STRUT_MAX_BLOCK ? frames - done : STRUT_MAX_BLOCK;
        strut_render(s, l, r, n);
        strut_output(s, l, r, out + 2 * done, n);
        done += n;
    }
#if defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr));
#endif
}

static plugin_api_v2_t api = {
    .api_version = 2,
    .create_instance = create_instance,
    .destroy_instance = destroy_instance,
    .on_midi = on_midi,
    .set_param = set_param,
    .get_param = get_param,
    .get_error = get_error,
    .render_block = render_block,
};

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
    g_host = host;
    return &api;
}
