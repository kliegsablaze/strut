/*
 * The v2 plugin entry, the pads and the placeholder voice (strut.h).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "strut.h"

const param_def_t STRUT_PAD_PARAMS[P_COUNT] = {
    [P_TUNE] = { "tune", "Tune", "Tune", -1.0f, 1.0f, 0.0f },
    [P_DECAY] = { "decay", "Decay", "Decay", 0.0f, 1.0f, 0.4f },
    [P_LEVEL] = { "level", "Level", "Level", 0.0f, 1.0f, 0.8f },
};

/* ---- the pads ---- */

void strut_init(strut_t *s) {
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) s->pad[i].p[k] = STRUT_PAD_PARAMS[k].def;
    s->press_at = s->note_at = -1.0;
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
    p->active = 1;
    p->phase = 0.0;
    p->env = 1.0f;
    p->drop = 1.0f;
    p->vel = (float)vel / 127.0f;
    s->note_pad = i;
    s->note_at = s->now;
    pair(s);
}

/* Placeholder: a sine from 45 Hz up a semitone per pad, dropping an octave
 * at the start, fading by DECAY. Replaced by the engines. */
void strut_render(strut_t *s, float *l, float *r, int frames) {
    memset(l, 0, sizeof(float) * frames);
    memset(r, 0, sizeof(float) * frames);
    for (int i = 0; i < STRUT_PADS; i++) {
        pad_t *p = &s->pad[i];
        if (!p->active) continue;
        const float hz = 45.0f * powf(2.0f, (i + 24.0f * p->p[P_TUNE]) / 12.0f);
        const float t60 = 0.03f * powf(100.0f, p->p[P_DECAY]);   /* 30 ms .. 3 s */
        const float fall = expf(-6.9f / (t60 * STRUT_SR));
        const float dfall = expf(-1.0f / (0.012f * STRUT_SR));
        const float g = p->vel * p->p[P_LEVEL] * 0.5f;
        for (int n = 0; n < frames; n++) {
            p->phase += hz * (1.0f + p->drop) / STRUT_SR;
            if (p->phase >= 1.0) p->phase -= 1.0;
            const float x = sinf(6.2831853f * (float)p->phase) * p->env * g;
            l[n] += x;
            r[n] += x;
            p->env *= fall;
            p->drop *= dfall;
        }
        if (p->env < 1e-4f) p->active = 0;
    }
    s->now += (double)frames / STRUT_SR;
}

/* ---- keys ---- */

/* "p01_tune" -> pad 0, P_TUNE; -1 if it is not a pad key. */
static int pad_key(const char *key, int *pad) {
    if (key[0] != 'p' || key[1] < '0' || key[1] > '9' || key[2] < '0' || key[2] > '9' || key[3] != '_') return -1;
    const int n = (key[1] - '0') * 10 + (key[2] - '0');
    if (n < 1 || n > STRUT_PADS) return -1;
    for (int k = 0; k < P_COUNT; k++)
        if (!strcmp(key + 4, STRUT_PAD_PARAMS[k].key)) { *pad = n - 1; return k; }
    return -1;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- the v2 API ---- */

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)module_dir;
    (void)json_defaults;
    strut_t *s = calloc(1, sizeof(*s));
    if (s) strut_init(s);
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
        s->pad[pad].p[k] = clampf((float)atof(val), STRUT_PAD_PARAMS[k].min, STRUT_PAD_PARAMS[k].max);
    }
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    strut_t *s = instance;
    int pad, k;
    if (!strcmp(key, "ui_hierarchy")) return strut_contract_hierarchy(buf, buf_len);
    if (!strcmp(key, "chain_params")) return strut_contract_params(buf, buf_len);
    if (!strcmp(key, "pad")) return snprintf(buf, buf_len, "%d", s->focus + 1);
    if ((k = pad_key(key, &pad)) >= 0) return snprintf(buf, buf_len, "%.4f", (double)s->pad[pad].p[k]);
    return -1;
}

static int get_error(void *instance, char *buf, int buf_len) {
    (void)instance;
    (void)buf;
    (void)buf_len;
    return 0;
}

static void render_block(void *instance, int16_t *out, int frames) {
    float l[STRUT_MAX_BLOCK], r[STRUT_MAX_BLOCK];
    if (frames > STRUT_MAX_BLOCK) frames = STRUT_MAX_BLOCK;
    strut_render(instance, l, r, frames);
    for (int n = 0; n < frames; n++) {
        out[2 * n] = (int16_t)lrintf(clampf(l[n], -1.0f, 1.0f) * 32767.0f);
        out[2 * n + 1] = (int16_t)lrintf(clampf(r[n], -1.0f, 1.0f) * 32767.0f);
    }
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
    (void)host;
    return &api;
}
