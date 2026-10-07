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

/* Room for every copy: each with two samples before and five after, as
 * nt_read reads. */
static smp_t *alloc_smp(uint32_t len) {
    size_t n = 0;
    for (int l = 0; l < NT_LEVELS; l++) n += (size_t)levels_n(len, l) + 7;
    smp_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->data = calloc(n, sizeof(int16_t));
    if (!s->data) { free(s); return NULL; }
    s->bytes = n * sizeof(int16_t) + sizeof(*s);
    s->len = len;
    int16_t *q = s->data + 2;
    for (int l = 0; l < NT_LEVELS; l++) s->t.level[l] = q, q += levels_n(len, l) + 7;
    return s;
}

/* Each quarter octave's share of the power, as Noise's (noise.c). */
static void add_band(float *band, float hz, float pw) {
    const int b = (int)floorf(4.0f * log2f(hz / 20.0f));
    if (b >= 0 && b < NT_BANDS) band[b] += pw;
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
    /* each copy an octave down from the last */
    int m = (int)len;
    float *src = y, *dst = b + DN_M;
    for (int l = 1; l < NT_LEVELS; l++) {
        const int h = levels_n(len, l);
        for (int i = 0; i < h; i++) {
            const float *c = src + 2 * i;
            float v = dn_c[0] * c[0];
            for (int k = 1; k <= DN_M; k++) v += dn_c[k] * ((2 * i - k >= 0 ? c[-k] : 0) + (2 * i + k < m ? c[k] : 0));
            dst[i] = v;
        }
        store((int16_t *)s->t.level[l], dst, h, 32767.0f);
        float *t = src;
        src = dst, dst = t, m = h;
    }
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

void smp_free(smp_t *s) {
    if (s) free(s->data), free(s);
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
        if (w != lib->seen[i]) lib->seen[i] = w, lib->still[i] = 0;
        else if (lib->still[i] < 1 << 20) lib->still[i]++;
        const smp_t *r = lib->ready[i];
        if (w < 0 || w >= count) {
            if (r) __atomic_store_n(&lib->ready[i], NULL, __ATOMIC_RELEASE);
            continue;
        }
        /* a knob turning through the list loads nothing until it rests */
        if ((r && r->entry == w) || lib->still[i] < patience || lib->still[i] < 0) continue;
        smp_t *s = lib->cache;
        while (s && s->entry != w) s = s->next;
        if (!s && (s = smp_load(w))) {
            s->next = lib->cache, lib->cache = s;
            lib->cached += s->bytes;
        }
        if (!s) { lib->still[i] = -(1 << 20); continue; }    /* unreadable: not tried again till TABLE moves */
        s->dead = 0;
        __atomic_store_n(&lib->ready[i], s, __ATOMIC_RELEASE);
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
