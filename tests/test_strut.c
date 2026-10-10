/*
 * Black-box tests: Strut driven through the v2 API, as the host drives it.
 * Writes ui_hierarchy.json and chain_params.json for plan.test.mjs.
 *   tests/run.sh
 */
#define _POSIX_C_SOURCE 200809L    /* nanosleep, for the loader's own thread */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "host/plugin_api_v1.h"
#include "levels.h"
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
            if (k == P_DICE) continue;      /* a turn, not a knob kept (dice()) */
            snprintf(key, sizeof(key), "p%02d_%s", i, d->key);
            if (d->kind == PK_ENUM) snprintf(val, sizeof(val), "%s", d->options[(i + k) % d->noptions]);
            else snprintf(val, sizeof(val), "%d", (int)d->max);
            A->set_param(p, key, val);
        }
    for (int i = 1; i <= STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) {
            const param_def_t *d = &STRUT_PAD_PARAMS[k];
            if (k == P_DICE) continue;
            snprintf(key, sizeof(key), "p%02d_%s", i, d->key);
            const char *got = get(p, key);
            if (d->kind == PK_ENUM) bad += !got || strcmp(got, d->options[(i + k) % d->noptions]);
            else bad += !got || atof(got) != (double)(int)d->max;
        }
    CHECK(bad == 0, "every pad keeps its own %d knobs (%d wrong)", P_COUNT - 1, bad);
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
                CHECK(!s->pad[0].voice.active, "%s %g: dies away", d->key, v);
            free(s);
        }
    }
    printf("skin: peak over every knob setting %.3f .. %.3f (%.1f dB)\n", lo, hi,
           20 * log10(hi / lo));
}

/* Zero-crossing pitch of L from `from` to n. */
static double crossing_hz(int from, int n) {
    int cross = 0, first = -1, last = 0;
    for (int k = from; k < n - 1; k++)
        if (L[k] <= 0 && L[k + 1] > 0) { if (first < 0) first = k; last = k; cross++; }
    return cross > 1 ? (cross - 1) * (double)STRUT_SR / (last - first) : 0;
}

/* Goertzel through a Hann window: the level of L at hz over n samples from
 * `from`, as an amplitude. The window keeps a loud partial from leaking
 * into a quiet frequency being measured. */
static double level_at(double hz, int from, int n) {
    const double w = 2 * 3.14159265358979 * hz / STRUT_SR, c = 2 * cos(w);
    double s1 = 0, s2 = 0, sum = 0;
    for (int k = 0; k < n; k++) {
        const double h = 0.5 - 0.5 * cos(2 * 3.14159265358979 * k / n);
        const double s0 = h * L[from + k] + c * s1 - s2;
        s2 = s1, s1 = s0, sum += h;
    }
    return 2 * sqrt(fmax(s1 * s1 + s2 * s2 - c * s1 * s2, 0)) / sum;
}

/* A pad of Wave alone. */
static strut_t *wave_pad(void) {
    strut_t *s = fresh();
    s->pad[0].p[P_SKIN] = 0;
    s->pad[0].p[P_WAVE] = 0.8f;
    return s;
}

/* Peak and health of a hit: finite, under full scale, heard, and gone by
 * the end unless it is meant to last. */
static double sweep_hit(strut_t *s, const char *what, float v, int must_end, double heard) {
    const int n = hit(s, 2.0f);
    double peak = 0;
    int finite = 1;
    for (int j = 0; j < n; j++) {
        finite &= isfinite(L[j]);
        peak = fmax(peak, fabs(L[j]));
    }
    CHECK(finite, "%s %g: finite", what, v);
    CHECK(peak < 0.9, "%s %g: does not clip (peak %.2f)", what, v, peak);
    CHECK(peak > heard, "%s %g: sounds (peak %.3f)", what, v, peak);
    if (must_end) CHECK(!s->pad[0].voice.active, "%s %g: dies away", what, v);
    return peak;
}

static void wave(void) {
    /* PITCH is the pitch, on the sine */
    const float pitches[] = { -12, 0, 12, 31, 60 };
    for (int i = 0; i < 5; i++) {
        strut_t *s = wave_pad();
        s->pad[0].p[P_W_PITCH] = pitches[i];
        s->pad[0].p[P_W_DECAY] = 1.0f;
        const int n = hit(s, 1.0f);
        const double hz = crossing_hz(STRUT_SR / 10, n), want = wave_hz(s->pad[0].p);
        CHECK(fabs(hz / want - 1) < 0.01, "Wave PITCH %+g st plays %.1f Hz, want %.1f", pitches[i], hz, want);
        free(s);
    }

    /* DECAY is the fall, as a T60 */
    const float decays[] = { 0.3f, 0.6f, 0.9f };
    for (int i = 0; i < 3; i++) {
        strut_t *s = wave_pad();
        s->pad[0].p[P_W_DECAY] = decays[i];
        s->pad[0].p[P_W_PITCH] = 24;
        const float want = wave_t60(s->pad[0].p);
        hit(s, 4.0f);
        const int a = (int)(0.2f * want * STRUT_SR), b = (int)(0.6f * want * STRUT_SR), w = 2048;
        const double db = 20 * log10(window_rms(a, w) / window_rms(b, w));
        const double t60 = 60.0 * (b - a) / STRUT_SR / db;
        CHECK(fabs(t60 / want - 1) < 0.1, "Wave DECAY %.1f falls in %.3f s, want %.3f", decays[i], t60, want);
        free(s);
    }

    /* BEND starts the pitch away and brings it home */
    {
        strut_t *s = wave_pad();
        s->pad[0].p[P_W_BEND] = 0.5f;           /* a fall of an octave */
        s->pad[0].p[P_W_PITCH] = 24;
        s->pad[0].p[P_W_DECAY] = 0.8f;
        const int n = hit(s, 1.0f);
        const double start = crossing_hz(0, 400), end = crossing_hz(n / 2, n), want = wave_hz(s->pad[0].p);
        CHECK(start > 1.6 * want, "BEND starts high (%.0f Hz against %.0f)", start, want);
        CHECK(fabs(end / want - 1) < 0.01, "and settles on PITCH (%.1f Hz)", end);
        free(s);
    }

    /* Every Wave knob at its ends and middle, every table across WAVE */
    const int knobs[] = { P_W_PITCH, P_W_BEND, P_W_DECAY, P_W_WAVE, P_W_FM, P_W_RING };
    double lo = 1e9, hi = 0;
    for (size_t k = 0; k < sizeof(knobs) / sizeof(knobs[0]); k++) {
        const param_def_t *d = &STRUT_PAD_PARAMS[knobs[k]];
        for (int i = 0; i < 3; i++) {
            strut_t *s = wave_pad();
            const float v = d->min + (d->max - d->min) * (float)i / 2;
            s->pad[0].p[knobs[k]] = v;
            const double peak = sweep_hit(s, d->key, v, !(knobs[k] == P_W_DECAY && i == 2), 0.05);
            lo = fmin(lo, peak), hi = fmax(hi, peak);
            free(s);
        }
    }
    for (int t = 0; t < WT_TABLES; t++)
        for (int i = 0; i <= 4; i++) {
            strut_t *s = wave_pad();
            s->pad[0].p[P_W_TABLE] = (float)t;
            s->pad[0].p[P_W_WAVE] = (float)i / 4;
            char what[64];
            snprintf(what, sizeof(what), "w_table %s, w_wave", STRUT_PAD_PARAMS[P_W_TABLE].options[t]);
            const double peak = sweep_hit(s, what, (float)i / 4, 1, 0.05);
            lo = fmin(lo, peak), hi = fmax(hi, peak);
            free(s);
        }
    printf("wave: peak over every knob setting %.3f .. %.3f (%.1f dB)\n", lo, hi, 20 * log10(hi / lo));

    /* A high saw does not alias: its 4th to 6th harmonics would fold back
     * to 15.9, 8.9 and 1.9 kHz */
    {
        strut_t *s = wave_pad();
        s->pad[0].p[P_W_WAVE] = 0.5f;           /* the saw */
        s->pad[0].p[P_W_PITCH] = 60;
        s->pad[0].p[P_TUNE] = 24;               /* 7040 Hz */
        s->pad[0].p[P_W_DECAY] = 1.0f;
        hit(s, 0.5f);
        const int from = 4410, n = 8192;
        const double f0 = wave_hz(s->pad[0].p), top = level_at(f0, from, n);
        double worst = 0;
        for (int h = 4; h <= 6; h++) worst = fmax(worst, level_at(STRUT_SR - h * f0, from, n));
        CHECK(20 * log10(worst / top) < -70, "a 7 kHz saw does not alias (%.0f dB)", 20 * log10(worst / top));
        free(s);
    }

    /* Skin struck by Wave rings at Skin's PITCH, sized like any hit */
    lo = 1e9, hi = 0;
    for (int t = 0; t < WT_TABLES; t++)
        for (int k = 0; k < 9; k++) {
            strut_t *s = fresh();
            s->pad[0].p[P_S_HIT] = HIT_WAVE;
            s->pad[0].p[P_W_TABLE] = (float)t;
            s->pad[0].p[P_W_WAVE] = (float)(k % 3) / 2;
            s->pad[0].p[P_S_SNAP] = (float)(k / 3) / 2;
            s->pad[0].p[P_W_PITCH] = (float)(t * 7 % 30);
            char what[64];
            snprintf(what, sizeof(what), "Skin hit by %s, case", STRUT_PAD_PARAMS[P_W_TABLE].options[t]);
            const double peak = sweep_hit(s, what, (float)k, 1, 0.05);
            lo = fmin(lo, peak), hi = fmax(hi, peak);
            free(s);
        }
    printf("wave: Skin hit by Wave, peak %.3f .. %.3f (%.1f dB)\n", lo, hi, 20 * log10(hi / lo));

    /* FM: Skin's ring bends Wave, and changes it. Wave 17 semitones over
     * Skin, so no sideband folds back onto Wave's pitch (at 1:1 they do). */
    {
        strut_t *s = wave_pad();
        s->pad[0].p[P_W_PITCH] = 17;
        s->pad[0].p[P_S_RING] = 0.8f;
        s->pad[0].p[P_W_DECAY] = 0.8f;
        hit(s, 0.5f);
        const double plain = level_at(wave_hz(s->pad[0].p), 2205, 4096);
        free(s);
        s = wave_pad();
        s->pad[0].p[P_W_PITCH] = 17;
        s->pad[0].p[P_S_RING] = 0.8f;
        s->pad[0].p[P_W_DECAY] = 0.8f;
        s->pad[0].p[P_W_FM] = 0.8f;             /* a swing of 2.6 times: little left at the pitch */
        sweep_hit(s, "w_fm with a long ring", 0.8f, 0, 0.05);
        hit(s, 0.5f);
        const double bent = level_at(wave_hz(s->pad[0].p), 2205, 4096);
        CHECK(bent < 0.5 * plain, "FM moves Wave's energy off its pitch (%.2f of it left)", bent / plain);
        free(s);
    }
}

/* Renders n samples of whatever is sounding into L from `at`. */
/* A pad of Noise alone, on table t, falling slowly enough to measure. */
static strut_t *noise_pad(int t) {
    strut_t *s = fresh();
    s->pad[0].p[P_SKIN] = 0;
    s->pad[0].p[P_NOISE] = 0.8f;
    s->pad[0].p[P_N_TABLE] = (float)t;
    s->pad[0].p[P_N_DECAY] = 1.0f;
    return s;
}

/* Zero crossings a second: rises and falls with where noise's power is. */
static double crossings(int from, int n) {
    int c = 0;
    for (int k = from; k < from + n - 1; k++) c += (L[k] <= 0) != (L[k + 1] <= 0);
    return c * (double)STRUT_SR / n;
}

/* The mean level from lo to hi Hz, in 50 Hz steps. */
static double band_level(double lo, double hi, int from, int n) {
    double a = 0;
    int k = 0;
    for (double f = lo; f <= hi; f += 50, k++) a += level_at(f, from, n) * level_at(f, from, n);
    return sqrt(a / k);
}

