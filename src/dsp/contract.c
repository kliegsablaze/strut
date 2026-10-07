/*
 * What Strut tells the host about its pages (ui_hierarchy) and its knobs
 * (chain_params).
 *
 * SCAFFOLD: the template shape (DESIGN.md, Pads and focus). Root is the rack
 * of sixteen pads, keys "p01_tune"..."p16_level", the focused pad in "pad",
 * and the host's pad-press vouch in "pad_press". Build step 1 measures
 * whether every engine's keys fit this way.
 */
#include <stdarg.h>
#include <stdio.h>

#include "strut.h"

typedef struct { char *p; int len, n; } sb_t;

static void sb_printf(sb_t *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int room = b->len - b->n;
    const int w = vsnprintf(b->p + b->n, room > 0 ? (size_t)room : 0, fmt, ap);
    va_end(ap);
    if (w > 0) b->n += w;
}

static int done(sb_t *b) { return b->n < b->len ? b->n : -1; }

int strut_contract_hierarchy(char *buf, int len) {
    sb_t b = { buf, len, 0 };
    sb_printf(&b, "{\"pad_layout\":\"drums\",\"levels\":{\"root\":{\"name\":\"Pads\","
                  "\"child_count\":%d,\"child_label\":\"Pad\",\"child_key_template\":\"p{index}_{key}\","
                  "\"child_index_base\":1,\"child_index_digits\":2,\"child_index_param\":\"pad\","
                  "\"child_press_param\":\"pad_press\",\"child_note_base\":%d,\"knobs\":[",
              STRUT_PADS, STRUT_NOTE0);
    for (int k = 0; k < P_COUNT; k++) sb_printf(&b, "%s\"%s\"", k ? "," : "", STRUT_PAD_PARAMS[k].key);
    sb_printf(&b, "]}}}");
    return done(&b);
}

int strut_contract_params(char *buf, int len) {
    sb_t b = { buf, len, 0 };
    sb_printf(&b, "[{\"key\":\"pad\",\"name\":\"Pad\",\"type\":\"int\",\"min\":1,\"max\":%d}", STRUT_PADS);
    for (int i = 1; i <= STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) {
            const param_def_t *d = &STRUT_PAD_PARAMS[k];
            sb_printf(&b, ",{\"key\":\"p%02d_%s\",\"name\":\"%s\",\"short_name\":\"%s\",\"type\":\"float\","
                          "\"min\":%g,\"max\":%g,\"step\":0.01}",
                      i, d->key, d->name, d->cell, (double)d->min, (double)d->max);
        }
    sb_printf(&b, "]");
    return done(&b);
}
