/*
 * The sample library (samples.h). A sample is read, made mono, and stored
 * as Noise stores a table: at twice the output's rate, with nothing above
 * 19.8 kHz, and in copies an octave apart, each with nothing above its own
 * top, so it can be read smoothly at any speed and played high without
 * folding back (noise.h). A one-shot's copies are made by filters, a
 * cycle's by its harmonics, as Wave's are.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "fft.h"
#include "samples.h"
#include "strut.h"

#define PI 3.14159265358979
#define TOP_HZ (0.45 * STRUT_SR)        /* the top of level 0, as Noise's */
#define CY_RMS 0.2f                     /* a cycle's level */
#define KEEP (16u << 20)                /* samples nobody uses kept this long, for going back */

_Static_assert(SM_PADS == STRUT_PADS, "one want, ready and used a pad");
_Static_assert((uint64_t)SM_MAX << SM_FRAC <= 0x80000000u, "a one-shot's read position never wraps");
_Static_assert((uint64_t)CY_N << CY_FRAC == 0x100000000u, "a cycle's read position wraps the cycle");

/* ---- the list ---- */

/* Drums first, in the order a kit is laid out, then the rest. */
static const char *const FOLDERS[] = {
    "Kick", "Snare", "Rim", "Clap", "Hat", "Cymbal", "Tom", "Percussion", "Bell", "Mallet", "Metal", "Pluck",
    "Wind", "Bowed", "Keys", "Bass", "Cycle", "Voice", "Foley", "Toy", "Noise", "Glitch", "Ambient",
};

static char names[SM_FILES][SM_NAME];
static char *paths[SM_FILES];
static unsigned char is_cycle[SM_FILES];
static int count, built;

int smp_count(void) { return count; }
const char *smp_name(int i) { return i >= 0 && i < count ? names[i] : ""; }
const char *smp_path(int i) { return i >= 0 && i < count ? paths[i] : ""; }

static int by_name(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

/* Every .wav in dir, by name, into the list. A name is the file's without
 * ".wav", kept to what the contract can carry: printable ASCII, no quotes,
 * at most 31 letters. */
static void scan(const char *dir, int cycle) {
    DIR *d = opendir(dir);
    if (!d) return;
    char *found[SM_FILES];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) && n < SM_FILES) {
        const size_t len = strlen(e->d_name);
        if (e->d_name[0] == '.' || len < 5 || strcasecmp(e->d_name + len - 4, ".wav")) continue;
        if ((found[n] = strdup(e->d_name))) n++;
    }
    closedir(d);
    qsort(found, (size_t)n, sizeof(found[0]), by_name);
    for (int i = 0; i < n; i++) {
        if (count < SM_FILES) {
            const size_t len = strlen(found[i]) - 4;
            char *nm = names[count];
            size_t k = 0;
            for (; k < len && k < SM_NAME - 1; k++) {
                /* plain letters only: a name cut short must not end half
                 * way through a letter the contract cannot then carry */
                const unsigned char c = (unsigned char)found[i][k];
                nm[k] = c < ' ' || c > '~' || c == '"' || c == '\\' ? '_' : (char)c;
            }
            nm[k] = '\0';
            const size_t pl = strlen(dir) + strlen(found[i]) + 2;
            if ((paths[count] = malloc(pl))) {
                snprintf(paths[count], pl, "%s/%s", dir, found[i]);
                is_cycle[count] = (unsigned char)cycle;
                count++;
            }
        }
        free(found[i]);
    }
}

void smp_catalogue(const char *module_dir) {
    if (built || !module_dir) return;
    built = 1;
    char dir[512];
    for (size_t f = 0; f < sizeof(FOLDERS) / sizeof(FOLDERS[0]); f++) {
        snprintf(dir, sizeof(dir), "%s/samples/%s", module_dir, FOLDERS[f]);
        scan(dir, !strcmp(FOLDERS[f], "Cycle"));
    }
    /* your own, beside the modules: /data/UserData/schwung/samples/strut */
    const char *m = strstr(module_dir, "/modules/");
    if (m) {
        snprintf(dir, sizeof(dir), "%.*s/samples/strut", (int)(m - module_dir), module_dir);
        scan(dir, 0);
    }
}

/* ---- reading a file ---- */

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

/* Whole-number PCM of 8 to 32 bits, or 32-bit floats, any rate, any
 * channels (averaged to one), at most SM_MAX / 2 frames. */