static void noise(void) {
    const int nt = STRUT_PAD_PARAMS[P_N_TABLE].noptions;

    /* every table at the ends and middle of PITCH, COLOR and DECAY: it
     * sounds, it is finite, it does not clip, and it dies away. Heard means
     * over -30 dB here: deep in COLOR's low-pass, a short noise's peak is
     * all chance (its level is checked below). */
    const int knobs[] = { P_N_PITCH, P_N_COLOR, P_N_DECAY };
    double lo = 1e9, hi = 0;
    for (int t = 0; t < nt; t++)
        for (int k = 0; k < 3; k++)
            for (int i = 0; i < 3; i++) {
                const param_def_t *d = &STRUT_PAD_PARAMS[knobs[k]];
                strut_t *s = noise_pad(t);
                s->pad[0].p[P_N_DECAY] = 0.3f;
                const float v = d->min + (d->max - d->min) * (float)i / 2;
                s->pad[0].p[knobs[k]] = v;
                char what[64];
                snprintf(what, sizeof(what), "%s %s", STRUT_PAD_PARAMS[P_N_TABLE].options[t], d->key);
                const double peak = sweep_hit(s, what, v, !(knobs[k] == P_N_DECAY && i == 2), 0.03);
                lo = fmin(lo, peak), hi = fmax(hi, peak);
                free(s);
            }
    printf("noise: peak over every table and knob end %.3f .. %.3f (%.1f dB)\n", lo, hi, 20 * log10(hi / lo));

    /* the tables' levels, for the record */
    printf("noise: tables' RMS");
    for (int t = 0; t < nt; t++) {
        strut_t *s = noise_pad(t);
        hit(s, 0.3f);
        printf(" %s %.3f", STRUT_PAD_PARAMS[P_N_TABLE].options[t], window_rms(0, STRUT_SR / 4));
        free(s);
    }
    printf("\n");

    /* PITCH: White an octave down crosses zero half as often, two down a quarter */
    {
        double z[3];
        for (int i = 0; i < 3; i++) {
            strut_t *s = noise_pad(NT_WHITE);
            s->pad[0].p[P_N_PITCH] = (float)(-12 * i);
            hit(s, 0.5f);
            z[i] = crossings(0, STRUT_SR / 2);
            free(s);
        }
        CHECK(fabs(z[1] / z[0] - 0.5) < 0.05 && fabs(z[2] / z[0] - 0.25) < 0.03,
              "PITCH -12 and -24 move White down an octave and two (%.3f, %.3f)", z[1] / z[0], z[2] / z[0]);
        double w[2];
        for (int i = 0; i < 2; i++) {
            strut_t *s = noise_pad(NT_WIRES);
            s->pad[0].p[P_N_PITCH] = (float)(12 * i);
            hit(s, 0.5f);
            w[i] = crossings(0, STRUT_SR / 2);
            free(s);
        }
        CHECK(w[1] / w[0] > 1.3, "PITCH +12 moves Wires up (%.2f times the crossings)", w[1] / w[0]);
    }

    /* played above its table's rate, nothing folds back: White at +7 has
     * nothing above 15 kHz, so all that is there is the cubic's images */
    {
        strut_t *s = noise_pad(NT_WHITE);
        s->pad[0].p[P_N_PITCH] = 7;
        hit(s, 0.3f);
        const double in = band_level(2000, 10000, 2000, 8192), out = band_level(18500, 21500, 2000, 8192);
        CHECK(20 * log10(out / in) < -40, "White at +7 st: nothing folds back above 18.5 kHz (%.1f dB)",
              20 * log10(out / in));
        free(s);
    }

    /* DECAY is the fall: the drop between two windows, as a T60 */
    const float decays[] = { 0.5f, 0.8f };
    for (int i = 0; i < 2; i++) {
        strut_t *s = noise_pad(NT_WHITE);
        s->pad[0].p[P_N_DECAY] = decays[i];
        const float want = noise_t60(s->pad[0].p);
        hit(s, 2.0f);
        const int a = (int)(0.2f * want * STRUT_SR), b = (int)(0.6f * want * STRUT_SR), w = 2048;
        const double t60 = 60.0 * (b - a) / STRUT_SR / (20 * log10(window_rms(a, w) / window_rms(b, w)));
        CHECK(fabs(t60 / want - 1) < 0.1, "Noise DECAY %.1f falls in %.3f s, want %.3f", decays[i], t60, want);
        free(s);
    }

    /* COLOR changes the colour, not the level: darker to the left, thinner
     * to the right, and every table within a few dB of the centre */
    {
        const float colors[] = { -1, -0.5f, 0.5f, 1 };
        double worst = 0;
        for (int t = 0; t < nt; t++) {
            strut_t *s = noise_pad(t);
            hit(s, 0.2f);
            const double r0 = window_rms(0, STRUT_SR / 5), z0 = crossings(0, STRUT_SR / 5);
            free(s);
            for (int c = 0; c < 4; c++) {
                s = noise_pad(t);
                s->pad[0].p[P_N_COLOR] = colors[c];
                hit(s, 0.2f);
                const double db = 20 * log10(window_rms(0, STRUT_SR / 5) / r0), z = crossings(0, STRUT_SR / 5);
                if (fabs(db) > fabs(worst)) worst = db;
                CHECK(fabs(db) < 4, "%s COLOR %+.1f keeps the level (%+.1f dB)",
                      STRUT_PAD_PARAMS[P_N_TABLE].options[t], colors[c], db);
                if (t == NT_WHITE && colors[c] == -1) CHECK(z < 0.1 * z0, "COLOR left darkens White (%.2f the crossings)", z / z0);
                if (t == NT_WHITE && colors[c] == 1) CHECK(z > 1.2 * z0, "COLOR right thins White (%.2f the crossings)", z / z0);
                free(s);
            }
        }
        printf("noise: COLOR's level, worst %+.1f dB from the centre\n", worst);
    }

    /* each hit starts somewhere new in the loop */
    {
        strut_t *s = noise_pad(NT_WHITE);
        s->pad[0].p[P_N_DECAY] = 0.2f;
        hit(s, 1.0f);
        float first[64];
        memcpy(first, L, sizeof(first));
        hit(s, 0.1f);
        CHECK(memcmp(first, L, sizeof(first)) != 0, "two hits are not the same noise");
        free(s);
    }

    /* Skin struck by Noise rings about as loud as struck by its own burst,
     * whatever the colour, with NOISE itself off. Each hit is a little
     * different, as noise is, so the power of 48 is compared. */
    {
        const int hits[] = { -1, NT_WHITE, NT_PINK, NT_BROWN, NT_HISS, NT_WIRES, NT_METAL, NT_CRACKLE, NT_GRIT };
        double ref = 0, lo = 1e9, hi = -1e9;
        for (int i = 0; i < 9; i++) {
            double pw = 0;
            for (int h = 0; h < 48; h++) {
                strut_t *s = fresh();
                s->seed = 7919u * (uint32_t)h;
                s->pad[0].p[P_S_PITCH] = 24;
                s->pad[0].p[P_S_RING] = 0.6f;
                s->pad[0].p[P_S_MODE] = 1;
                s->pad[0].p[P_S_SNAP] = 0.4f;
                s->pad[0].p[P_S_HIT] = hits[i] < 0 ? HIT_BURST : HIT_NOISE;
                if (hits[i] >= 0) s->pad[0].p[P_N_TABLE] = (float)hits[i];
                hit(s, 0.15f);
                const double r = window_rms(STRUT_SR / 20, STRUT_SR / 10);
                pw += r * r / 48;
                free(s);
            }
            if (i == 0) ref = pw;
            else {
                const double db = 10 * log10(pw / ref);
                lo = fmin(lo, db), hi = fmax(hi, db);
                CHECK(fabs(db) < 3, "Skin struck by %s rings within 3 dB of its burst (%+.1f dB)",
                      STRUT_PAD_PARAMS[P_N_TABLE].options[hits[i]], db);
            }
        }
        printf("noise: Skin struck by Noise, %+.1f .. %+.1f dB from its burst\n", lo, hi);
    }
}

static void play(strut_t *s, int at, int n) {
    for (int k = 0; k < n; k += 128) strut_render(s, L + at + k, R + at + k, n - k < 128 ? n - k : 128);
}

/* The peak of x from `from`, over n samples. */
static double peak_of(const float *x, int from, int n) {
    double p = 0;
    for (int k = from; k < from + n; k++) p = fmax(p, fabs(x[k]));
    return p;
}

