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
static double sweep_hit(strut_t *s, const char *what, float v, int must_end) {
    const int n = hit(s, 2.0f);
    double peak = 0;
    int finite = 1;
    for (int j = 0; j < n; j++) {
        finite &= isfinite(L[j]);
        peak = fmax(peak, fabs(L[j]));
    }
    CHECK(finite, "%s %g: finite", what, v);
    CHECK(peak < 0.9, "%s %g: does not clip (peak %.2f)", what, v, peak);
    CHECK(peak > 0.05, "%s %g: sounds (peak %.3f)", what, v, peak);
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
            const double peak = sweep_hit(s, d->key, v, !(knobs[k] == P_W_DECAY && i == 2));
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
            const double peak = sweep_hit(s, what, (float)i / 4, 1);
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
            const double peak = sweep_hit(s, what, (float)k, 1);
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
        sweep_hit(s, "w_fm with a long ring", 0.8f, 0);
        hit(s, 0.5f);
        const double bent = level_at(wave_hz(s->pad[0].p), 2205, 4096);
        CHECK(bent < 0.5 * plain, "FM moves Wave's energy off its pitch (%.2f of it left)", bent / plain);
        free(s);
    }
}

/* Renders n samples of whatever is sounding into L from `at`. */
static void play(strut_t *s, int at, int n) {
    for (int k = 0; k < n; k += 128) strut_render(s, L + at + k, R + at + k, n - k < 128 ? n - k : 128);
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
    const int keys[] = { P_SKIN, P_WAVE, P_LEVEL };
    for (int k = 0; k < 3; k++) {
        double prev = 0;
        int even = 1;
        for (int i = 0; i <= 10; i++) {
            strut_t *s = keys[k] == P_WAVE ? wave_pad() : fresh();
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
    restrike();
    levels();
    tail();
    focus();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