int smp_read_wav(const char *path, float **x, int *frames, int *rate) {
    *x = NULL, *frames = 0, *rate = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t h[12];
    int fmt = 0, ch = 0, bits = 0, ok = 0;
    if (fread(h, 1, 12, f) != 12 || memcmp(h, "RIFF", 4) || memcmp(h + 8, "WAVE", 4)) { fclose(f); return 0; }
    for (;;) {
        uint8_t c[8];
        if (fread(c, 1, 8, f) != 8) break;
        const uint32_t sz = le32(c + 4);
        if (!memcmp(c, "fmt ", 4)) {
            uint8_t m[40] = { 0 };
            const uint32_t take = sz < sizeof(m) ? sz : (uint32_t)sizeof(m);
            if (sz < 16 || fread(m, 1, take, f) != take) break;
            fmt = le16(m), ch = le16(m + 2), *rate = (int)le32(m + 4), bits = le16(m + 14);
            if (fmt == 0xFFFE && sz >= 26) fmt = le16(m + 24);     /* extensible: its sub-format */
            if (fseek(f, (long)(sz - take + (sz & 1)), SEEK_CUR)) break;
        } else if (!memcmp(c, "data", 4)) {
            const int bytes = bits / 8;
            if (!((fmt == 1 && bytes >= 1 && bytes <= 4) || (fmt == 3 && bits == 32)) || ch < 1 || ch > 16 || *rate < 8000) break;
            long n = (long)(sz / (uint32_t)(bytes * ch));
            if (n > SM_MAX / 2) n = SM_MAX / 2;
            uint8_t *raw = malloc((size_t)n * (size_t)(bytes * ch));
            float *y = malloc(sizeof(float) * (size_t)(n > 0 ? n : 1));
            if (!raw || !y) { free(raw), free(y); break; }
            n = (long)fread(raw, (size_t)(bytes * ch), (size_t)n, f);
            for (long i = 0; i < n; i++) {
                float s = 0;
                for (int k = 0; k < ch; k++) {
                    const uint8_t *p = raw + ((size_t)i * (size_t)ch + (size_t)k) * (size_t)bytes;
                    float v;
                    if (fmt == 3) { uint32_t u = le32(p); memcpy(&v, &u, 4); }
                    else if (bytes == 1) v = ((float)p[0] - 128.0f) / 128.0f;
                    else if (bytes == 2) v = (float)(int16_t)le16(p) / 32768.0f;
                    else if (bytes == 3) v = (float)((int32_t)((uint32_t)p[0] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 24) >> 8) / 8388608.0f;
                    else v = (float)(int32_t)le32(p) / 2147483648.0f;
                    s += isfinite(v) ? v : 0.0f;
                }
                y[i] = s / (float)ch;
            }
            free(raw);
            *x = y, *frames = (int)n, ok = n > 0;
            if (!ok) free(y), *x = NULL;
            break;
        } else if (fseek(f, (long)(sz + (sz & 1)), SEEK_CUR)) {
            break;
        }
    }
    fclose(f);
    return ok;
}

/* ---- making the copies ---- */

/* The Kaiser window's Bessel function (its series). */
static double i0(double x) {
    double s = 1, t = 1;
    for (int k = 1; k < 30; k++) t *= (x / (2 * k)) * (x / (2 * k)), s += t;
    return s;
}

/* Kaiser's window at m of +-M, beta 6.76: 70 dB down outside its band. */
static double kaiser(int m, int M) {
    const double r = (double)m / M;
    return i0(6.76 * sqrt(fmax(0, 1 - r * r))) / i0(6.76);
}

static double sinc(double x) { return x == 0 ? 1 : sin(PI * x) / (PI * x); }

/* Twice the rate: a half-band filter, every other tap zero, passing to
 * 19.8 kHz and stopping from 24.3 (88 taps). Down an octave: a low-pass
 * passing to the copy's top, a ninth of its rate, stopping from 0.15 (117
 * taps), then every other sample. Worked out once, by the first load. */
#define UP_J 22
#define DN_M 58
static float up_c[UP_J], dn_c[DN_M + 1];
static pthread_once_t designed = PTHREAD_ONCE_INIT;

static void design(void) {
    for (int j = 0; j < UP_J; j++) up_c[j] = (float)(sinc((2 * j + 1) / 2.0) * kaiser(2 * j + 1, 2 * UP_J - 1));
    double s = 0;
    for (int m = 0; m <= DN_M; m++) {
        dn_c[m] = (float)(2 * 0.13125 * sinc(2 * 0.13125 * m) * kaiser(m, DN_M));
        s += m ? 2 * dn_c[m] : dn_c[m];
    }
    for (int m = 0; m <= DN_M; m++) dn_c[m] = (float)(dn_c[m] / s);
}

