/*
 * Black-box tests: Strut driven through the v2 API, as the host drives it.
 * Writes ui_hierarchy.json and chain_params.json for plan.test.mjs.
 *   tests/run.sh
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "strut.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static int checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); } } while (0)

/* The host's contract buffers are 128 KB (shadow_constants.h). */
static char buf[131072];
static plugin_api_v2_t *A;

static const char *get(void *p, const char *k) {
    int n = A->get_param(p, k, buf, sizeof(buf));
    return n < 0 ? NULL : buf;
}

static void midi3(void *p, int a, int b, int c) {
    const uint8_t m[3] = { (uint8_t)a, (uint8_t)b, (uint8_t)c };
    A->on_midi(p, m, 3, 0);
}

/* RMS over some blocks, and the largest sample seen. */
static double rms(void *p, int blocks, int *peak) {
    int16_t out[256];
    double acc = 0;
    long n = 0;
    for (int b = 0; b < blocks; b++) {
        A->render_block(p, out, 128);
        for (int i = 0; i < 256; i++) {
            acc += (double)out[i] * out[i];
            n++;
            if (peak && abs(out[i]) > *peak) *peak = abs(out[i]);
        }
    }
    return sqrt(acc / (double)n);
}

static void dump(void *p, const char *key, const char *dir) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s.json", dir, key);
    const char *s = get(p, key);
    FILE *f = fopen(path, "w");
    CHECK(f && s, "dump %s", key);
    if (f && s) fputs(s, f);
    if (f) fclose(f);
}

static void contracts(void *p, const char *dir) {
    const int nh = A->get_param(p, "ui_hierarchy", buf, sizeof(buf));
    CHECK(nh > 0 && nh < (int)sizeof(buf), "ui_hierarchy fits 128 KB (%d bytes)", nh);
    const int nc = A->get_param(p, "chain_params", buf, sizeof(buf));
    CHECK(nc > 0 && nc < (int)sizeof(buf), "chain_params fits 128 KB (%d bytes)", nc);
    printf("contracts: ui_hierarchy %d bytes, chain_params %d bytes\n", nh, nc);
    CHECK(A->get_param(p, "chain_params", buf, 100) < 0, "a short buffer is refused, not overrun");
    dump(p, "ui_hierarchy", dir);
    dump(p, "chain_params", dir);
}

static void pads(void) {
    void *p = A->create_instance(".", "");
    int peak = 0;
    CHECK(rms(p, 10, &peak) == 0 && peak == 0, "silent until played");
    for (int i = 0; i < STRUT_PADS; i++) {
        midi3(p, 0x90, STRUT_NOTE0 + i, 100);
        peak = 0;
        const double r = rms(p, 20, &peak);
        CHECK(r > 100 && peak < 32767, "pad %d sounds and does not clip (rms %.0f, peak %d)", i + 1, r, peak);
    }
    peak = 0;
    rms(p, 2000, NULL);
    CHECK(rms(p, 10, &peak) == 0, "and falls silent");
    A->set_param(p, "p03_decay", "7");
    CHECK(!strcmp(get(p, "p03_decay"), "1.0000"), "a pad's knob is kept, clamped");
    A->destroy_instance(p);
}

/* Every pad has its own copy of every knob, kept apart; enums travel as
 * option names (or indices), and the MOD switches are one key for the kit. */
