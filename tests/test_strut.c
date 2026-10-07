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
    focus();
    printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "ok", checks, fails);
    return fails ? 1 : 0;
}