/* Floats to 16 bits, held under full scale. */
static void store(int16_t *q, const float *x, int n, float g) {
    for (int i = 0; i < n; i++) {
        const float v = fminf(fmaxf(x[i] * g, -32767.0f), 32767.0f);
        q[i] = (int16_t)(int32_t)(v + (v < 0 ? -0.5f : 0.5f));
    }
}

static int levels_n(uint32_t len, int l) { return (int)((len + (1u << l) - 1) >> l); }

/* Room for a table of every copy: each with two samples before and five
 * after, as nt_read reads. */
static int16_t *alloc_levels(nt_table_t *t, uint32_t len, size_t *bytes) {
    size_t n = 0;
    for (int l = 0; l < NT_LEVELS; l++) n += (size_t)levels_n(len, l) + 7;
    int16_t *d = calloc(n, sizeof(int16_t));
    if (!d) return NULL;
    *bytes = n * sizeof(int16_t);
    int16_t *q = d + 2;
    for (int l = 0; l < NT_LEVELS; l++) t->level[l] = q, q += levels_n(len, l) + 7;
    return d;
}

static smp_t *alloc_smp(uint32_t len) {
    smp_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    size_t b;
    if (!(s->data = alloc_levels(&s->t, len, &b))) { free(s); return NULL; }
    s->bytes = b + sizeof(*s);
    s->len = len;
    return s;
}

/* Each quarter octave's share of the power, as Noise's (noise.c). */
static void add_band(float *band, float hz, float pw) {
    const int b = (int)floorf(4.0f * log2f(hz / 20.0f));
    if (b >= 0 && b < NT_BANDS) band[b] += pw;
}

/* Each copy an octave down from the last, from level 0 in y (which it
 * overwrites); b is scratch for half of it. */
static void copies(nt_table_t *t, float *y, uint32_t len, float *b) {
    int m = (int)len;
    float *src = y, *dst = b;
    for (int l = 1; l < NT_LEVELS; l++) {
        const int h = levels_n(len, l);
        for (int i = 0; i < h; i++) {
            const float *c = src + 2 * i;
            float v = dn_c[0] * c[0];
            for (int k = 1; k <= DN_M; k++) v += dn_c[k] * ((2 * i - k >= 0 ? c[-k] : 0) + (2 * i + k < m ? c[k] : 0));
            dst[i] = v;
        }
        store((int16_t *)t->level[l], dst, h, 32767.0f);
        float *x = src;
        src = dst, dst = x, m = h;
    }
}

static smp_t *load_cycle(const float *x) {
    smp_t *s = alloc_smp(CY_N);
    float *re = malloc(sizeof(float) * CY_N), *im = malloc(sizeof(float) * CY_N);
    float *hr = malloc(sizeof(float) * CY_N / 2), *hi = malloc(sizeof(float) * CY_N / 2);
    float *lv = malloc(sizeof(float) * CY_N);
    if (!s || !re || !im || !hr || !hi || !lv) {
        free(re), free(im), free(hr), free(hi), free(lv);
        if (s) free(s->data), free(s);
        return NULL;
    }
    /* its harmonics */
    for (int i = 0; i < CY_N / 2; i++) re[i] = x[i], im[i] = 0;
    fft(re, im, CY_N / 2, -1);
    for (int k = 0; k < CY_N / 4; k++) hr[k] = re[k] / (CY_N / 2), hi[k] = im[k] / (CY_N / 2);
    float g = 0, peak = 0;
    for (int l = 0; l < NT_LEVELS; l++) {
        const int n = CY_N >> l;
        int top = (int)(TOP_HZ / (1 << l) / CY_HZ);
        if (top > n / 2 - 1) top = n / 2 - 1;
        if (top > CY_N / 4 - 1) top = CY_N / 4 - 1;
        memset(re, 0, sizeof(float) * n), memset(im, 0, sizeof(float) * n);
        for (int k = 1; k <= top; k++) re[k] = hr[k], im[k] = hi[k], re[n - k] = hr[k], im[n - k] = -hi[k];
        fft(re, im, n, 1);
        if (l == 0) {       /* to CY_RMS, under full scale */
            double e = 0;
            for (int i = 0; i < n; i++) e += (double)re[i] * re[i], peak = fmaxf(peak, fabsf(re[i]));
            const float rms = (float)sqrt(e / n);
            g = rms > 0 ? CY_RMS / rms : 0;
            if (peak * g > 0.99f) g = 0.99f / peak;
            g *= 32767.0f;
            for (int k = 1; k <= top; k++) add_band(s->t.band, (float)k * CY_HZ, 2 * (hr[k] * hr[k] + hi[k] * hi[k]));
        }
        for (int i = 0; i < n; i++) lv[i] = re[i];
        int16_t *q = (int16_t *)s->t.level[l];
        store(q, lv, n, g);
        q[-2] = q[n - 2], q[-1] = q[n - 1];
        for (int i = 0; i < 5; i++) q[n + i] = q[i];
    }
    for (int b = 0; b < NT_BANDS; b++) s->t.var += s->t.band[b];
    s->cycle = 1;
    free(re), free(im), free(hr), free(hi), free(lv);
    return s;
}