/* Pad COLOR and the Finish page (DESIGN.md, How Finish works). */
static void finish(void) {
    /* every Finish knob, and Pad COLOR, at its ends and middle, on a pad of
     * all three engines: it sounds, it is finite, and it dies away */
    const int knobs[] = { P_COLOR, P_PAN, P_CHOKE, P_FLAM, P_DRIVE, P_CRUSH, P_LOW, P_HIGH };
    for (size_t k = 0; k < sizeof(knobs) / sizeof(knobs[0]); k++) {
        const param_def_t *d = &STRUT_PAD_PARAMS[knobs[k]];
        const int steps = d->kind == PK_ENUM ? d->noptions : 3;
        for (int i = 0; i < steps; i++) {
            strut_t *s = fresh();
            s->pad[0].p[P_WAVE] = 0.6f, s->pad[0].p[P_NOISE] = 0.6f, s->pad[0].p[P_N_DECAY] = 0.3f;
            const float v = d->kind == PK_ENUM ? (float)i : d->min + (d->max - d->min) * (float)i / 2;
            s->pad[0].p[knobs[k]] = v;
            const int n = hit(s, 2.0f);
            const double pk = fmax(peak_of(L, 0, n), peak_of(R, 0, n));
            int finite = 1;
            for (int j = 0; j < n; j++) finite &= isfinite(L[j]) && isfinite(R[j]);
            CHECK(finite, "%s %g: finite", d->key, v);
            /* LOW and HIGH may lift a loud hit 18 dB; the output's limiter rounds it */
            const double most = knobs[k] == P_LOW || knobs[k] == P_HIGH ? 8.0 : 1.5;
            CHECK(pk > 0.03 && pk < most, "%s %g: sounds, and not wildly (peak %.3f)", d->key, v, pk);
            CHECK(!s->pad[0].voice.active, "%s %g: dies away", d->key, v);
            free(s);
        }
    }

    /* PAN: hard left is the left alone, as loud as the centre in power */
    {
        double pw[3];
        for (int i = 0; i < 3; i++) {
            strut_t *s = fresh();
            s->pad[0].p[P_PAN] = (float)(i - 1);
            hit(s, 0.3f);
            double a = 0, b = 0;
            for (int k = 0; k < STRUT_SR / 4; k++) a += (double)L[k] * L[k], b += (double)R[k] * R[k];
            pw[i] = a + b;
            if (i == 0) CHECK(b < 1e-9 * a, "PAN left: nothing on the right");
            if (i == 2) CHECK(a < 1e-9 * b, "PAN right: nothing on the left");
            free(s);
        }
        CHECK(fabs(10 * log10(pw[0] / pw[1])) < 0.1 && fabs(10 * log10(pw[2] / pw[1])) < 0.1,
              "PAN keeps the power (%+.2f, %+.2f dB)", 10 * log10(pw[0] / pw[1]), 10 * log10(pw[2] / pw[1]));
    }

    /* CHOKE: pad 2 in pad 1's group silences pad 1 within the fade */
    for (int same = 0; same < 2; same++) {
        strut_t *s = fresh();
        s->pad[0].p[P_S_RING] = 0.9f;
        s->pad[1].p[P_SKIN] = 0.0f;     /* pad 2 heard as nothing, so only pad 1 is measured */
        s->pad[0].p[P_CHOKE] = 1;
        s->pad[1].p[P_CHOKE] = same ? 1 : 2;
        strut_note_on(s, STRUT_NOTE0, 100);
        play(s, 0, 4410);
        strut_note_on(s, STRUT_NOTE0 + 1, 100);
        play(s, 4410, 4410);
        const double after = window_rms(4410 + STRUT_CHOKE + 128, 2000), before = window_rms(2000, 2000);
        if (same) CHECK(after == 0 && !s->pad[0].voice.active, "CHOKE: a hit in the group stops the pad");
        else CHECK(after > 0.3 * before, "CHOKE: another group leaves it ringing");
        free(s);
    }

    /* FLAM: three onsets, each gap apart, rising to the hit */
    {
        strut_t *s = fresh();
        s->pad[0].p[P_S_RING] = 0.1f;
        s->pad[0].p[P_FLAM] = 0.5f;
        const int gap = (int)(0.002f * powf(25.0f, 0.5f) * STRUT_SR);
        hit(s, 0.2f);
        const double a = peak_of(L, 0, gap / 2), b = peak_of(L, gap, gap / 2), c = peak_of(L, 2 * gap, gap / 2);
        CHECK(a > 0.05 && b > a && c > b, "FLAM: three hits %d samples apart, rising (%.3f %.3f %.3f)", gap, a, b, c);
        free(s);
    }

    /* DRIVE: a sine grows overtones, a loud hit stays about as loud
     * halfway round, and fully round it is louder, still under full scale */
    {
        double third[3], peak[3];
        for (int i = 0; i < 3; i++) {
            strut_t *s = wave_pad();
            s->pad[0].p[P_W_DECAY] = 1.0f;
            s->pad[0].p[P_W_PITCH] = 24;
            s->pad[0].p[P_DRIVE] = i == 2 ? 1.0f : i ? 0.5f : 0.0f;
            hit(s, 0.5f);
            third[i] = level_at(3 * wave_hz(s->pad[0].p), 4410, 8192) / level_at(wave_hz(s->pad[0].p), 4410, 8192);
            peak[i] = peak_of(L, 0, STRUT_SR / 2);
            free(s);
        }
        CHECK(third[1] > 30 * third[0] + 0.01, "DRIVE adds overtones (third harmonic %.4f, from %.4f)", third[1], third[0]);
        CHECK(fabs(20 * log10(peak[1] / peak[0])) < 6, "DRIVE halfway keeps a loud hit's level (%+.1f dB)", 20 * log10(peak[1] / peak[0]));
        CHECK(20 * log10(peak[2] / peak[0]) > 5 && peak[2] < 1.0, "DRIVE fully round is louder, under full scale (%+.1f dB, peak %.2f)",
              20 * log10(peak[2] / peak[0]), peak[2]);
    }

    /* DRIVE turned while a pad rings: no crackle. Each block's turn is a
     * big one, in and out of the curve twice; no sample may jump further
     * than DRIVE held fully round ever makes one jump. */
    {
        double jump[2];
        for (int i = 0; i < 2; i++) {
            strut_t *s = wave_pad();
            s->pad[0].p[P_W_DECAY] = 1.0f;
            s->pad[0].p[P_W_PITCH] = -12;
            s->pad[0].p[P_DRIVE] = i ? 0.0f : 1.0f;
            strut_note_on(s, STRUT_NOTE0, 100);
            const int n = STRUT_SR / 2;
            for (int k = 0; k < n; k += 128) {
                if (i) {
                    const int b = (k / 128) % 50;
                    s->pad[0].p[P_DRIVE] = (b < 25 ? b : 50 - b) / 24.0f;
                    if (s->pad[0].p[P_DRIVE] > 1) s->pad[0].p[P_DRIVE] = 1;
                }
                strut_render(s, L + k, R + k, 128);
            }
            jump[i] = 0;
            for (int k = 2; k < n; k++) jump[i] = fmax(jump[i], fabs(L[k] - 2 * L[k - 1] + L[k - 2]));
            free(s);
        }
        CHECK(jump[1] <= 1.2 * jump[0], "DRIVE turned while ringing does not crackle (bend %.4f, held round %.4f)", jump[1], jump[0]);
    }

    /* CRUSH: fully on, a held sample on few levels */
    {
        strut_t *s = wave_pad();
        s->pad[0].p[P_CRUSH] = 1.0f;
        hit(s, 0.2f);
        int same = 0;
        for (int k = 1000; k < 3000; k++) same += L[k] == L[k - 1];
        CHECK(same > 1500, "CRUSH holds samples (%d of 2000 repeat)", same);
        free(s);
    }

    /* LOW and HIGH lift and cut their ends of White noise by most of 18 dB */
    {
        double lo[3], hi[3];
        for (int i = 0; i < 3; i++) {
            strut_t *s = noise_pad(NT_WHITE);
            s->pad[0].p[P_LOW] = (float)(18 * (i - 1));
            hit(s, 0.3f);
            lo[i] = band_level(40, 80, 0, 8192);
            free(s);
            s = noise_pad(NT_WHITE);
            s->pad[0].p[P_HIGH] = (float)(18 * (i - 1));
            hit(s, 0.3f);
            hi[i] = band_level(14000, 16000, 0, 8192);
            free(s);
        }
        CHECK(20 * log10(lo[2] / lo[1]) > 14 && 20 * log10(lo[0] / lo[1]) < -14, "LOW lifts and cuts the lows (%+.1f, %+.1f dB)",
              20 * log10(lo[2] / lo[1]), 20 * log10(lo[0] / lo[1]));
        CHECK(20 * log10(hi[2] / hi[1]) > 12 && 20 * log10(hi[0] / hi[1]) < -12, "HIGH lifts and cuts the highs (%+.1f, %+.1f dB)",
              20 * log10(hi[2] / hi[1]), 20 * log10(hi[0] / hi[1]));
    }

    /* Pad COLOR: darker to the left, thinner to the right */
    {
        double z[3];
        for (int i = 0; i < 3; i++) {
            strut_t *s = noise_pad(NT_WHITE);
            s->pad[0].p[P_COLOR] = (float)(i - 1);
            hit(s, 0.3f);
            z[i] = crossings(0, STRUT_SR / 5);
            free(s);
        }
        CHECK(z[0] < 0.2 * z[1] && z[2] > 1.1 * z[1], "Pad COLOR darkens and thins (%.2f, %.2f the crossings)", z[0] / z[1], z[2] / z[1]);
    }
}

/* The modulators and CURVE (DESIGN.md, Modulation). */
static void modulation(void) {
    /* Envelope on Skin's pitch: the hit starts high and settles on PITCH,
     * and the knob itself is never written */
    {
        strut_t *s = fresh();
        float *p = s->pad[0].p;
        p[P_S_RING] = 0.9f, p[P_S_MODE] = MODE_BAND, p[P_S_PITCH] = 12;
        p[P_S_KIND] = KIND_ENVELOPE, p[P_S_RATE] = -0.6f, p[P_S_AIM1] = 0, p[P_S_DEPTH1] = 0.5f;
        hit(s, 1.0f);
        const double early = crossing_hz(100, 1500), late = crossing_hz(STRUT_SR / 2, STRUT_SR - 1);
        CHECK(early > 1.5 * late && fabs(late / skin_hz(p) - 1) < 0.02,
              "Envelope on Skin Pitch: starts high (%.0f Hz), settles on PITCH (%.0f Hz, want %.0f)", early, late, skin_hz(p));
        CHECK(p[P_S_PITCH] == 12, "the modulator never writes the knob");
        free(s);
    }

    /* LFO on Wave's level: the note swells and dips at RATE */
    {
        strut_t *s = wave_pad();
        float *p = s->pad[0].p;
        p[P_W_DECAY] = 1.0f;
        p[P_W_KIND] = KIND_LFO, p[P_W_RATE] = -0.55f, p[P_W_AIM1] = 4, p[P_W_DEPTH1] = 0.8f;   /* Level */
        hit(s, 1.0f);
        double lo = 1e9, hi = 0;
        for (int w = 2000; w < 30000; w += 512) {
            const double r = window_rms(w, 512);
            lo = fmin(lo, r), hi = fmax(hi, r);
        }
        CHECK(20 * log10(hi / lo) > 10, "LFO on Wave Level wobbles (%.1f dB between swell and dip)", 20 * log10(hi / lo));
        free(s);
    }

    /* Random on Wave's pitch: two hits, two pitches */
    {
        strut_t *s = wave_pad();
        float *p = s->pad[0].p;
        p[P_W_DECAY] = 0.8f;
        p[P_W_KIND] = KIND_RANDOM, p[P_W_AIM1] = 0, p[P_W_DEPTH1] = 0.3f;
        hit(s, 0.3f);
        const double a = crossing_hz(2000, 12000);
        hit(s, 0.3f);
        const double b = crossing_hz(2000, 12000);
        CHECK(fabs(a / b - 1) > 0.01, "Random on Wave Pitch: two hits, two pitches (%.1f, %.1f Hz)", a, b);
        free(s);
    }

    /* Velocity on Noise's level: a soft hit drops further than velocity
     * alone makes it */
    {
        double r[2][2];
        for (int m = 0; m < 2; m++)
            for (int v = 0; v < 2; v++) {
                strut_t *s = noise_pad(NT_WHITE);
                float *p = s->pad[0].p;
                if (m) p[P_N_KIND] = KIND_VELOCITY, p[P_N_AIM1] = 4, p[P_N_DEPTH1] = 1.0f;
                strut_note_on(s, STRUT_NOTE0, v ? 127 : 40);
                play(s, 0, 4410);
                r[m][v] = window_rms(0, 4410);
                free(s);
            }
        const double plain = 20 * log10(r[0][1] / r[0][0]), moved = 20 * log10(r[1][1] / r[1][0]);
        CHECK(moved > plain + 3, "Velocity on Noise Level widens the hard-soft gap (%.1f dB, from %.1f)", moved, plain);
    }

    /* CURVE on Wave: Hold stays full then stops; Swell rises; Soft starts
     * gently; Ping falls faster than Natural */
    {
        double e[5][3];
        for (int c = 0; c < 5; c++) {
            strut_t *s = wave_pad();
            float *p = s->pad[0].p;
            p[P_W_DECAY] = 0.6f, p[P_W_CURVE] = (float)c, p[P_W_PITCH] = 24;
            const float T = wave_t60(p);
            hit(s, 2.0f);
            e[c][0] = peak_of(L, 0, 44);                            /* the first millisecond */
            e[c][1] = window_rms((int)(0.05f * T * STRUT_SR), 512);
            e[c][2] = window_rms((int)(0.4f * T * STRUT_SR), 512);
            CHECK(!s->pad[0].voice.active, "CURVE %s: dies away", STRUT_PAD_PARAMS[P_W_CURVE].options[c]);
            free(s);
        }
        CHECK(20 * log10(e[CURVE_HOLD][2] / e[CURVE_HOLD][1]) > -1.5, "Hold stays full (%+.1f dB at 0.4 of DECAY)",
              20 * log10(e[CURVE_HOLD][2] / e[CURVE_HOLD][1]));
        CHECK(e[CURVE_SWELL][2] > 4 * e[CURVE_SWELL][1], "Swell rises (%.1f times by 0.4 of DECAY)", e[CURVE_SWELL][2] / e[CURVE_SWELL][1]);
        CHECK(e[CURVE_SOFT][0] < 0.5 * e[CURVE_NATURAL][0], "Soft starts gently (%.2f of Natural's first millisecond)",
              e[CURVE_SOFT][0] / e[CURVE_NATURAL][0]);
        CHECK(e[CURVE_PING][2] < 0.5 * e[CURVE_NATURAL][2], "Ping falls faster (%.2f of Natural at 0.4 of DECAY)",
              e[CURVE_PING][2] / e[CURVE_NATURAL][2]);
    }

    /* Noise's Clap: three slaps with quiet between, then a tail as loud */
    {
        strut_t *s = fresh();
        float *p = s->pad[0].p;
        p[P_SKIN] = 0.0f, p[P_NOISE] = 0.8f, p[P_N_DECAY] = 0.45f, p[P_N_CURVE] = CURVE_CLAP;
        hit(s, 1.0f);
        const double slap = window_rms(0, 132), gap = window_rms(330, 88), tail = window_rms(1400, 132);
        CHECK(20 * log10(gap / slap) < -15, "Clap: quiet between slaps (%+.1f dB)", 20 * log10(gap / slap));
        CHECK(fabs(20 * log10(tail / slap)) < 6, "Clap: the tail starts as loud as a slap (%+.1f dB)", 20 * log10(tail / slap));
        free(s);
    }

    /* Skin's Hold: the ring is kept from falling, then let go */
    {
        double r[2];
        for (int c = 0; c < 2; c++) {
            strut_t *s = fresh();
            float *p = s->pad[0].p;
            p[P_S_RING] = 0.5f, p[P_S_MODE] = MODE_BAND, p[P_S_CURVE] = c ? CURVE_HOLD : CURVE_NATURAL;
            const float T = skin_t60(p);
            hit(s, 2.0f);
            r[c] = window_rms((int)(0.4f * T * STRUT_SR), 1024) / window_rms(400, 1024);
            CHECK(!s->pad[0].voice.active, "Skin CURVE %d: dies away", c);
            free(s);
        }
        CHECK(20 * log10(r[1]) > -3 && 20 * log10(r[0]) < -15, "Skin Hold rings on (%+.1f dB at 0.4 of RING, Natural %+.1f)",
              20 * log10(r[1]), 20 * log10(r[0]));
    }

    /* every KIND on every destination of every engine, at full depth either
     * way: finite, heard, and it ends */
    const int first[3] = { P_S_KIND, P_W_KIND, P_N_KIND };
    for (int e = 0; e < 3; e++) {
        const param_def_t *aim = &STRUT_PAD_PARAMS[first[e] + 3];
        for (int k = 0; k < 4; k++)
            for (int a = 0; a < aim->noptions; a++)
                for (int d = -1; d <= 1; d += 2) {
                    strut_t *s = fresh();
                    float *p = s->pad[0].p;
                    p[P_SKIN] = 0.6f, p[P_WAVE] = 0.6f, p[P_NOISE] = 0.6f, p[P_N_DECAY] = 0.3f;
                    p[first[e]] = (float)k, p[first[e] + 1] = -0.3f, p[first[e] + 3] = (float)a, p[first[e] + 4] = (float)d;
                    const int n = hit(s, 2.0f);
                    int finite = 1;
                    for (int j = 0; j < n; j++) finite &= isfinite(L[j]);
                    const double pk = peak_of(L, 0, n);
                    char what[96];
                    snprintf(what, sizeof(what), "%s %s on %s, depth %+d", aim->name,
                             STRUT_PAD_PARAMS[first[e]].options[k], aim->options[a], d);
                    CHECK(finite && pk > 0.02 && pk < 2.0, "%s: finite and heard (peak %.3f)", what, pk);
                    /* Skin's RING pushed to full rings 4 s, past the 2 s heard */
                    if (!(e == 0 && a == 1 && d > 0)) CHECK(!s->pad[0].voice.active, "%s: dies away", what);
                    free(s);
                }
    }
}