static void keys(void) {
    void *p = A->create_instance(".", "");
    char key[32], val[32];
    int bad = 0;
    for (int i = 1; i <= STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) {
            const param_def_t *d = &STRUT_PAD_PARAMS[k];
            snprintf(key, sizeof(key), "p%02d_%s", i, d->key);
            if (d->kind == PK_ENUM) snprintf(val, sizeof(val), "%s", d->options[(i + k) % d->noptions]);
            else snprintf(val, sizeof(val), "%d", (int)d->max);
            A->set_param(p, key, val);
        }
    for (int i = 1; i <= STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) {
            const param_def_t *d = &STRUT_PAD_PARAMS[k];
            snprintf(key, sizeof(key), "p%02d_%s", i, d->key);
            const char *got = get(p, key);
            if (d->kind == PK_ENUM) bad += !got || strcmp(got, d->options[(i + k) % d->noptions]);
            else bad += !got || atof(got) != (double)(int)d->max;
        }
    CHECK(bad == 0, "every pad keeps its own %d knobs (%d wrong)", P_COUNT, bad);
    CHECK(get(p, "tune") == NULL && get(p, "p17_tune") == NULL && get(p, "p00_tune") == NULL,
          "a bare or out-of-range pad key is not served");
    A->set_param(p, "p02_s_mode", "2");
    CHECK(!strcmp(get(p, "p02_s_mode"), "High"), "an enum takes an index too");
    A->set_param(p, "p02_s_mode", "Sideways");
    CHECK(!strcmp(get(p, "p02_s_mode"), "High"), "and ignores a name it does not know");
    CHECK(!strcmp(get(p, "skin_view"), "Sound"), "the engine pages open on Sound");
    A->set_param(p, "skin_view", "Mod");
    CHECK(!strcmp(get(p, "skin_view"), "Mod") && !strcmp(get(p, "wave_view"), "Sound"), "MOD flips one page");
    A->set_param(p, "vol", "-6");
    CHECK(!strcmp(get(p, "vol"), "-6.0000"), "the kit's VOL is kept");
    A->destroy_instance(p);
}

/* ---- Skin, through strut_render, in floats so a NaN cannot hide ---- */

static float L[STRUT_SR * 4], R[STRUT_SR * 4];

/* Hits pad 1 with its knobs as set and renders seconds of it. */
static int hit(strut_t *s, float seconds) {
    strut_note_on(s, STRUT_NOTE0, 100);
    const int total = (int)(seconds * STRUT_SR);
    for (int n = 0; n < total; n += 128) strut_render(s, L + n, R + n, total - n < 128 ? total - n : 128);
    return total;
}

static strut_t *fresh(void) {
    strut_t *s = calloc(1, sizeof(*s));
    strut_init(s);
    return s;
}

static double window_rms(int from, int len) {
    double a = 0;
    for (int n = from; n < from + len; n++) a += (double)L[n] * L[n];
    return sqrt(a / len);
}