static smp_t *load_shot(const float *x, int n) {
    const uint32_t len = (uint32_t)n * 2;
    smp_t *s = alloc_smp(len);
    float *a = malloc(sizeof(float) * (len + 2 * DN_M + 8));
    float *b = malloc(sizeof(float) * (len / 2 + 2 * DN_M + 8));
    float *re = malloc(sizeof(float) * 4096), *im = malloc(sizeof(float) * 4096);
    if (!s || !a || !b || !re || !im) {
        free(a), free(b), free(re), free(im);
        if (s) free(s->data), free(s);
        return NULL;
    }
    /* level 0: twice the rate, between each pair the half-band's guess */
    float *y = a + DN_M;            /* room either side for the filter below */
    memset(a, 0, sizeof(float) * (len + 2 * DN_M + 8));
    for (int i = 0; i < n; i++) {
        float v = 0;
        for (int j = 0; j < UP_J; j++) {
            const float p = i - j >= 0 ? x[i - j] : 0, q = i + 1 + j < n ? x[i + 1 + j] : 0;
            v += up_c[j] * (p + q);
        }
        y[2 * i] = x[i], y[2 * i + 1] = v;
    }
    store((int16_t *)s->t.level[0], y, (int)len, 32767.0f);
    /* its colour, for COLOR's level match and Skin's strike: averaged over
     * windows of 4096, half overlapped */
    for (uint32_t at = 0; at < len; at += 2048) {
        for (int i = 0; i < 4096; i++) {
            const float w = 0.5f - 0.5f * cosf(2 * (float)PI * (float)i / 4096);
            re[i] = at + (uint32_t)i < len ? y[at + (uint32_t)i] * w : 0, im[i] = 0;
        }
        fft(re, im, 4096, -1);
        for (int k = 1; k < 2048; k++) add_band(s->t.band, (float)k * NT_SR / 4096, re[k] * re[k] + im[k] * im[k]);
        if (at + 4096 >= len) break;
    }
    for (int k = 0; k < NT_BANDS; k++) s->t.var += s->t.band[k];
    copies(&s->t, y, len, b);
    free(a), free(b), free(re), free(im);
    return s;
}

smp_t *smp_load(int entry) {
    if (entry < 0 || entry >= count) return NULL;
    pthread_once(&designed, design);
    float *x;
    int n, rate;
    if (!smp_read_wav(paths[entry], &x, &n, &rate)) return NULL;
    smp_t *s = is_cycle[entry] && n == CY_N / 2 ? load_cycle(x) : load_shot(x, n);
    free(x);
    if (!s) return NULL;
    s->entry = entry;
    s->speed = (float)rate / STRUT_SR;
    return s;
}

/* ---- Resynth and Noise (MODE) ---- */

#define AN_N 4096       /* the sine waves' window: 46 ms of level 0 at 88.2 kHz, 43 Hz apart */
#define AN_PEAKS 48     /* the loudest peaks a frame offers the tracks */
#define NZ_N 512        /* the noise's window: 5.8 ms, so an attack stays sharp */
#define NZ_HOP 128
#define PHASES 4096

static float phase_sin[PHASES + PHASES / 4];
static float an_w[AN_N], nz_w[NZ_N];
static pthread_once_t analysed = PTHREAD_ONCE_INIT;

static void design_an(void) {
    for (int i = 0; i < PHASES + PHASES / 4; i++) phase_sin[i] = (float)sin(2 * PI * i / PHASES);
    for (int i = 0; i < AN_N; i++) an_w[i] = (float)(0.5 - 0.5 * cos(2 * PI * i / AN_N));
    for (int i = 0; i < NZ_N; i++) nz_w[i] = (float)(0.5 - 0.5 * cos(2 * PI * i / NZ_N));
}

typedef struct { float f, a; } peak_t;

static int by_amp(const void *x, const void *y) {
    const float a = ((const peak_t *)x)->a, b = ((const peak_t *)y)->a;
    return (a < b) - (a > b);
}

/* One frame's peaks (McAulay and Quatieri): the local highs of the
 * spectrum, each placed between bins by a parabola through its log level
 * and its neighbours' (Smith and Serra), louder than floor and than 60 dB
 * under the frame's loudest, under top; the loudest AN_PEAKS. */