/* One voice a pad: a new hit strikes the same Skin again, as a drum is
 * struck again, and restarts Wave without a click. */
static void restrike(void) {
    /* Skin: a second hit in step with the ring builds it, one against it
     * stops it, as on a real drum. 220.5 Hz: 200 samples a cycle exactly. */
    double once = 0, with = 0, against = 0;
    for (int c = 0; c < 3; c++) {
        strut_t *s = fresh();
        s->pad[0].p[P_S_PITCH] = 24;
        s->pad[0].p[P_S_RING] = 1.0f;
        s->pad[0].p[P_S_MODE] = 1;
        s->pad[0].p[P_S_SNAP] = 0;
        s->pad[0].p[P_TUNE] = 0;
        strut_note_on(s, STRUT_NOTE0, 100);
        const int second = c == 1 ? 2000 : 2100;     /* ten cycles, or ten and a half */
        play(s, 0, second);
        if (c) strut_note_on(s, STRUT_NOTE0, 100);
        play(s, second, 8000 - second);
        const double r = window_rms(6000, 2000);
        if (c == 0) once = r; else if (c == 1) with = r; else against = r;
        free(s);
    }
    CHECK(20 * log10(with / once) > 4, "a hit in step with the ring builds it (%+.1f dB)", 20 * log10(with / once));
    CHECK(20 * log10(against / once) < -6, "a hit against it stops it (%+.1f dB)", 20 * log10(against / once));

    /* a soft hit on a loud ring keeps the ring: velocity is in the hit */
    {
        strut_t *s = fresh();
        s->pad[0].p[P_S_RING] = 0.9f;
        strut_note_on(s, STRUT_NOTE0, 127);
        play(s, 0, 4410);
        const double before = window_rms(2205, 2205);
        strut_note_on(s, STRUT_NOTE0, 10);
        play(s, 4410, 4410);
        const double after = window_rms(4410 + 1000, 2205);
        CHECK(after > 0.5 * before, "a soft hit does not drop a loud ring (%.2f of it)", after / before);
        free(s);
    }

    /* Noise: a soft hit on loud noise adds to it, as two noises do, against
     * the same note left alone */
    {
        double r[2];
        for (int c = 0; c < 2; c++) {
            strut_t *s = noise_pad(NT_WHITE);
            s->pad[0].p[P_N_DECAY] = 0.9f;
            strut_note_on(s, STRUT_NOTE0, 127);
            play(s, 0, 8820);
            if (c) strut_note_on(s, STRUT_NOTE0, 20);
            play(s, 8820, 8820);
            r[c] = window_rms(8820, 4096);
            free(s);
        }
        CHECK(r[1] > 0.98 * r[0], "a soft hit does not drop loud noise (%.2f of it)", r[1] / r[0]);
    }

    /* Wave: a restarted note fades the old one out, so no step */
    {
        strut_t *s = wave_pad();
        s->pad[0].p[P_W_DECAY] = 1.0f;
        strut_note_on(s, STRUT_NOTE0, 100);
        play(s, 0, 5000);
        strut_note_on(s, STRUT_NOTE0, 100);
        play(s, 5000, 3000);
        double peak = 0, step = 0;
        for (int n = 1; n < 8000; n++) {
            peak = fmax(peak, fabs(L[n]));
            if (n > 4900 && n < 5600) step = fmax(step, fabs(L[n] - L[n - 1]));
        }
        CHECK(step < 0.1 * peak, "a restarted Wave note does not click (a step of %.3f of the peak)", step / peak);
        CHECK(s->pad[0].voice.old_n == 0, "and the old note is gone after its fade");
        free(s);
    }
}

/* The level knobs are faders: off at zero, then a few dB a tenth of a turn,
 * so a turn is always heard. */
static void levels(void) {
    const int keys[] = { P_SKIN, P_WAVE, P_NOISE, P_LEVEL };
    for (int k = 0; k < 4; k++) {
        double prev = 0;
        int even = 1;
        for (int i = 0; i <= 10; i++) {
            strut_t *s = keys[k] == P_WAVE ? wave_pad() : keys[k] == P_NOISE ? noise_pad(NT_WHITE) : fresh();
            s->pad[0].p[keys[k]] = (float)i / 10;
            const int n = hit(s, 0.3f);
            double peak = 0;
            for (int j = 0; j < n; j++) peak = fmax(peak, fabs(L[j]));
            if (i == 0) CHECK(peak == 0, "%s at zero is off", STRUT_PAD_PARAMS[keys[k]].key);
            if (i >= 2) {
                const double db = 20 * log10(peak / prev);
                even &= db > 2.0 && db < 4.0;
            }
            prev = peak;
            free(s);
        }
        CHECK(even, "%s: each tenth of a turn is 2 to 4 dB", STRUT_PAD_PARAMS[keys[k]].key);
    }
}

/* Through the real 16-bit output: a fading tail follows the float signal to
 * within a step and a half (the dither, no grit), and ends in true silence. */
static void tail(void) {
    void *p = A->create_instance(".", "");
    A->set_param(p, "space", "0");     /* the room's tail is not in ref */
    A->set_param(p, "p01_s_mode", "Band");
    A->set_param(p, "p01_s_ring", "0.7");
    strut_t *ref = fresh();
    ref->pad[0].p[P_S_MODE] = 1;
    ref->pad[0].p[P_S_RING] = 0.7f;
    midi3(p, 0x90, STRUT_NOTE0, 100);
    strut_note_on(ref, STRUT_NOTE0, 100);
    int16_t out[256];
    float l[128], r[128];
    /* the error through four one-poles at 2 kHz, where the ear is keen; the
     * shaped noise must sit well under plain dither's, made alongside */
    const double a = exp(-2 * 3.14159265358979 * 2000.0 / STRUT_SR);
    double worst = 0, lp[4] = { 0 }, pl[4] = { 0 }, lp2 = 0, pl2 = 0;
    uint32_t rs = 12345;
    int zeros = 0, blocks = 0, quiet = 0;
    for (; blocks < 4000; blocks++) {
        A->render_block(p, out, 128);
        strut_render(ref, l, r, 128);
        int all0 = 1;
        for (int i = 0; i < 128; i++) {
            if (fabs(l[i]) * STRUT_MAKEUP * 32000 < 40 && (out[2 * i] || out[2 * i + 1])) {
                const double e = out[2 * i] - l[i] * STRUT_MAKEUP * 32000;
                worst = fmax(worst, fabs(e));
                rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
                const double v = l[i] * STRUT_MAKEUP * 32000, tp = ((rs & 0xFFFF) + (rs >> 16)) / 65536.0 - 1;
                double x = e, y = rint(v + tp) - v;
                for (int k = 0; k < 4; k++) {
                    x = lp[k] = a * lp[k] + (1 - a) * x;
                    y = pl[k] = a * pl[k] + (1 - a) * y;
                }
                lp2 += x * x;
                pl2 += y * y;
                quiet++;
            }
            all0 &= out[2 * i] == 0 && out[2 * i + 1] == 0;
        }
        zeros = all0 ? zeros + 1 : 0;
        if (zeros > 50) break;
    }
    CHECK(quiet > 1000 && lp2 < 0.5 * pl2, "a quiet tail's noise below 2 kHz is over 3 dB under plain dither's (%.1f dB)",
          10 * log10(lp2 / (pl2 + 1e-12)));
    CHECK(worst <= 8, "and the shaper never runs away (%.1f steps at worst)", worst);
    CHECK(zeros > 50, "and ends in true silence, not hiss");
    free(ref);
    A->destroy_instance(p);
}

/* A beat through the Kit page: pad 1 every quarter second, loud and soft in
 * turn, for `seconds`, then silence to `total`; GLUE, WARM and the room
 * as set. Returns 1 if the kit still rang at the end. */
static int beat(strut_t *s, float seconds, float total) {
    const int n = (int)(total * STRUT_SR), hits = (int)(seconds * 4);
    s->pad[0].p[P_WAVE] = 0.6f, s->pad[0].p[P_NOISE] = 0.6f, s->pad[0].p[P_N_DECAY] = 0.3f;
    int at = 0, h = 0;
    for (; at < n; at += 128) {
        if (h < hits && at >= h * STRUT_SR / 4) strut_note_on(s, STRUT_NOTE0, h % 2 ? 60 : 120), h++;
        const int m = n - at < 128 ? n - at : 128;
        s->sounding = 0;
        strut_render(s, L + at, R + at, m);
        strut_kit(s, L + at, R + at, m);
    }
    return s->sounding > 0;
}

static double rms_of(const float *x, int from, int n) {
    double a = 0;
    for (int k = from; k < from + n; k++) a += (double)x[k] * x[k];
    return sqrt(a / n);
}

/* Every pad's SPACE, its send to the room. */
static void space_all(strut_t *s, float v) {
    for (int i = 0; i < STRUT_PADS; i++) s->pad[i].p[P_SPACE] = v;
}

