/*
 * demo: renders a few hand-set Skin and Wave sounds, one after another, then a short
 * groove made of them, to a WAV file, for listening away from the Move.
 *   tools/demo <out.wav>
 * The sounds are starting points for the voicing pass, not presets.
 */
#include <stdio.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "strut.h"

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static const struct { const char *name, *knobs; } SOUNDS[] = {
    { "deep kick (Skin)", "s_pitch=-7 s_ring=0.75 s_mode=Low s_hit=Click s_snap=0.2 s_tone=0.5" },
    { "snare body (Skin)", "s_pitch=24 s_ring=0.3 s_mode=High s_hit=Burst s_snap=0.55 s_metal=0.25 s_tone=0.8" },
    { "rim (Skin)", "s_pitch=40 s_ring=0.15 s_mode=Band s_hit=Click s_snap=0 s_tone=1" },
    { "cowbell (Skin)", "s_pitch=38 s_ring=0.45 s_mode=Band s_hit=Click s_snap=0.1 s_metal=0.75 s_tone=0.8" },
    { "sine kick (Wave)", "skin=0 wave=0.9 w_wave=0 w_pitch=-5 w_bend=0.45 w_decay=0.55" },
    { "zap (Wave)", "skin=0 wave=0.75 w_wave=0.5 w_pitch=36 w_bend=0.9 w_decay=0.45" },
    { "sync blip (Wave)", "skin=0 wave=0.75 w_table=Sync w_wave=0.7 w_pitch=24 w_bend=0.3 w_decay=0.25" },
    { "vowel tom (Wave)", "skin=0 wave=0.85 w_table=Vowel w_wave=0.4 w_pitch=12 w_bend=0.35 w_decay=0.5" },
    { "glass bell (Wave, ring)", "skin=0 wave=0.75 w_table=Glass w_wave=0.6 w_pitch=36 w_ring=0.35 w_decay=0.8" },
    { "fold kick (Skin + Wave)", "s_pitch=-5 s_ring=0.4 s_mode=Low skin=0.6 wave=0.8 w_table=Fold w_wave=0.5 w_pitch=-3 w_bend=0.5 w_decay=0.5" },
    { "FM tom (Skin bends Wave)", "s_pitch=7 s_ring=0.6 s_mode=Band skin=0.7 wave=0.8 w_pitch=19 w_fm=0.5 w_decay=0.5" },
    { "metal snare (Wave strikes Skin)", "s_hit=Wave s_pitch=26 s_ring=0.35 s_mode=High s_snap=0.5 s_metal=0.4 wave=0.3 w_table=Metal w_wave=0.7 w_pitch=40 w_decay=0.2" },
};
#define NSOUNDS (int)(sizeof(SOUNDS) / sizeof(SOUNDS[0]))

/* 16 steps a bar; each step lists the sounds it hits (bit i = sound i). */
static const unsigned GROOVE[16] = {
    1 << 9, 0, 1 << 2, 1 << 6, 1 << 11, 0, 1 << 3, 1 << 9,
    1 << 4, 1 << 2, 1 << 6, 1 << 10, 1 << 11, 1 << 5, 1 << 7, 1 << 8,
};

static FILE *wav;
static long frames_written;

static void put16(int v) { fputc(v & 0xff, wav); fputc((v >> 8) & 0xff, wav); }
static void put32(long v) { put16((int)(v & 0xffff)); put16((int)((v >> 16) & 0xffff)); }

static void render(plugin_api_v2_t *a, void *p, double seconds) {
    int16_t out[256];
    for (long n = (long)(seconds * STRUT_SR); n > 0; n -= 128) {
        a->render_block(p, out, 128);
        fwrite(out, sizeof(int16_t), 256, wav);
        frames_written += 128;
    }
}

static void hit(plugin_api_v2_t *a, void *p, int pad, int vel) {
    const uint8_t on[3] = { 0x90, (uint8_t)(STRUT_NOTE0 + pad), (uint8_t)vel };
    a->on_midi(p, on, 3, 0);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: demo <out.wav>\n"); return 1; }
    plugin_api_v2_t *a = move_plugin_init_v2(NULL);
    void *p = a->create_instance(".", "");
    for (int i = 0; i < NSOUNDS; i++) {
        char knobs[256], key[48];
        snprintf(knobs, sizeof(knobs), "%s", SOUNDS[i].knobs);
        for (char *t = strtok(knobs, " "); t; t = strtok(NULL, " ")) {
            char *eq = strchr(t, '=');
            *eq = '\0';
            snprintf(key, sizeof(key), "p%02d_%s", i + 1, t);
            a->set_param(p, key, eq + 1);
        }
    }
    wav = fopen(argv[1], "wb");
    if (!wav) return 1;
    fwrite("RIFF\0\0\0\0WAVEfmt ", 1, 16, wav);
    put32(16); put16(1); put16(2); put32(STRUT_SR); put32(STRUT_SR * 4); put16(4); put16(16);
    fwrite("data\0\0\0\0", 1, 8, wav);

    for (int i = 0; i < NSOUNDS; i++) {
        printf("%4.1f s  %s\n", (double)frames_written / STRUT_SR, SOUNDS[i].name);
        hit(a, p, i, 100);
        render(a, p, 1.2);
    }
    printf("%4.1f s  groove, 104 bpm\n", (double)frames_written / STRUT_SR);
    const double step = 60.0 / 104 / 4;
    for (int bar = 0; bar < 4; bar++)
        for (int s = 0; s < 16; s++) {
            for (int i = 0; i < NSOUNDS; i++)
                if (GROOVE[s] & (1u << i)) hit(a, p, i, (s % 4) ? 80 : 115);
            render(a, p, step);
        }
    render(a, p, 1.5);

    const long bytes = frames_written * 4;
    fseek(wav, 4, SEEK_SET); put32(36 + bytes);
    fseek(wav, 40, SEEK_SET); put32(bytes);
    fclose(wav);
    a->destroy_instance(p);
    return 0;
}