static int peaks(const float *re, const float *im, float *mag, int kmax, float floor_a, peak_t *pk) {
    float loud = 0;
    for (int k = 0; k <= kmax + 1; k++) mag[k] = sqrtf(re[k] * re[k] + im[k] * im[k]), loud = fmaxf(loud, mag[k]);
    const float least = fmaxf(floor_a * AN_N / 4, loud * 1e-3f);
    peak_t all[AN_N / 4];
    int n = 0;
    for (int k = 2; k <= kmax && n < AN_N / 4; k++) {
        const float m = mag[k];
        if (m <= least || m <= mag[k - 1] || m < mag[k + 1]) continue;
        const float a = logf(mag[k - 1] + 1e-12f), b = logf(m), c = logf(mag[k + 1] + 1e-12f);
        const float d = a - 2 * b + c;
        const float x = d < 0 ? fminf(fmaxf(0.5f * (a - c) / d, -0.5f), 0.5f) : 0.0f;
        all[n].f = ((float)k + x) / AN_N;
        all[n].a = 4.0f * expf(b - 0.25f * (a - c) * x) / AN_N;    /* a sine of level A peaks at A N / 4 */
        n++;
    }
    qsort(all, (size_t)n, sizeof(peak_t), by_amp);
    if (n > AN_PEAKS) n = AN_PEAKS;
    memcpy(pk, all, sizeof(peak_t) * (size_t)n);
    return n;
}

/* The sine waves: each frame's peaks joined into tracks, RS_SLOTS at
 * most, a track kept in its slot from frame to frame so its sine runs on
 * smoothly (McAulay and Quatieri's matching: each track takes the nearest
 * peak within 3 %, the loudest tracks first; peaks left over start new
 * tracks in free slots, the loudest first). */
static int analyse(smp_t *s, const float *x) {
    const uint32_t len = s->len;
    const int nf = (int)(len / RS_HOP) + 1;
    s->fq = calloc((size_t)nf * RS_SLOTS, sizeof(float));
    s->am = calloc((size_t)nf * RS_SLOTS, sizeof(float));
    float *re = malloc(sizeof(float) * AN_N), *im = malloc(sizeof(float) * AN_N);
    float *mag = malloc(sizeof(float) * (AN_N / 2 + 2));
    float raw[2][RS_SLOTS] = { { 0 } };     /* each track's level before the gate below, last frame and this */
    int idle[RS_SLOTS];                     /* frames since each slot's track ended */
    for (int k = 0; k < RS_SLOTS; k++) idle[k] = 2;
    if (!s->fq || !s->am || !re || !im || !mag) { free(re), free(im), free(mag); return 0; }
    float top = 0;
    for (uint32_t i = 0; i < len; i++) top = fmaxf(top, fabsf(x[i]));
    const float floor_a = top * 3e-4f;     /* 70 dB under the sample's peak */
    s->onset = 0;
    while (s->onset + 1 < len && fabsf(x[s->onset]) < 0.1f * top) s->onset++;
    /* nothing over 19.8 kHz as the file plays */
    int kmax = (int)(TOP_HZ / (2.0 * s->speed * STRUT_SR) * AN_N);
    if (kmax > AN_N / 2 - 2) kmax = AN_N / 2 - 2;
    double wsum2 = 0, ssum2 = 0;
    for (int i = 0; i < AN_N; i++) wsum2 += (double)an_w[i] * an_w[i];
    for (int i = 0; i < AN_N / 2; i++) ssum2 += (double)an_w[2 * i] * an_w[2 * i];
    for (int f = 0; f < nf; f++) {
        const long c = (long)f * RS_HOP;
        double el = 0, es = 0;
        for (int i = 0; i < AN_N; i++) {
            const long j = c - AN_N / 2 + i;
            const float v = j >= 0 && j < (long)len ? x[j] : 0.0f;
            re[i] = v * an_w[i], im[i] = 0;
            el += (double)re[i] * re[i];
            if (i >= AN_N / 4 && i < 3 * AN_N / 4) {
                const double h = v * an_w[2 * (i - AN_N / 4)];
                es += h * h;
            }
        }
        /* the window is long, so on its own a sine would rise before the
         * hit that starts it: each frame's sines are scaled by the level
         * the middle half of the window heard (through a window of its
         * own, so a low pitch's swing does not wobble it) against the
         * level the whole heard; at most 2, which a sine starting at the
         * middle needs */
        el /= wsum2, es /= ssum2;
        const float gate = el > 0 ? (float)fmin(es / el, 2.0) : 0.0f;
        fft(re, im, AN_N, -1);
        peak_t pk[AN_PEAKS];
        const int np = peaks(re, im, mag, kmax, floor_a, pk);
        float *fq = s->fq + (size_t)f * RS_SLOTS, *am = s->am + (size_t)f * RS_SLOTS;
        const float *fq0 = f ? fq - RS_SLOTS : NULL;
        int taken[AN_PEAKS] = { 0 }, done[RS_SLOTS] = { 0 };
        /* the tracks still going, loudest first */
        for (;;) {
            int j = -1;
            for (int k = 0; k < RS_SLOTS; k++)
                if (!done[k] && raw[0][k] > 0 && (j < 0 || raw[0][k] > raw[0][j])) j = k;
            if (j < 0) break;
            done[j] = 1;
            int best = -1;
            const float tol = fmaxf(0.03f * fq0[j], 1.5f / AN_N);
            for (int p = 0; p < np; p++)
                if (!taken[p] && fabsf(pk[p].f - fq0[j]) < tol && (best < 0 || fabsf(pk[p].f - fq0[j]) < fabsf(pk[best].f - fq0[j]))) best = p;
            if (best >= 0) taken[best] = 1, fq[j] = pk[best].f, raw[1][j] = pk[best].a;
            else fq[j] = fq0[j], raw[1][j] = 0;     /* it ends, fading to nothing at its own pitch */
        }
        /* new ones, in the slots free */
        for (int p = 0, k = 0; p < np; p++) {
            if (taken[p]) continue;
            /* a slot rests a frame first, so the track it held fades
             * out at its own pitch, not gliding to this one's */
            while (k < RS_SLOTS && (done[k] || idle[k] < 2)) k++;
            if (k == RS_SLOTS) break;
            done[k] = 1;
            fq[k] = pk[p].f, raw[1][k] = pk[p].a;
            if (f) s->fq[(size_t)(f - 1) * RS_SLOTS + (size_t)k] = pk[p].f;    /* rising from nothing at its own pitch */
            k++;
        }
        for (int k = 0; k < RS_SLOTS; k++) {
            if (!done[k]) fq[k] = fq0 ? fq0[k] : 0, raw[1][k] = 0;
            am[k] = raw[1][k] * gate;
            raw[0][k] = raw[1][k];
            idle[k] = raw[1][k] > 0 ? 0 : idle[k] + 1;
        }
    }
    s->frames = nf;
    free(re), free(im), free(mag);
    return 1;
}