/* The Kit page (DESIGN.md, How the Kit page works). */
static void kit(void) {
    const int n2 = 2 * STRUT_SR;
    /* dry: every kit knob at zero leaves the pads exactly as they were */
    strut_t *s = fresh();
    space_all(s, 0);
    beat(s, 2, 3);
    static float dl[3 * STRUT_SR];
    memcpy(dl, L, sizeof(dl));
    strut_t *t = fresh();
    t->pad[0].p[P_WAVE] = 0.6f, t->pad[0].p[P_NOISE] = 0.6f, t->pad[0].p[P_N_DECAY] = 0.3f;
    int same = 1;
    for (int at = 0, h = 0; at < 3 * STRUT_SR; at += 128) {
        if (h < 8 && at >= h * STRUT_SR / 4) strut_note_on(t, STRUT_NOTE0, h % 2 ? 60 : 120), h++;
        float l[128], r[128];
        strut_render(t, l, r, 128);
        for (int i = 0; i < 128 && at + i < 3 * STRUT_SR; i++) same &= l[i] == dl[at + i];
    }
    CHECK(same, "with SPACE, GLUE and WARM at zero the kit passes untouched");
    const double dry = rms_of(dl, 0, n2), dpk = peak_of(dl, 0, n2);
    free(s), free(t);

    /* every kit knob at its ends and middle: finite, under the limiter's
     * knee by a margin, and silent in the end */
    const int knobs[] = { -1, G_SIZE, G_GLUE, G_WARM };   /* -1: every pad's SPACE */
    int ok = 1;
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < 3; i++) {
            s = fresh();
            space_all(s, k ? 0.5f : (float)i / 2);
            if (k) s->g[knobs[k]] = (float)i / 2;
            beat(s, 1, 1);
            int rings = 1;
            for (int b = 0; b < 4000 && rings; b++) {
                float l[128] = { 0 }, r[128] = { 0 };
                s->sounding = 0;
                memset(s->send, 0, sizeof(s->send));
                strut_kit(s, l, r, 128);
                rings = s->sounding > 0;
            }
            int finite = 1;
            for (int j = 0; j < STRUT_SR; j++) finite &= isfinite(L[j]) && isfinite(R[j]);
            const double pk = fmax(peak_of(L, 0, STRUT_SR), peak_of(R, 0, STRUT_SR));
            if (!finite || pk > 1.0 || rings) {
                printf("  %s %.1f: finite %d peak %.2f rings %d\n", k ? STRUT_GLOBALS[knobs[k]].key : "space", i / 2.0, finite, pk, rings);
                ok = 0;
            }
            free(s);
        }
    CHECK(ok, "every kit knob at its ends and middle is finite, peaks under 1 and falls silent");

    /* SPACE: the room grows against the dry beat, to about 6 dB under it
     * full up, and rings on, wide, after the last hit */
    double prev = -100, wet = 0;
    int grows = 1;
    const int last = 2 * STRUT_SR - STRUT_SR / 4;     /* the beat's last hit */
    const double dtail = rms_of(dl, last + STRUT_SR / 2, STRUT_SR / 4);
    for (int i = 1; i <= 4; i++) {
        s = fresh();
        space_all(s, (float)i / 4);
        beat(s, 2, 3);
        double d = 0;
        for (int j = 0; j < n2; j++) d += (double)(L[j] - dl[j]) * (L[j] - dl[j]);
        wet = 10 * log10(d / n2) - 20 * log10(dry);
        grows &= wet > prev + 2;
        prev = wet;
        if (i == 4) {
            const double tail = rms_of(L, last + STRUT_SR / 2, STRUT_SR / 4);
            double diff = 0;
            for (int j = last + STRUT_SR / 2; j < last + 3 * STRUT_SR / 4; j++) diff += fabs(L[j] - R[j]);
            CHECK(tail > 10 * dtail && diff > 0.1 * tail * STRUT_SR / 4,
                  "SPACE rings on, wide, after the last hit (%.1f dB over the dry)", 20 * log10(tail / dtail));
        }
        free(s);
    }
    printf("kit: SPACE full: the room %.1f dB against the dry beat\n", wet);
    CHECK(grows && wet > -9 && wet < -3, "and grows to about 6 dB under the beat (%.1f dB)", wet);

    /* SIZE: the room rings longer as it grows, from under a second to
     * several: the time its answer to a click takes to fall 30 dB, from
     * 50 ms on */
    kit_t *k = calloc(1, sizeof(*k));
    float g[G_COUNT] = { 0 };
    static float x[4 * STRUT_SR], y[4 * STRUT_SR], sx[4 * STRUT_SR];
    double t30[3];
    for (int i = 0; i < 3; i++) {
        memset(k, 0, sizeof(*k));
        memset(x, 0, sizeof(x)), memset(y, 0, sizeof(y));
        memset(sx, 0, sizeof(sx));
        g[G_SIZE] = (float)i / 2;
        x[0] = y[0] = sx[0] = 0.5f;   /* SPACE full up */
        const int len = 4 * STRUT_SR, w = STRUT_SR / 50;
        for (int j = 0; j < len; j += 128) kit_run(k, g, sx + j, x + j, y + j, len - j < 128 ? len - j : 128);
        const double ref = rms_of(x, STRUT_SR / 20, w);
        int at = STRUT_SR / 20;
        while (at < len - w && rms_of(x, at, w) > ref * 0.0316) at += w / 2;
        t30[i] = (double)(at - STRUT_SR / 20) / STRUT_SR;
    }
    printf("kit: SIZE 0, 0.5, 1: the room falls 30 dB in %.2f, %.2f, %.2f s\n", t30[0], t30[1], t30[2]);
    CHECK(t30[0] < 0.6 && t30[1] > t30[0] * 1.5 && t30[2] > t30[1] * 1.5 && t30[2] > 1.5,
          "SIZE rings longer as it grows");

    /* GLUE: the soft hits come nearer the loud ones, while the beat's
     * loudness and peaks stay about where they were */
    s = fresh();
    s->g[G_GLUE] = 1;
    beat(s, 2, 2);
    const double grms = rms_of(L, 0, n2), gpk = peak_of(L, 0, n2);
    double gap[2] = { 0 }, body[2] = { 0 };
    for (int v = 0; v < 2; v++) {
        const float *z = v ? L : dl;
        double loud = 0, soft = 0, head = 0, rest = 0;
        for (int h = 0; h < 8; h++) {
            const int at = h * STRUT_SR / 4;
            const double e = rms_of(z, at, STRUT_SR / 4);
            if (h % 2) soft += e; else loud += e;
            head += rms_of(z, at, STRUT_SR / 40), rest += rms_of(z, at + STRUT_SR / 20, STRUT_SR / 10);
        }
        gap[v] = 20 * log10(loud / soft), body[v] = 20 * log10(rest / head);
    }
    printf("kit: dry beat peak %.3f rms %.3f; GLUE full: loud-soft gap %.1f -> %.1f dB, tail %.1f -> %.1f dB, level %+.1f, peak %+.1f dB\n",
           dpk, dry, gap[0], gap[1], body[0], body[1], 20 * log10(grms / dry), 20 * log10(gpk / dpk));
    CHECK(gap[0] - gap[1] > 2, "GLUE brings soft hits nearer loud ones (%.1f dB)", gap[0] - gap[1]);
    CHECK(fabs(20 * log10(grms / dry)) < 2, "and keeps the beat's level (%+.1f dB)", 20 * log10(grms / dry));
    CHECK(20 * log10(gpk / dpk) < 3, "and its peaks (%+.1f dB)", 20 * log10(gpk / dpk));
    free(s);

    /* WARM: on a 110 Hz sine at a typical level, even and odd harmonics
     * appear, the level holds, and nothing drifts off centre */
    memset(sx, 0, sizeof(sx));
    g[G_SIZE] = 0;
    for (int w = 0; w <= 1; w++) {
        memset(k, 0, sizeof(*k));
        g[G_WARM] = (float)w;
        for (int j = 0; j < STRUT_SR; j++) x[j] = y[j] = 0.3f * sinf(2 * 3.14159265f * 110 * j / STRUT_SR);
        for (int j = 0; j < STRUT_SR; j += 128) kit_run(k, g, sx + j, x + j, y + j, STRUT_SR - j < 128 ? STRUT_SR - j : 128);
        double h[4] = { 0 }, mean = 0;
        const int from = STRUT_SR / 2, len = STRUT_SR / 2;
        for (int m = 1; m <= 3; m++) {
            double c = 0, q = 0;
            for (int j = from; j < from + len; j++) {
                c += x[j] * cos(2 * 3.14159265358979 * 110 * m * j / STRUT_SR);
                q += x[j] * sin(2 * 3.14159265358979 * 110 * m * j / STRUT_SR);
            }
            h[m] = 2 * sqrt(c * c + q * q) / len;
        }
        for (int j = from; j < from + len; j++) mean += x[j];
        mean /= len;
        const double d2 = 20 * log10(h[2] / h[1] + 1e-12), d3 = 20 * log10(h[3] / h[1] + 1e-12);
        printf("kit: WARM %d: fundamental %.3f, second %.0f dB, third %.0f dB, offset %.5f\n", w, h[1], d2, d3, mean);
        if (w == 0) CHECK(d2 < -90 && d3 < -90, "WARM at zero adds nothing");
        else {
            CHECK(d2 > -40 && d3 > -40, "WARM adds even and odd harmonics (%.0f, %.0f dB)", d2, d3);
            CHECK(fabs(20 * log10(h[1] / 0.3)) < 2, "at about the same level (%+.1f dB)", 20 * log10(h[1] / 0.3));
            CHECK(fabs(mean) < 1e-3, "and stays centred (%.5f)", mean);
        }
    }
    free(k);
}

/* The loudest 400 ms of L's first n, as the library was matched (its
 * loudness, momentary). */
static double loudest(int n) {
    double best = 0;
    for (int at = 0; at + STRUT_SR * 2 / 5 <= n; at += STRUT_SR / 20) best = fmax(best, window_rms(at, STRUT_SR * 2 / 5));
    return best;
}

