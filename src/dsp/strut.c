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

void strut_render(strut_t *s, float *l, float *r, int frames) {
    memset(l, 0, sizeof(float) * frames);
    for (int i = 0; i < STRUT_PADS; i++) {
        pad_t *p = &s->pad[i];
        const float level = p->p[P_LEVEL] * p->p[P_LEVEL];
        const float skin = p->p[P_SKIN] * p->p[P_SKIN];
        for (int v = 0; v < STRUT_VOICES; v++) {
            if (!p->active[v]) continue;
            p->active[v] = skin_render(&p->skin[v], p->p, 1.5f * p->vel[v] * level * skin, l, frames);
        }
    }
    memcpy(r, l, sizeof(float) * frames);   /* PAN comes with Finish (step 6) */
    s->now += (double)frames / STRUT_SR;
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