/* The sine waves as Resynth plays them at the sample's own pitch and
 * length, into p (level 0's rate): each frame to the next, at the pitch
 * between them, its level gliding. */
static void sines(const smp_t *s, float *p) {
    memset(p, 0, sizeof(float) * s->len);
    uint32_t r = 0x9E3779B9u;
    for (int k = 0; k < RS_SLOTS; k++) {
        r ^= r << 13, r ^= r >> 17, r ^= r << 5;
        double c = cos(2 * PI * (r >> 20) / 4096), sn = sin(2 * PI * (r >> 20) / 4096);
        for (int f = 0; f + 1 < s->frames; f++) {
            const float *q = s->fq + (size_t)f * RS_SLOTS + k, *a = s->am + (size_t)f * RS_SLOTS + k;
            if (a[0] == 0 && a[RS_SLOTS] == 0) continue;
            const double w = PI * (q[0] + q[RS_SLOTS]), cr = cos(w), ci = sin(w);
            const double da = (a[RS_SLOTS] - a[0]) / RS_HOP;
            double g = a[0];
            const uint32_t at = (uint32_t)f * RS_HOP;
            for (uint32_t i = 0; i < RS_HOP && at + i < s->len; i++) {
                const double t = c * cr - sn * ci;
                sn = sn * cr + c * ci, c = t;
                g += da;
                p[at + i] += (float)(g * sn);
            }
            const double m = 1.5 - 0.5 * (c * c + sn * sn);
            c *= m, sn *= m;
        }
    }
}

/* x's colour as it changes, every pitch taken out, into y: each short
 * stretch's spectrum kept and its phases thrown away (Serra and Smith's
 * stochastic part), laid end to end. With sub (the sine waves), what they
 * account for is taken out first, leaving Resynth's noise. Returns how much
 * of x's power it keeps. */