static int by_double(const void *a, const void *b) {
    const double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static struct timespec now_ts(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t;
}

static double since(struct timespec t0) {
    const struct timespec t = now_ts();
    return (double)(t.tv_sec - t0.tv_sec) + 1e-9 * (double)(t.tv_nsec - t0.tv_nsec);
}

/* The library's entry called name, as TABLE's index; -1 none. */
static int table_of(const char *name) {
    for (int i = 0; i < smp_count(); i++)
        if (!strcmp(smp_name(i), name)) return NT_TABLES + i;
    return -1;
}

/* Pad 1 on Noise alone, TABLE at t, loaded as the loader would. */
static strut_t *sample_pad(int t) {
    strut_t *s = fresh();
    s->pad[0].p[P_SKIN] = 0, s->pad[0].p[P_NOISE] = 0.8f, s->pad[0].p[P_N_TABLE] = (float)t;
    s->pad[0].p[P_N_DECAY] = 1;
    float l[128], r[128];
    strut_render(s, l, r, 128);     /* the pad says what it wants */
    smp_service(&s->lib, 0);
    return s;
}

/* The last sample above a whisper, from `from`. */
static int last_heard(const float *x, int from, int n) {
    int at = from;
    for (int k = from; k < from + n; k++) if (fabs(x[k]) > 1e-4) at = k;
    return at;
}

/* The sample library and Noise's sample mode (DESIGN.md, The sample library). */
static void samples(void) {
    CHECK(smp_count() == 208, "the library lists 208 sounds (%d)", smp_count());
    CHECK(!strcmp(smp_name(0), "Kick 001") && param_noptions(&STRUT_PAD_PARAMS[P_N_TABLE]) == NT_TABLES + smp_count(),
          "TABLE lists the noise tables, then the library, drums first");
    /* every file reads, a one-shot at 44.1 kHz and at most 4 s, every
     * cycle one period of 2048 samples; and every one loads */
    int bad = 0, cycles = 0, loaded = 0;
    for (int i = 0; i < smp_count(); i++) {
        float *x;
        int n, rate;
        const int ok = smp_read_wav(smp_path(i), &x, &n, &rate);
        const int cycle = !strncmp(smp_name(i), "Cycle", 5);
        if (!ok || rate != 44100 || (cycle ? n != 2048 : n > 4 * 44100)) {
            printf("  %s: read %d, %d frames at %d Hz\n", smp_name(i), ok, n, rate);
            bad++;
        }
        cycles += cycle && n == 2048;
        free(x);
        smp_t *sm = smp_load(i);
        loaded += sm && sm->cycle == cycle;
        smp_free(sm);
    }
    CHECK(bad == 0, "every file reads: one-shots at 44.1 kHz under 4 s, cycles 2048 samples (%d not)", bad);
    CHECK(cycles == 36 && loaded == smp_count(), "and every one loads (%d of %d; %d cycles)", loaded, smp_count(), cycles);

    /* named by its name, kept, and served back */
    void *p = A->create_instance(".", "");
    A->set_param(p, "p01_n_table", "Snare 001");
    CHECK(!strcmp(get(p, "p01_n_table"), "Snare 001"), "TABLE takes a sample by name");
    A->destroy_instance(p);

    /* nothing plays until it has loaded */
    const int kick = table_of("Kick 001");
    strut_t *s = fresh();
    s->pad[0].p[P_SKIN] = 0, s->pad[0].p[P_NOISE] = 0.8f, s->pad[0].p[P_N_TABLE] = (float)kick;
    int n = hit(s, 0.2f);
    CHECK(peak_of(L, 0, n) == 0, "a sample not yet loaded plays nothing");
    free(s);

    /* every sound at its knobs' defaults: sounds, finite, under full
     * scale, and ends; and how loud they are against each other */
    double lo = 1e9, hi = 0, ok = 1, top = 0;
    static double louds[SM_FILES];
    int nl = 0;
    for (int i = 0; i < smp_count(); i++) {
        s = sample_pad(NT_TABLES + i);
        n = hit(s, 4.0f);
        int finite = 1;
        for (int j = 0; j < n; j++) finite &= isfinite(L[j]);
        const double pk = peak_of(L, 0, n), loud = loudest(n);
        const int cycle = !strncmp(smp_name(i), "Cycle", 5);
        if (!finite || pk > 0.95 || pk < 0.01 || (!cycle && s->pad[0].voice.noise.env > 0)) {
            printf("  %s: finite %d peak %.3f still %d\n", smp_name(i), finite, pk, s->pad[0].voice.noise.env > 0);
            ok = 0;
        }
        if (!cycle) lo = fmin(lo, loud), hi = fmax(hi, loud), louds[nl++] = loud, top = fmax(top, pk);
        free(s);
    }
    s = noise_pad(NT_WHITE);
    const double white = loudest(hit(s, 1.0f));
    free(s);
    qsort(louds, (size_t)nl, sizeof(double), by_double);
    printf("samples: loudest 400 ms %.1f .. %.1f dB against White's, half of them over %.1f; peaks up to %.2f\n",
           20 * log10(lo / white), 20 * log10(hi / white), 20 * log10(louds[nl / 2] / white), top);
    CHECK(ok, "every sound sounds, stays finite, peaks under 0.95 and ends");

    /* PITCH +12 plays it in half the time; START half way, half of it */
    const int snare = table_of("Snare 001");
    s = sample_pad(snare);
    n = hit(s, 3.0f);
    const int whole = last_heard(L, 0, n);
    free(s);
    s = sample_pad(snare);
    s->pad[0].p[P_N_PITCH] = 12;
    n = hit(s, 3.0f);
    const int up = last_heard(L, 0, n);
    free(s);
    s = sample_pad(snare);
    s->pad[0].p[P_N_START] = 0.5f;
    n = hit(s, 3.0f);
    const int half = last_heard(L, 0, n);
    free(s);
    printf("samples: Snare 001 lasts %d samples, %d an octave up, %d from half way\n", whole, up, half);
    CHECK(fabs((double)up / whole - 0.5) < 0.05, "PITCH +12 plays a sample in half the time");
    CHECK(fabs((double)half / whole - 0.5) < 0.05, "START half way plays its second half");

    /* LOOP repeats a slice: the sound goes on past the sample's end, and
     * what is heard one loop apart is the same */
    s = sample_pad(snare);
    s->pad[0].p[P_N_LOOP] = 0.6f, s->pad[0].p[P_N_START] = 0.1f, s->pad[0].p[P_N_DECAY] = 0.8f;
    n = hit(s, 3.0f);
    const smp_t *sm = s->pad[0].voice.noise.smp;
    const double rest = 0.9 * sm->len / 2, shortest = 0.002 * 44100;
    const int loop = (int)(shortest * pow(rest / shortest, 0.6));
    double e = 0, d = 0;
    for (int j = whole; j < whole + STRUT_SR / 4; j++) e += L[j] * L[j], d += (L[j] - L[j - loop]) * (L[j] - L[j - loop]);
    printf("samples: a loop of %d samples, %.1f dB the same one loop apart\n", loop, 10 * log10(d / e));
    CHECK(e > 0 && d < 0.05 * e, "LOOP repeats a slice past the sample's end");
    free(s);

    /* a cycle is looped and pitched from A1: 55 Hz at PITCH 0 */
    s = sample_pad(table_of("Cycle 001"));
    s->pad[0].p[P_N_DECAY] = 0.8f;
    n = hit(s, 1.0f);
    int best = 0;
    double bc = -1e9;
    for (int lag = 600; lag < 1200; lag++) {
        double c = 0;
        for (int j = STRUT_SR / 10; j < STRUT_SR / 10 + 4000; j++) c += L[j] * L[j + lag];
        if (c > bc) bc = c, best = lag;
    }
    CHECK(fabs(44100.0 / best - 55) < 0.5, "a cycle plays at 55 Hz at PITCH 0 (%.2f Hz)", 44100.0 / best);
    CHECK(window_rms(STRUT_SR / 2, 1000) > 1e-3, "and keeps going: it loops");
    free(s);

    /* the loader keeps a sample while a voice plays it, and lets go of
     * what nobody uses once it holds too much */
    s = sample_pad(kick);
    strut_note_on(s, STRUT_NOTE0, 100);
    const smp_t *playing = s->pad[0].voice.noise.smp;
    float l[128], r[128];
    int kept = 1;
    for (int i = 1; i < 120; i++) {
        s->pad[0].p[P_N_TABLE] = (float)(NT_TABLES + i);
        strut_render(s, l, r, 128);
        smp_service(&s->lib, 0);
        strut_render(s, l, r, 128);
        int found = 0;
        for (const smp_t *c = s->lib.cache; c; c = c->next) found |= c == playing;
        kept &= found || !s->pad[0].voice.active;
    }
    printf("samples: the loader holds %.1f MB after 120 sounds\n", s->lib.cached / 1048576.0);
    CHECK(kept, "a sample a voice plays is never let go");
    CHECK(s->lib.cached < (20u << 20), "and what nobody uses is let go past 16 MB");
    smp_stop(&s->lib);
    free(s);

    /* the loader's own thread, as on the Move: a pad's sample arrives
     * while blocks render */
    p = A->create_instance(".", "");
    A->set_param(p, "p01_n_table", "Clap 001");
    A->set_param(p, "p01_skin", "0");
    A->set_param(p, "p01_noise", "0.8");
    int16_t out[256];
    const smp_t *ready = NULL;
    strut_t *st = p;
    for (int i = 0; i < 400 && !ready; i++) {
        A->render_block(p, out, 128);
        const struct timespec ms = { 0, 5000000 };
        nanosleep(&ms, NULL);
        ready = __atomic_load_n(&st->lib.ready[0], __ATOMIC_ACQUIRE);
    }
    CHECK(ready != NULL, "the loader's thread loads a pad's sample");
    midi3(p, 0x90, STRUT_NOTE0, 100);
    int peak = 0;
    rms(p, 40, &peak);
    CHECK(peak > 1000, "and the pad plays it (peak %d)", peak);
    A->destroy_instance(p);
}

/* Pad 1 on a sample in MODE m, made as the loader would. */
static strut_t *mode_pad(int t, int m) {
    strut_t *s = sample_pad(t);
    s->pad[0].p[P_N_MODE] = (float)m;
    if (m == SM_RESYNTH) s->pad[0].p[P_N_DECAY] = 0.5f;     /* its own length */
    float l[128], r[128];
    strut_render(s, l, r, 128);
    smp_service(&s->lib, 0);
    return s;
}

/* How much x repeats itself: the best of its normalised self-likeness at
 * lags of 0.5 to 20 ms, over n samples from `from`. */
static double tonal(const float *x, int from, int n) {
    double e0 = 0, best = 0;
    for (int j = from; j < from + n; j++) e0 += (double)x[j] * x[j];
    for (int lag = 22; lag < 882; lag++) {
        double c = 0, e1 = 0;
        for (int j = from; j < from + n; j++) c += (double)x[j] * x[j + lag], e1 += (double)x[j + lag] * x[j + lag];
        if (e0 > 0 && e1 > 0) best = fmax(best, c / sqrt(e0 * e1));
    }
    return best;
}

/* The loudest frequency, 40 Hz to 4 kHz to the nearest hertz, in n
 * samples of x from `from` through a Hann window. */
static double loudest_hz(const float *x, int from, int n) {
    double best = 0, bp = -1;
    for (int f = 40; f < 4000; f++) {
        double c = 0, d = 0;
        for (int j = 0; j < n; j++) {
            const double w = (0.5 - 0.5 * cos(2 * 3.14159265358979 * j / n)) * x[from + j], ph = 2 * 3.14159265358979 * f * j / STRUT_SR;
            c += w * cos(ph), d += w * sin(ph);
        }
        if (c * c + d * d > bp) bp = c * c + d * d, best = f;
    }
    return best;
}

/* Noise's MODE: Resynth and Noise (DESIGN.md, Noise). */
static void modes(void) {
    /* every one-shot in both: sounds, finite, under full scale, ends, and
     * about as loud as the sample played straight */
    static double dr[SM_FILES], dn[SM_FILES];
    int nr = 0, ok = 1;
    double slow = 0;
    size_t extra = 0;
    for (int i = 0; i < smp_count(); i++) {
        if (!strncmp(smp_name(i), "Cycle", 5)) continue;
        strut_t *s = mode_pad(NT_TABLES + i, SM_SAMPLE);
        int n = hit(s, 4.0f);
        const double straight = loudest(n);
        free(s);
        for (int m = SM_RESYNTH; m <= SM_NOISE; m++) {
            s = sample_pad(NT_TABLES + i);
            s->pad[0].p[P_N_MODE] = (float)m;
            if (m == SM_RESYNTH) s->pad[0].p[P_N_DECAY] = 0.5f;
            float l[128], r[128];
            strut_render(s, l, r, 128);
            const struct timespec t0 = now_ts();
            smp_service(&s->lib, 0);
            slow = fmax(slow, since(t0));
            const smp_t *sm = s->lib.ready[0];
            if (m == SM_RESYNTH) extra = sm->bytes > extra ? sm->bytes : extra;
            n = hit(s, 4.0f);
            int finite = 1;
            for (int j = 0; j < n; j++) finite &= isfinite(L[j]);
            const double pk = peak_of(L, 0, n), loud = loudest(n);
            const int made = s->pad[0].voice.noise.mode == m;
            if (!made || !finite || pk > 0.95 || pk < 0.005 || s->pad[0].voice.noise.env > 0) {
                printf("  %s %s: made %d finite %d peak %.3f still %d\n", smp_name(i), m == SM_RESYNTH ? "Resynth" : "Noise",
                       made, finite, pk, s->pad[0].voice.noise.env > 0);
                ok = 0;
            }
            (m == SM_RESYNTH ? dr : dn)[nr] = 20 * log10(loud / straight);
            if (fabs(20 * log10(loud / straight)) > 3) printf("  %s %d: %.1f dB\n", smp_name(i), m, 20 * log10(loud / straight));
            free(s);
        }
        nr++;
    }
    double r0 = 1e9, r1 = -1e9, n0 = 1e9, n1 = -1e9;
    for (int i = 0; i < nr; i++) r0 = fmin(r0, dr[i]), r1 = fmax(r1, dr[i]), n0 = fmin(n0, dn[i]), n1 = fmax(n1, dn[i]);
    qsort(dr, (size_t)nr, sizeof(double), by_double);
    qsort(dn, (size_t)nr, sizeof(double), by_double);
    printf("modes: against Sample, Resynth %.1f .. %.1f dB (half over %.1f), Noise %.1f .. %.1f dB (half over %.1f)\n",
           r0, r1, dr[nr / 2], n0, n1, dn[nr / 2]);
    printf("modes: the slowest to make took %.0f ms here; a sample with Resynth holds up to %.1f MB\n", 1000 * slow, extra / 1048576.0);
    CHECK(ok, "every one-shot in Resynth and Noise sounds, stays finite, peaks under 0.95 and ends");
    CHECK(fabs(dr[nr / 2]) < 1.5 && r0 > -6 && r1 < 6, "Resynth is about as loud as the sample (%.1f .. %.1f dB)", r0, r1);
    CHECK(fabs(dn[nr / 2]) < 1.5 && n0 > -6 && n1 < 6, "and so is Noise (%.1f .. %.1f dB)", n0, n1);

    /* Resynth keeps a tune: a bass note sounds at its pitch, PITCH moves it
     * and not its length, DECAY its length and not its pitch */
    const int bass = table_of("Bass 007");
    strut_t *s = mode_pad(bass, SM_SAMPLE);
    int n = hit(s, 2.0f);
    const int whole = last_heard(L, 0, n);
    const double f_sample = loudest_hz(L, 2000, 4096);
    free(s);
    s = mode_pad(bass, SM_RESYNTH);
    n = hit(s, 2.0f);
    const int own = last_heard(L, 0, n);
    const double f_rs = loudest_hz(L, 2000, 4096);
    free(s);
    s = mode_pad(bass, SM_RESYNTH);
    s->pad[0].p[P_N_PITCH] = 12;
    n = hit(s, 2.0f);
    const int up_len = last_heard(L, 0, n);
    const double f_up = loudest_hz(L, 2000, 4096);
    free(s);
    s = mode_pad(bass, SM_RESYNTH);
    s->pad[0].p[P_N_DECAY] = 1.0f;
    n = hit(s, 3.0f);
    const int long_len = last_heard(L, 0, n);
    const double f_long = loudest_hz(L, 8000, 4096);
    free(s);
    s = mode_pad(bass, SM_RESYNTH);
    s->pad[0].p[P_N_DECAY] = 0.0f;
    n = hit(s, 2.0f);
    const int short_len = last_heard(L, 0, n);
    free(s);
    printf("modes: Bass 007 lasts %d samples, Resynth %d; +12 %d; DECAY 1 %d, 0 %d\n", whole, own, up_len, long_len, short_len);
    printf("modes: loudest at %.0f Hz, Resynth %.0f, +12 %.0f, DECAY 1 %.0f\n", f_sample, f_rs, f_up, f_long);
    CHECK(fabs((double)own / whole - 1) < 0.1, "Resynth at DECAY's centre lasts as the sample does");
    CHECK(fabs(f_rs / f_sample - 1) < 0.02, "and sounds at its pitch");
    CHECK(fabs((double)up_len / own - 1) < 0.1 && fabs(f_up / f_rs - 2) < 0.04, "PITCH +12 an octave up, as long");
    CHECK(fabs((double)long_len / own - 4) < 0.4 && fabs(f_long / f_rs - 1) < 0.02, "DECAY fully right four times as long, at its pitch");
    CHECK(fabs((double)short_len / own - 0.25) < 0.1, "DECAY 0 a quarter");

    /* Noise mode takes the pitch out */
    const int mallet = table_of("Mallet 003");
    s = mode_pad(mallet, SM_SAMPLE);
    n = hit(s, 1.0f);
    const double t_sample = tonal(L, 2000, 4000);
    free(s);
    s = mode_pad(mallet, SM_NOISE);
    n = hit(s, 1.0f);
    const double t_noise = tonal(L, 2000, 4000);
    free(s);
    printf("modes: Mallet 003 repeats itself %.2f, as Noise %.2f\n", t_sample, t_noise);
    CHECK(t_noise < 0.5 * t_sample, "Noise mode takes the pitch out");

    /* LOOP in Resynth holds a moment as a drone */
    s = mode_pad(bass, SM_RESYNTH);
    s->pad[0].p[P_N_LOOP] = 0.0f, s->pad[0].p[P_N_START] = 0.1f;
    n = hit(s, 4.0f);
    const double a = window_rms(STRUT_SR, 4410), b = window_rms(3 * STRUT_SR, 4410);
    printf("modes: a held moment at 1 s %.4f, at 3 s %.4f\n", a, b);
    CHECK(a > 1e-3 && fabs(20 * log10(b / a)) < 1.5 && s->pad[0].voice.active, "LOOP in Resynth holds a moment still");
    free(s);

    /* MODE plays Sample until the rest is made; a cycle is always Sample */
    s = sample_pad(bass);
    s->pad[0].p[P_N_MODE] = SM_RESYNTH;
    strut_note_on(s, STRUT_NOTE0, 100);
    CHECK(s->pad[0].voice.noise.mode == SM_SAMPLE, "Resynth not yet made plays the sample");
    free(s);
    s = mode_pad(table_of("Cycle 001"), SM_RESYNTH);
    strut_note_on(s, STRUT_NOTE0, 100);
    CHECK(s->pad[0].voice.noise.mode == SM_SAMPLE && !s->lib.ready[0]->has, "a cycle plays as itself in every MODE");
    free(s);
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

/* ---- DICE (DESIGN.md, Sounds, presets and the randomiser) ---- */

static int same(const float *a, const float *b, int n) { return !memcmp(a, b, sizeof(float) * (size_t)n); }

/* Hits pad i with its knobs as set, its sample loaded first, and renders
 * seconds of it into L. */
static int hit_pad(strut_t *s, int i, float seconds) {
    strut_render(s, L, R, 128);     /* the pad says what it wants */
    smp_service(&s->lib, 0);
    strut_note_on(s, STRUT_NOTE0 + i, 100);
    const int total = (int)(seconds * STRUT_SR);
    for (int n = 0; n < total; n += 128) strut_render(s, L + n, R + n, total - n < 128 ? total - n : 128);
    return total;
}

/* The table DICE levels the library by (levels.h) is the library's own. */
static void sample_levels(void) {
    int stale = 0, listed = 0;
    for (int i = 0; i < smp_count(); i++) {
        if (!strncmp(smp_name(i), "Cycle ", 6)) continue;
        listed++;
        float pk, listed_pk;
        const float db = levels_measure(NT_TABLES + i, L, R, &pk), was = levels_of(smp_name(i), &listed_pk);
        if (fabsf(db - was) > 0.15f || fabsf(pk - listed_pk) > 0.15f) {
            if (!stale++) printf("  %s plays at %.1f dB, peak %.1f; listed %.1f, %.1f\n", smp_name(i),
                                 (double)db, (double)pk, (double)was, (double)listed_pk);
        }
    }
    CHECK(listed == LEVELS_N && !stale, "every library sample's level is listed as it plays (%d of %d off, %d listed;"
          " remake: tools/levels > src/dsp/levels.c)", stale, listed, LEVELS_N);
}

static void dice(void) {
    void *p = A->create_instance(".", "");
    strut_t *s = p;
    float s0[P_COUNT], s1[P_COUNT], s2[P_COUNT], s3[P_COUNT];
    memcpy(s0, s->pad[1].p, sizeof(s0));
    A->set_param(p, "p02_dice", "Roll");
    memcpy(s1, s->pad[1].p, sizeof(s1));
    A->set_param(p, "p02_dice", "Roll");
    memcpy(s2, s->pad[1].p, sizeof(s2));
    CHECK(!same(s0, s1, P_DICE) && !same(s1, s2, P_DICE), "Roll makes a new sound each time");
    CHECK(!strcmp(get(p, "p02_dice"), "Roll"), "DICE shows the way it last went");
    CHECK(same(s->pad[0].p, s->pad[2].p, P_COUNT), "and leaves the other pads alone");
    A->set_param(p, "p02_dice", "Back");
    CHECK(same(s->pad[1].p, s1, P_DICE) && !strcmp(get(p, "p02_dice"), "Back"), "Back steps to the roll before");
    A->set_param(p, "p02_dice", "1");
    CHECK(same(s->pad[1].p, s2, P_DICE), "Roll after Back goes forward again, not to a new roll");
    A->set_param(p, "p02_dice", "Back");
    A->set_param(p, "p02_dice", "Back");
    CHECK(same(s->pad[1].p, s0, P_DICE), "and back to the sound before any roll");
    A->set_param(p, "p02_dice", "Back");
    CHECK(same(s->pad[1].p, s0, P_DICE), "which is as far as it goes");
    A->set_param(p, "p02_dice", "Roll");
    A->set_param(p, "p02_dice", "Roll");
    A->set_param(p, "p02_dice", "Roll");
    memcpy(s3, s->pad[1].p, sizeof(s3));
    CHECK(!same(s3, s2, P_DICE), "past the newest, a new roll");
    A->set_param(p, "p02_tune", "5");
    A->set_param(p, "p02_dice", "Back");
    A->set_param(p, "p02_dice", "Roll");
    CHECK(s->pad[1].p[P_TUNE] == 5.0f, "an edit after a roll survives a step back and forward");
    /* eight back at most */
    float kept[DICE_SLOTS][P_COUNT];
    for (int k = 0; k < 12; k++) A->set_param(p, "p02_dice", "Roll");
    memcpy(kept[0], s->pad[1].p, sizeof(kept[0]));
    for (int k = 1; k < DICE_SLOTS; k++) {
        A->set_param(p, "p02_dice", "Back");
        memcpy(kept[k], s->pad[1].p, sizeof(kept[k]));
    }
    A->set_param(p, "p02_dice", "Back");
    CHECK(same(s->pad[1].p, kept[DICE_KEEP], P_DICE), "eight steps back, and no further");
    int walk = 1;
    for (int k = DICE_KEEP - 1; k >= 0; k--) {
        A->set_param(p, "p02_dice", "Roll");
        walk &= same(s->pad[1].p, kept[k], P_DICE);
    }
    CHECK(walk, "and forward through the same eight");
    /* a pad's roll keeps its place in the mix */
    A->set_param(p, "p02_level", "0.5");
    A->set_param(p, "p02_pan", "-0.4");
    A->set_param(p, "p02_choke", "C");
    A->set_param(p, "p02_dice", "Roll");
    CHECK(s->pad[1].p[P_LEVEL] == 0.5f && s->pad[1].p[P_PAN] == -0.4f && s->pad[1].p[P_CHOKE] == 3.0f,
          "a pad's roll keeps its LEVEL, PAN and CHOKE");

    /* the kit */
    float before[STRUT_PADS][P_COUNT], after[STRUT_PADS][P_COUNT];
    for (int i = 0; i < STRUT_PADS; i++) memcpy(before[i], s->pad[i].p, sizeof(before[i]));
    A->set_param(p, "kit_dice", "Roll");
    int changed = 0;
    for (int i = 0; i < STRUT_PADS; i++) changed += !same(before[i], s->pad[i].p, P_DICE);
    CHECK(changed == STRUT_PADS, "Kit > DICE rolls all sixteen (%d)", changed);
    CHECK(s->pad[2].p[P_CHOKE] == 1.0f && s->pad[3].p[P_CHOKE] == 1.0f && s->pad[0].p[P_CHOKE] == 0.0f,
          "the hats choke each other, and nothing else does");
    CHECK(s->pad[1].p[P_LEVEL] == STRUT_PAD_PARAMS[P_LEVEL].def, "the kit's roll sets each pad's level");
    for (int i = 0; i < STRUT_PADS; i++) memcpy(after[i], s->pad[i].p, sizeof(after[i]));
    A->set_param(p, "p05_dice", "Back");
    CHECK(same(s->pad[4].p, after[4], P_DICE), "a pad's own rolls start again from the kit's");
    A->set_param(p, "kit_dice", "Back");
    int back = 0;
    for (int i = 0; i < STRUT_PADS; i++) back += same(before[i], s->pad[i].p, P_DICE);
    CHECK(back == STRUT_PADS, "Kit > DICE Back brings every pad back (%d)", back);
    A->set_param(p, "kit_dice", "Roll");
    back = 0;
    for (int i = 0; i < STRUT_PADS; i++) back += same(after[i], s->pad[i].p, P_DICE);
    CHECK(back == STRUT_PADS, "and Roll forward again (%d)", back);
    A->destroy_instance(p);

    /* Every role's rolls: they sound, stay finite, peak under 0.95 and end
     * in their role's time; each role's are near each other in loudness. */
    static const float ENDS[ROLE_COUNT] = { 2.0f, 1.5f, 1.2f, 0.4f, 1.8f, 3.9f, 0.6f, 2.5f, 2.0f, 3.5f, 3.0f, 3.9f };
    const int rolls = 40;
    for (int i = 0; i < STRUT_PADS; i++) {
        const dice_role_t role = dice_role(i);
        double loud[64], lo_pk = 1, hi_pk = 0;
        int bad = 0, quiet = 0, long_ = 0, worst = 0;
        uint32_t rng = 0x1234567u + (uint32_t)i;
        for (int k = 0; k < rolls; k++) {
            strut_t *t = fresh();
            dice_roll(t->pad[i].p, i, 1, &rng);
            const int n = hit_pad(t, i, 4.0f);
            double pk = 0;
            int finite = 1;
            for (int m = 0; m < n; m++) {
                finite &= isfinite(L[m]) && isfinite(R[m]);
                pk = fmax(pk, fmax(fabs(L[m]), fabs(R[m])));
            }
            for (int m = 0; m < n; m++) L[m] = 0.5f * (L[m] + R[m]);
            const int end = last_heard(L, 0, n);
            loud[k] = 20 * log10(fmax(loudest(n), 1e-9));
            bad += !finite || pk >= 0.95;
            quiet += pk < 0.05;
            if (end > (int)(ENDS[role] * STRUT_SR)) long_++, worst = end > worst ? end : worst;
            lo_pk = fmin(lo_pk, pk), hi_pk = fmax(hi_pk, pk);
            smp_stop(&t->lib);
            free(t);
        }
        qsort(loud, (size_t)rolls, sizeof(double), by_double);
        CHECK(!bad && !quiet, "pad %d (%s): every roll sounds, finite, under 0.95 (%d bad, %d quiet)",
              i + 1, dice_role_name(role), bad, quiet);
        CHECK(!long_, "pad %d (%s): every roll ends within %.1f s (%d over, the longest %.2f s)",
              i + 1, dice_role_name(role), (double)ENDS[role], long_, (double)worst / STRUT_SR);
        CHECK(loud[3 * rolls / 4] - loud[rolls / 4] <= 5.0 && loud[rolls - 1] - loud[0] <= 15.0,
              "pad %d (%s): rolls level-matched, the middle half within 5 dB and all within 15 (%.1f, %.1f)",
              i + 1, dice_role_name(role), loud[3 * rolls / 4] - loud[rolls / 4], loud[rolls - 1] - loud[0]);
        printf("dice: pad %2d %-6s peaks %.2f .. %.2f, loudness %.1f .. %.1f dB (middle half %.1f .. %.1f)\n",
               i + 1, dice_role_name(role), lo_pk, hi_pk, loud[0], loud[rolls - 1], loud[rolls / 4], loud[3 * rolls / 4]);
    }
}

/* A pass through the kit as it stands: each pad once, 0.2 s apart, through
 * the Kit page, its samples loaded first. The loudness over all of it in
 * dB; *peak its peak. */
static double kit_pass(strut_t *s, double *peak) {
    strut_render(s, L, R, 128);
    for (int k = 0; k < 4; k++) smp_service(&s->lib, 0);
    const int n = STRUT_SR * 4, gap = STRUT_SR / 5;
    double pk = 0;
    for (int at = 0, h = 0; at < n; at += 128) {
        if (h < STRUT_PADS && at >= h * gap) strut_note_on(s, STRUT_NOTE0 + h, 100), h++;
        const int m = n - at < 128 ? n - at : 128;
        strut_render(s, L + at, R + at, m);
        strut_kit(s, L + at, R + at, m);
    }
    for (int k = 0; k < n; k++) pk = fmax(pk, fmax(fabs(L[k]), fabs(R[k])));
    *peak = pk;
    return 20 * log10(fmax(rms_of(L, 0, n), 1e-9));
}

/* Pad > SOUND and Kit > KIT: fixed sounds and kits, each one step back for
 * DICE however far the knob turned, and never loaded by a saved kit. */
static void picks(void) {
    void *p = A->create_instance(".", "");
    strut_t *s = p;
    float s0[P_COUNT], s1[P_COUNT];
    CHECK(!strcmp(get(p, "p02_sound"), "Own"), "SOUND starts on Own, the pad's own sound");
    A->set_param(p, "p02_level", "0.5");
    memcpy(s0, s->pad[1].p, sizeof(s0));
    A->set_param(p, "p02_sound", "Kick 1");
    A->set_param(p, "pad_press", "1");
    A->set_param(p, "p02_sound", "Kick 2");
    A->set_param(p, "p02_sound", "Kick 3");
    memcpy(s1, s->pad[1].p, sizeof(s1));
    CHECK(!same(s0, s1, P_DICE) && !strcmp(get(p, "p02_sound"), "Kick 3") && s1[P_LEVEL] == 0.5f,
          "SOUND puts a sound on the pad, names it, and keeps its LEVEL");
    A->set_param(p, "p05_sound", "Kick 3");
    CHECK(same(s->pad[4].p + 1, s1 + 1, P_LEVEL - 1), "a SOUND is the same sound on any pad");
    A->set_param(p, "p02_dice", "Back");
    CHECK(same(s->pad[1].p, s0, P_DICE), "one Finish > DICE Back undoes a whole turn of SOUND");
    A->set_param(p, "p02_dice", "Roll");
    CHECK(same(s->pad[1].p, s1, P_DICE), "and Roll brings the pick back");
    A->set_param(p, "p02_dice", "Roll");
    CHECK(!strcmp(get(p, "p02_sound"), "Own"), "a roll is the pad's own sound again");

    float before[STRUT_PADS][P_COUNT], after[STRUT_PADS][P_COUNT];
    for (int i = 0; i < STRUT_PADS; i++) memcpy(before[i], s->pad[i].p, sizeof(before[i]));
    A->set_param(p, "kit", "Dust");
    A->set_param(p, "kit", "Hall");
    int changed = 0;
    for (int i = 0; i < STRUT_PADS; i++) changed += !same(before[i], s->pad[i].p, P_DICE);
    CHECK(changed == STRUT_PADS && !strcmp(get(p, "kit"), "Hall") && s->g[G_SIZE] > 0.8f,
          "KIT loads all sixteen pads and the Kit page (%d)", changed);
    for (int i = 0; i < STRUT_PADS; i++) memcpy(after[i], s->pad[i].p, sizeof(after[i]));
    A->set_param(p, "kit_dice", "Back");
    int back = 0;
    for (int i = 0; i < STRUT_PADS; i++) back += same(before[i], s->pad[i].p, P_DICE);
    CHECK(back == STRUT_PADS, "one Kit > DICE Back brings the kit before (%d)", back);
    A->set_param(p, "kit", "Hall");
    A->set_param(p, "p03_tune", "3");
    static char buf[131072];
    A->get_param(p, "state", buf, sizeof(buf));
    void *q = A->create_instance(".", "");
    A->set_param(q, "state", buf);
    CHECK(!strcmp(get(q, "kit"), "Hall") && ((strut_t *)q)->pad[2].p[P_TUNE] == 3.0f,
          "a saved kit names its KIT and loads as saved, not as the factory kit");
    A->destroy_instance(q);
    A->destroy_instance(p);

    /* every SOUND plays, finite and under 0.95 */
    int bad = 0, quiet = 0;
    double lo = 99, hi = -99;
    for (int n = 1; n < DICE_SOUNDS; n++) {
        strut_t *t = fresh();
        dice_sound(t->pad[0].p, n);
        const int m = hit_pad(t, 0, 4.0f);
        double pk = 0;
        int finite = 1;
        for (int k = 0; k < m; k++) {
            finite &= isfinite(L[k]) && isfinite(R[k]);
            pk = fmax(pk, fmax(fabs(L[k]), fabs(R[k])));
        }
        for (int k = 0; k < m; k++) L[k] = 0.5f * (L[k] + R[k]);
        const double db = 20 * log10(fmax(loudest(m), 1e-9));
        if (!finite || pk >= 0.95) bad++, printf("  %s: peak %.2f\n", DICE_SOUND_NAMES[n], pk);
        if (pk < 0.05) quiet++, printf("  %s: quiet, peak %.3f\n", DICE_SOUND_NAMES[n], pk);
        lo = fmin(lo, db), hi = fmax(hi, db);
        printf("%s %s %.1f", n == 1 ? "picks: SOUNDs" : ",", DICE_SOUND_NAMES[n], db);
        smp_stop(&t->lib);
        free(t);
    }
    CHECK(!bad && !quiet, "every SOUND plays, finite, under 0.95 (%d bad, %d quiet)", bad, quiet);
    printf("\npicks: SOUNDs loudness %.1f .. %.1f dB\n", lo, hi);

    /* every factory kit plays, finite, under full scale, near the others */
    double db[DICE_KITS];
    lo = 99, hi = -99, bad = 0;
    printf("picks: kits");
    for (int k = 1; k < DICE_KITS; k++) {
        strut_t *t = fresh();
        float kit[STRUT_PADS][P_COUNT];
        for (int i = 0; i < STRUT_PADS; i++) memcpy(kit[i], t->pad[i].p, sizeof(kit[i]));
        dice_kit(k, &kit[0][0], t->g);
        for (int i = 0; i < STRUT_PADS; i++) memcpy(t->pad[i].p, kit[i], sizeof(kit[i]));
        double pk;
        db[k] = kit_pass(t, &pk);
        bad += !isfinite(db[k]) || pk >= 1.0;
        lo = fmin(lo, db[k]), hi = fmax(hi, db[k]);
        printf(" %s %.1f (peak %.2f)", DICE_KIT_NAMES[k], db[k], pk);
        smp_stop(&t->lib);
        free(t);
    }
    printf("\n");
    CHECK(!bad && hi - lo <= 4.0, "every kit plays, under full scale, within 4 dB of the others (%.1f)", hi - lo);
}

/* `state` saves the kit and loads it back exactly, keeps no DICE turn, and
 * answers even for an untouched kit (the host retries a slot that does not). */
static void state(void) {
    static char buf[131072];
    void *a = A->create_instance(".", ""), *b = A->create_instance(".", "");
    strut_t *sa = a, *sb = b;
    int n = A->get_param(a, "state", buf, sizeof(buf));
    CHECK(n > 0 && !strcmp(buf, "{\"v\":1}"), "an untouched kit saves as nothing but its version");
    A->set_param(a, "p01_dice", "Roll");
    A->set_param(a, "kit_dice", "Roll");
    A->set_param(a, "p03_decay", "0.25");
    A->set_param(a, "p16_n_table", param_option(&STRUT_PAD_PARAMS[P_N_TABLE], NT_TABLES + 3));
    A->set_param(a, "space", "0.4");
    A->set_param(a, "skin_view", "Mod");
    n = A->get_param(a, "state", buf, sizeof(buf));
    CHECK(n > 0 && n < 32768, "a rolled kit saves in under 32 KB");
    CHECK(!strstr(buf, "dice") && !strstr(buf, "_view"), "DICE and the MOD views are not saved");
    A->set_param(b, "p05_s_pitch", "0.9");
    A->set_param(b, "state", buf);
    int same = 1;
    for (int i = 0; i < STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++)
            if (k != P_DICE && fabsf(sa->pad[i].p[k] - sb->pad[i].p[k]) > 1e-4f) same = 0;
    for (int k = 0; k < G_COUNT; k++)
        if (k != G_DICE && k != G_SKIN_VIEW && fabsf(sa->g[k] - sb->g[k]) > 1e-4f) same = 0;
    CHECK(same, "every saved value loads back, and what was not saved returns to its default");
    CHECK(sb->g[G_SKIN_VIEW] == 0 && sb->dice.newest == 0, "loading rolls nothing and keeps no history");
    A->set_param(b, "state", "{\"v\":9,\"p02_nothing\":\"1\",\"p02_decay\":0.5,\"glue\":\"x\"}");
    CHECK(fabsf(sb->pad[1].p[P_DECAY] - 0.5f) < 1e-6f && sb->pad[0].p[P_DECAY] == STRUT_PAD_PARAMS[P_DECAY].def,
          "unknown keys are skipped, bare numbers read, the rest reset");
    CHECK(A->get_param(a, "state", buf, 20) < 0, "a short buffer is refused, not overrun");
    A->destroy_instance(a);
    A->destroy_instance(b);
}

/* A sound gone to inf or NaN is dropped, not left to silence Strut until it
 * is reloaded: the next hit plays. */
static void heal(void) {
    static int16_t out[2 * 128];
    void *p = A->create_instance(".", "");
    strut_t *s = p;
    A->set_param(p, "p01_space", "0.5");
    for (int bad = 0; bad < 2; bad++) {
        uint8_t on[3] = { 0x90, STRUT_NOTE0, 100 };
        A->on_midi(p, on, 3, 0);
        A->render_block(p, out, 128);
        if (bad) s->kit.damp[0] = NAN;      /* the room */
        else s->pad[0].voice.gs = NAN;      /* one pad */
        for (int b = 0; b < 50; b++) A->render_block(p, out, 128);
        A->on_midi(p, on, 3, 0);
        int pk = 0;
        for (int b = 0; b < 100; b++) {
            A->render_block(p, out, 128);
            for (int i = 0; i < 256; i++) pk = abs(out[i]) > pk ? abs(out[i]) : pk;
        }
        CHECK(pk > 1000 && s->healed, "%s gone to NaN is dropped and the next hit plays (peak %d)",
              bad ? "the room" : "a pad", pk);
        char buf[64];
        A->get_param(p, "pad", buf, sizeof(buf));
        CHECK(!s->healed, "and it is logged once");
    }
    A->destroy_instance(p);
}

int main(int argc, char **argv) {
    smp_catalogue("src");    /* the library, as the module's folder holds it on the Move */
    const char *dir = argc > 1 ? argv[1] : ".";
    A = move_plugin_init_v2(NULL);
    CHECK(A && A->api_version == 2, "the v2 API");
    void *p = A->create_instance(".", "");
    contracts(p, dir);
    A->destroy_instance(p);
    pads();
    keys();
    skin();
    wave();
    noise();
    finish();
    modulation();
    restrike();
    levels();
    tail();
    kit();
    samples();
    modes();
    focus();
    sample_levels();
    dice();
    picks();
    state();
    heal();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