static void skin(void) {
    /* PITCH is the pitch: zero crossings of a long, plain ring */
    const float pitches[] = { -12, 0, 12, 31, 60 };
    for (int i = 0; i < 5; i++) {
        strut_t *s = fresh();
        s->pad[0].p[P_S_PITCH] = pitches[i];
        s->pad[0].p[P_S_RING] = 1.0f;
        s->pad[0].p[P_S_MODE] = 1;
        const int n = hit(s, 1.0f);
        int cross = 0, first = -1, last = 0;
        for (int k = STRUT_SR / 10; k < n - 1; k++)
            if (L[k] <= 0 && L[k + 1] > 0) { if (first < 0) first = k; last = k; cross++; }
        const double hz = (cross - 1) * (double)STRUT_SR / (last - first);
        const float want = skin_hz(s->pad[0].p);
        CHECK(fabs(hz / want - 1) < 0.01, "PITCH %+g st rings at %.1f Hz, want %.1f", pitches[i], hz, want);
        free(s);
    }

    /* RING is the ring time: the fall between two windows, as a T60 */
    const float rings[] = { 0.3f, 0.6f, 0.9f };
    for (int i = 0; i < 3; i++) {
        strut_t *s = fresh();
        s->pad[0].p[P_S_RING] = rings[i];
        s->pad[0].p[P_S_PITCH] = 24;
        const float want = skin_t60(s->pad[0].p);
        hit(s, 4.0f);
        const int a = (int)(0.2f * want * STRUT_SR), b = (int)(0.6f * want * STRUT_SR), w = 2048;
        const double db = 20 * log10(window_rms(a, w) / window_rms(b, w));
        const double t60 = 60.0 * (b - a) / STRUT_SR / db;
        CHECK(fabs(t60 / want - 1) < 0.1, "RING %.1f rings %.3f s, want %.3f", rings[i], t60, want);
        free(s);
    }

    /* Every Skin knob at its ends and middle (every option of an enum):
     * it sounds, it is finite, it does not clip, and it dies away. */
    const int knobs[] = { P_S_PITCH, P_S_RING, P_S_HIT, P_S_SNAP, P_S_METAL, P_S_TONE, P_S_MODE, P_TUNE, P_DECAY };
    double lo = 1e9, hi = 0;
    for (size_t k = 0; k < sizeof(knobs) / sizeof(knobs[0]); k++) {
        const param_def_t *d = &STRUT_PAD_PARAMS[knobs[k]];
        const int steps = d->kind == PK_ENUM ? d->noptions : 3;
        for (int i = 0; i < steps; i++) {
            strut_t *s = fresh();
            const float v = d->kind == PK_ENUM ? (float)i : d->min + (d->max - d->min) * (float)i / 2;
            s->pad[0].p[knobs[k]] = v;
            const int n = hit(s, 2.0f);
            double peak = 0, acc = 0;
            int finite = 1;
            for (int j = 0; j < n; j++) {
                finite &= isfinite(L[j]);
                if (fabs(L[j]) > peak) peak = fabs(L[j]);
                if (j < STRUT_SR / 4) acc += (double)L[j] * L[j];
            }
            const double rms = sqrt(acc / (STRUT_SR / 4));
            if (peak < lo) lo = peak;
            if (peak > hi) hi = peak;
            (void)rms;
            CHECK(finite, "%s %g: finite", d->key, v);
            CHECK(peak < 0.9, "%s %g: does not clip (peak %.2f)", d->key, v, peak);
            CHECK(peak > 0.05, "%s %g: sounds (peak %.3f)", d->key, v, peak);
            /* the longest ring, 12 s, may still be going after two */
            if (!(knobs[k] == P_S_RING && i == 2) && !(knobs[k] == P_DECAY && i == 2))
                CHECK(!s->pad[0].active[0] && !s->pad[0].active[1], "%s %g: dies away", d->key, v);
            free(s);
        }
    }
    printf("skin: peak over every knob setting %.3f .. %.3f (%.1f dB)\n", lo, hi,
           20 * log10(hi / lo));
}

static void focus(void) {
    void *p = A->create_instance(".", "");
    CHECK(!strcmp(get(p, "pad"), "1"), "pad 1 is focused at the start");
    midi3(p, 0x90, STRUT_NOTE0 + 4, 100);
    rms(p, 40, NULL);
    CHECK(!strcmp(get(p, "pad"), "1"), "a sequenced note moves nothing");
    A->set_param(p, "pad_press", "1");
    midi3(p, 0x90, STRUT_NOTE0 + 6, 100);
    CHECK(!strcmp(get(p, "pad"), "7"), "a press, then its note, focuses that pad");
    rms(p, 40, NULL);
    midi3(p, 0x90, STRUT_NOTE0 + 9, 100);
    A->set_param(p, "pad_press", "1");
    CHECK(!strcmp(get(p, "pad"), "10"), "a note, then its press, too");
    rms(p, 40, NULL);
    A->set_param(p, "pad_press", "1");
    CHECK(!strcmp(get(p, "pad"), "10"), "a press with no note inside 50 ms moves nothing");
    A->set_param(p, "pad", "16");
    CHECK(!strcmp(get(p, "pad"), "16"), "PAD can be set");
    A->destroy_instance(p);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    A = move_plugin_init_v2(NULL);
    CHECK(A && A->api_version == 2, "the v2 API");
    void *p = A->create_instance(".", "");
    contracts(p, dir);
    A->destroy_instance(p);
    pads();
    keys();
    skin();
    focus();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