static double colour_of(const float *x, const float *sub, uint32_t len, float speed, float *y) {
    float re[NZ_N], im[NZ_N], sr[NZ_N], si[NZ_N];
    int kmax = (int)(TOP_HZ / (2.0 * speed * STRUT_SR) * NZ_N);
    if (kmax > NZ_N / 2 - 1) kmax = NZ_N / 2 - 1;
    double ex = 0, ek = 0;
    uint32_t r = 0x2545F491u;
    memset(y, 0, sizeof(float) * len);
    for (long st = -NZ_N + NZ_HOP; st < (long)len; st += NZ_HOP) {
        for (int i = 0; i < NZ_N; i++) {
            const long j = st + i;
            const int in = j >= 0 && j < (long)len;
            re[i] = in ? x[j] * nz_w[i] : 0, im[i] = 0;
            if (sub) sr[i] = in ? sub[j] * nz_w[i] : 0, si[i] = 0;
        }
        fft(re, im, NZ_N, -1);
        if (sub) fft(sr, si, NZ_N, -1);
        float m[NZ_N / 2 + 2] = { 0 };
        for (int k = 0; k < NZ_N / 2; k++) {
            const float px = re[k] * re[k] + im[k] * im[k];
            const float pk = k >= 1 && k <= kmax ? sub ? fmaxf(px - (sr[k] * sr[k] + si[k] * si[k]), 0.0f) : px : 0.0f;
            ex += px, ek += pk;
            m[k] = pk;
        }
        if (!sub) {
            /* Noise: the colour, not its lines; each bin's power spread
             * over two either side, so a note becomes a band of noise
             * around it and not a pitch */
            float q[NZ_N / 2];
            for (int k = 1; k <= kmax; k++) {
                float a = 3 * m[k];
                for (int d = 1; d <= 2; d++) a += (float)(3 - d) * ((k - d >= 1 ? m[k - d] : 0) + (k + d <= kmax ? m[k + d] : 0));
                q[k] = a * (1.0f / 9);
            }
            for (int k = 1; k <= kmax; k++) m[k] = q[k];
        }
        for (int k = 0; k < NZ_N / 2; k++) m[k] = sqrtf(m[k]);
        memset(re, 0, sizeof(re)), memset(im, 0, sizeof(im));
        for (int k = 1; k <= kmax; k++) {
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            const int ph = (int)(r >> 20);
            const float c = m[k] * phase_sin[ph + PHASES / 4], sn = m[k] * phase_sin[ph];
            re[k] = c, im[k] = sn, re[NZ_N - k] = c, im[NZ_N - k] = -sn;
        }
        fft(re, im, NZ_N, 1);
        for (int i = 0; i < NZ_N; i++) {
            const long j = st + i;
            if (j >= 0 && j < (long)len) y[j] += re[i] * nz_w[i];
        }
    }
    return ex > 0 ? ek / ex : 0;
}

/* A table of y (level 0, len samples) at the power of x times keep. */
static int16_t *as_table(nt_table_t *t, float *y, const float *x, double keep, uint32_t len, float *b, size_t *bytes) {
    int16_t *d = alloc_levels(t, len, bytes);
    if (!d) return NULL;
    double px = 0, py = 0;
    for (uint32_t i = 0; i < len; i++) px += (double)x[i] * x[i], py += (double)y[i] * y[i];
    const float g = py > 0 ? (float)sqrt(px * keep / py) / 32767.0f : 0.0f;     /* to floats, as load_shot's */
    for (uint32_t i = 0; i < len; i++) y[i] *= g;
    store((int16_t *)t->level[0], y, (int)len, 32767.0f);
    copies(t, y, len, b);
    return d;
}

int smp_build(smp_t *s, int m) {
    if (s->cycle || (m != SM_RESYNTH && m != SM_NOISE)) return 0;
    if (s->has & 1 << m) return 1;
    pthread_once(&analysed, design_an);
    const uint32_t len = s->len;
    float *x = malloc(sizeof(float) * len), *y = malloc(sizeof(float) * len);
    float *b = malloc(sizeof(float) * (len / 2 + 8));
    float *p = m == SM_RESYNTH ? malloc(sizeof(float) * len) : NULL;
    int ok = x && y && b && (m != SM_RESYNTH || p);
    if (ok) {
        for (uint32_t i = 0; i < len; i++) x[i] = s->t.level[0][i];
        size_t bytes = 0;
        if (m == SM_NOISE) {
            /* as loud as the sample, though it keeps nothing under
             * 170 Hz, where 5.8 ms is too short to hold a pitch's colour */
            colour_of(x, NULL, len, s->speed, y);
            ok = (s->ndata = as_table(&s->noise, y, x, 1.0, len, b, &bytes)) != NULL;
            if (ok) {
                memcpy(s->noise.band, s->t.band, sizeof(s->t.band));
                s->noise.var = s->t.var;
            }
        } else if ((ok = analyse(s, x))) {
            sines(s, p);
            const double keep = colour_of(x, p, len, s->speed, y);
            ok = (s->rdata = as_table(&s->rest, y, x, keep, len, b, &bytes)) != NULL;
            bytes += (size_t)s->frames * RS_SLOTS * 2 * sizeof(float);
        }
        if (ok) s->bytes += bytes;
    }
    free(x), free(y), free(b), free(p);
    s->tried |= 1 << m;
    if (ok) __atomic_or_fetch(&s->has, 1 << m, __ATOMIC_RELEASE);
    return ok;
}

