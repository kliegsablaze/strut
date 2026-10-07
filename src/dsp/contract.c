/*
 * What Strut tells the host about its pages (ui_hierarchy) and its knobs
 * (chain_params). DESIGN.md, Pads and focus, and Pages.
 *
 *   chain_params  every knob ONCE, by its bare key ("tune"), plus PAD. The
 *                 host types each pad's key ("p05_tune") through the rack's
 *                 template, in the UI (child_key.mjs) and in the chain
 *                 (lanes, step locks, modulation: CHAIN.md, "A rack's
 *                 templated keys are typed by the CHAIN"). Listing all
 *                 sixteen copies would pass the chain's 256-entry table
 *                 four times over and lose every pad past the fourth.
 *   ui_hierarchy  root is the Pad page and the rack itself (the host titles
 *                 any root page "Main"); Skin, Wave,
 *                 Noise and Finish are racks on the same template and the
 *                 same focus; Kit is the only level that is not per pad.
 *
 * An engine page declares fourteen knobs and MOD. Seven are gated on its
 * MOD switch being Sound, seven on Mod; hidden knobs close up, so MOD stays
 * in cell 8. The switch is a gate key its own knob writes, so the grid
 * re-plans the moment it flips. It is one key for every pad, mapped out of
 * the template by child_key_overrides.
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

static void param_json(sb_t *b, const param_def_t *d) {
    sb_printf(b, ",{\"key\":\"%s\",\"name\":\"%s\",\"short_name\":\"%s\",", d->key, d->name, d->cell);
    if (d->kind == PK_ENUM) {
        sb_printf(b, "\"type\":\"enum\",\"options\":[");
        for (int i = 0; i < d->noptions; i++) sb_printf(b, "%s\"%s\"", i ? "," : "", d->options[i]);
        sb_printf(b, "],\"options_as_string\":true,\"default\":\"%s\"", d->options[(int)d->def]);
        /* MOD flips on a click; two words need no list over the page. */
        if (d->options == STRUT_VIEW_OPTIONS) sb_printf(b, ",\"peek\":false");
    } else if (d->kind == PK_INT) {
        sb_printf(b, "\"type\":\"int\",\"min\":%d,\"max\":%d,\"default\":%d", (int)d->min, (int)d->max, (int)d->def);
        if (d->unit) sb_printf(b, ",\"unit\":\"%s\"", d->unit);
    } else if (d->unit) {
        sb_printf(b, "\"type\":\"float\",\"min\":%g,\"max\":%g,\"step\":0.5,\"default\":%g,\"unit\":\"%s\"",
                  (double)d->min, (double)d->max, (double)d->def, d->unit);
    } else {
        sb_printf(b, "\"type\":\"float\",\"min\":%g,\"max\":1,\"step\":0.01,\"default\":%g,\"unit\":\"%%\"",
                  (double)d->min, (double)d->def);
    }
    sb_printf(b, "}");
}

static const param_def_t *page_param(const page_def_t *pg, int i) {
    return pg->per_pad ? &STRUT_PAD_PARAMS[pg->first + i] : &STRUT_GLOBALS[pg->first + i];
}

/* The fields that make a level one of the sixteen pads. Every rack names
 * the same focus, so the host plans one pad picker for all of them. The
 * header of a pad's page reads "<child_label> <pad>", never the level's
 * name, so each rack labels its pads with its own page: "Skin 3". */
static void rack(sb_t *b, const page_def_t *pg) {
    sb_printf(b, ",\"child_count\":%d,\"child_label\":\"%s\",\"child_key_template\":\"p{index}_{key}\","
                 "\"child_index_base\":1,\"child_index_digits\":2,\"child_index_param\":\"pad\"",
              STRUT_PADS, pg->label);
}

static void level(sb_t *b, const page_def_t *pg) {
    const char *view = pg->view >= 0 ? STRUT_GLOBALS[pg->view].key : NULL;
    const int half = pg->count / 2;   /* an engine page: seven Sound, seven Mod */
    sb_printf(b, "\"%s\":{\"name\":\"%s\"", pg->level, pg->label);
    if (pg->per_pad) rack(b, pg);
    if (pg == &STRUT_PAGES[0])
        sb_printf(b, ",\"child_press_param\":\"pad_press\",\"child_note_base\":%d", STRUT_NOTE0);
    if (view) sb_printf(b, ",\"child_key_overrides\":{\"%s\":\"%s\"}", view, view);

    sb_printf(b, ",\"knobs\":[");
    for (int i = 0; i < pg->count; i++) sb_printf(b, "%s\"%s\"", i ? "," : "", page_param(pg, i)->key);
    if (view) sb_printf(b, ",\"%s\"", view);
    /* The Kit page's last cell picks the pad the other pages edit. A cell
     * for the focus is what stops the host planning a Selected Pad page of
     * its own (page_plan.mjs, childPickerNeeded); tapping a pad still picks
     * it, and this is the way to when page-follow is off. */
    if (!pg->per_pad) sb_printf(b, ",\"pad\"");
    sb_printf(b, "],\"params\":[");
    for (int i = 0; i < pg->count; i++) {
        sb_printf(b, "%s{\"key\":\"%s\"", i ? "," : "", page_param(pg, i)->key);
        if (view)
            sb_printf(b, ",\"visible_if\":{\"param\":\"%s\",\"equals\":\"%s\"}", view,
                      STRUT_VIEW_OPTIONS[i < half ? 0 : 1]);
        sb_printf(b, "}");
    }
    if (view) sb_printf(b, ",{\"key\":\"%s\"}", view);
    if (!pg->per_pad) sb_printf(b, ",{\"key\":\"pad\"}");
    /* root links every other page, in the order the jog walks them */
    if (pg == &STRUT_PAGES[0])
        for (int p = 1; p < STRUT_NPAGES; p++)
            sb_printf(b, ",{\"level\":\"%s\",\"label\":\"%s\"}", STRUT_PAGES[p].level, STRUT_PAGES[p].label);
    sb_printf(b, "]}");
}

int strut_contract_hierarchy(char *buf, int len) {
    sb_t b = { buf, len, 0 };
    sb_printf(&b, "{\"pad_layout\":\"drums\",\"levels\":{");
    for (int p = 0; p < STRUT_NPAGES; p++) {
        if (p) sb_printf(&b, ",");
        level(&b, &STRUT_PAGES[p]);
    }
    sb_printf(&b, "}}");
    return done(&b);
}

int strut_contract_params(char *buf, int len) {
    sb_t b = { buf, len, 0 };
    sb_printf(&b, "[{\"key\":\"pad\",\"name\":\"Selected Pad\",\"short_name\":\"Pad\",\"type\":\"int\",\"min\":1,\"max\":%d}",
              STRUT_PADS);
    for (int k = 0; k < P_COUNT; k++) param_json(&b, &STRUT_PAD_PARAMS[k]);
    for (int k = 0; k < G_COUNT; k++) param_json(&b, &STRUT_GLOBALS[k]);
    sb_printf(&b, "]");
    return done(&b);
}
