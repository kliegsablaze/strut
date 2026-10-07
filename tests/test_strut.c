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

    /* DRIVE: a sine grows overtones, and a loud hit stays about as loud */
    {
        double third[2], peak[2];
        for (int i = 0; i < 2; i++) {
            strut_t *s = wave_pad();
            s->pad[0].p[P_W_DECAY] = 1.0f;
            s->pad[0].p[P_W_PITCH] = 24;
            s->pad[0].p[P_DRIVE] = i ? 0.7f : 0.0f;
            hit(s, 0.5f);
            third[i] = level_at(3 * wave_hz(s->pad[0].p), 4410, 8192) / level_at(wave_hz(s->pad[0].p), 4410, 8192);
            peak[i] = peak_of(L, 0, STRUT_SR / 2);
            free(s);
        }
        CHECK(third[1] > 30 * third[0] + 0.01, "DRIVE adds overtones (third harmonic %.4f, from %.4f)", third[1], third[0]);
        CHECK(fabs(20 * log10(peak[1] / peak[0])) < 6, "DRIVE keeps a loud hit's level (%+.1f dB)", 20 * log10(peak[1] / peak[0]));
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
            if (fabs(l[i]) * 32000 < 40 && (out[2 * i] || out[2 * i + 1])) {
                const double e = out[2 * i] - l[i] * 32000;
                worst = fmax(worst, fabs(e));
                rs ^= rs << 13, rs ^= rs >> 17, rs ^= rs << 5;
                const double v = l[i] * 32000, tp = ((rs & 0xFFFF) + (rs >> 16)) / 65536.0 - 1;
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
    wave();
    noise();
    finish();
    modulation();
    restrike();
    levels();
    tail();
    focus();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