void smp_free(smp_t *s) {
    if (s) free(s->data), free(s->ndata), free(s->rdata), free(s->fq), free(s->am), free(s);
}

/* ---- the loader ---- */

static int held(const smp_lib_t *lib, const smp_t *s) {
    for (int i = 0; i < SM_PADS; i++)
        if (lib->ready[i] == s || __atomic_load_n(&lib->used[i], __ATOMIC_ACQUIRE) == s) return 1;
    return 0;
}

void smp_service(smp_lib_t *lib, int patience) {
    for (int i = 0; i < SM_PADS; i++) {
        const int w = __atomic_load_n(&lib->want[i], __ATOMIC_RELAXED);
        const int m = __atomic_load_n(&lib->mode[i], __ATOMIC_RELAXED);
        const int key = w < 0 ? -1 : w * 4 + (m & 3);
        if (key != lib->seen[i]) lib->seen[i] = key, lib->still[i] = 0;
        else if (lib->still[i] < 1 << 20) lib->still[i]++;
        const smp_t *r = lib->ready[i];
        if (w < 0 || w >= count) {
            if (r) __atomic_store_n(&lib->ready[i], NULL, __ATOMIC_RELEASE);
            continue;
        }
        /* MODE's other ways are made when first asked for */
        const int need = m == SM_RESYNTH || m == SM_NOISE ? 1 << m : 0;
        /* a knob turning through the list loads nothing until it rests */
        if ((r && r->entry == w && (r->cycle || !(need & ~r->tried))) || lib->still[i] < patience || lib->still[i] < 0) continue;
        smp_t *s = lib->cache;
        while (s && s->entry != w) s = s->next;
        if (!s && (s = smp_load(w))) {
            s->next = lib->cache, lib->cache = s;
            lib->cached += s->bytes;
        }
        if (!s) { lib->still[i] = -(1 << 20); continue; }    /* unreadable: not tried again till TABLE moves */
        if (need & ~s->tried && !s->cycle) {
            const size_t was = s->bytes;
            smp_build(s, m);
            lib->cached += s->bytes - was;
        }
        s->dead = 0;
        if (r != s) __atomic_store_n(&lib->ready[i], s, __ATOMIC_RELEASE);
    }
    /* what no pad has ready is let go: kept while the cache has room, and
     * freed only once two blocks have passed and no voice is using it */
    const unsigned now = __atomic_load_n(&lib->blocks, __ATOMIC_ACQUIRE);
    for (smp_t **pp = &lib->cache, *s; (s = *pp);) {
        if (held(lib, s)) { s->dead = 0; pp = &s->next; continue; }
        if (!s->dead) s->dead = now + 1;
        if (lib->cached > KEEP && now - (s->dead - 1) >= 2) {
            *pp = s->next;
            lib->cached -= s->bytes;
            smp_free(s);
            continue;
        }
        pp = &s->next;
    }
}

static void *loader(void *arg) {
    smp_lib_t *lib = arg;
#ifdef __linux__
    /* first: off the audio's priority and its core (plugin_api_v1.h) */
    struct sched_param sp = { .sched_priority = 0 };
    sched_setscheduler(0, SCHED_OTHER, &sp);
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set), CPU_SET(1, &set), CPU_SET(2, &set);
    sched_setaffinity(0, sizeof(set), &set);
#endif
    const struct timespec tick = { 0, 10 * 1000000L };
    while (!__atomic_load_n(&lib->quit, __ATOMIC_ACQUIRE)) {
        smp_service(lib, 6);        /* 60 ms at rest */
        nanosleep(&tick, NULL);
    }
    return NULL;
}

void smp_start(smp_lib_t *lib) {
    pthread_t *t = malloc(sizeof(*t));
    if (t && pthread_create(t, NULL, loader, lib) == 0) lib->thread = t;
    else free(t);
}

void smp_stop(smp_lib_t *lib) {
    if (lib->thread) {
        __atomic_store_n(&lib->quit, 1, __ATOMIC_RELEASE);
        pthread_join(*(pthread_t *)lib->thread, NULL);
        free(lib->thread);
        lib->thread = NULL;
    }
    for (smp_t *s = lib->cache, *n; s; s = n) n = s->next, smp_free(s);
    lib->cache = NULL, lib->cached = 0;
    for (int i = 0; i < SM_PADS; i++) lib->ready[i] = NULL;
}
