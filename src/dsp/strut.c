/*
 * The v2 plugin entry, the pads, their voices and their keys (strut.h).
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host/plugin_api_v1.h"
#include "strut.h"

_Static_assert(NT_BLOCK == STRUT_MAX_BLOCK, "Resynth makes a whole block at once");

/* ---- the pads ---- */

void strut_init(strut_t *s) {
    wt_build();
    memset(s, 0, sizeof(*s));
    for (int i = 0; i < STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) s->pad[i].p[k] = STRUT_PAD_PARAMS[k].def;
    for (int k = 0; k < G_COUNT; k++) s->g[k] = STRUT_GLOBALS[k].def;
    s->press_at = s->note_at = -1.0;
    s->dither = 0x9E3779B9u;
    s->bpm = 120.0f;
    s->vol_g = -1.0f;
    for (int i = 0; i < STRUT_PADS; i++) s->lib.want[i] = s->lib.seen[i] = -1, s->pad[i].space_g = s->pad[i].level_g = -1.0f;
}

static void focus(strut_t *s, int pad) {
    s->focus = pad;
    s->focus_count++;
}

/* A press and its note cross a process boundary and arrive a few
 * milliseconds apart, in either order. Pair them inside a short window;
 * a note with no press is a sequenced note and moves nothing. */
static void pair(strut_t *s) {
    if (s->press_at < 0 || s->note_at < 0) return;
    if (fabs(s->press_at - s->note_at) <= STRUT_PRESS_WINDOW) {
        focus(s, s->note_pad);
        s->press_at = s->note_at = -1.0;
    }
}

void strut_press(strut_t *s) {
    s->press_at = s->now;
    pair(s);
}

/* ---- DICE (dice.h) ---- */

/* One DICE's step: saves what is there (n floats at cur) into its roll's
 * slot, then moves. 1 if the step is a new roll, for the caller to make. */
static int dice_step(dice_hist_t *h, float *slots, float *cur, size_t n, int way) {
    memcpy(slots + (size_t)(h->at % DICE_SLOTS) * n, cur, n * sizeof(float));
    if (way == DICE_ROLL && h->at == h->newest) {
        h->at = ++h->newest;
        return 1;
    }
    if (way == DICE_ROLL) h->at++;
    else if (h->at > 0 && h->at > h->newest - DICE_KEEP) h->at--;
    memcpy(cur, slots + (size_t)(h->at % DICE_SLOTS) * n, n * sizeof(float));
    return 0;
}

static uint32_t *dice_rng(strut_t *s) {
    /* the hits so far and the time stir it, so no two sessions roll alike */
    s->dice_rng ^= s->seed + (uint32_t)(s->now * STRUT_SR);
    if (!s->dice_rng) s->dice_rng = 0x2545F491u;
    return &s->dice_rng;
}

void strut_dice(strut_t *s, int pad, int way) {
    if (pad >= 0) {
        pad_t *p = &s->pad[pad];
        if (dice_step(&p->dice, &p->rolls[0][0], p->p, P_COUNT, way)) dice_roll(p->p, pad, 0, dice_rng(s));
        p->p[P_DICE] = (float)way;
        return;
    }
    float kit[STRUT_PADS][P_COUNT];
    for (int i = 0; i < STRUT_PADS; i++) memcpy(kit[i], s->pad[i].p, sizeof(kit[i]));
    if (dice_step(&s->dice, &s->rolls[0][0][0], &kit[0][0], STRUT_PADS * P_COUNT, way))
        for (int i = 0; i < STRUT_PADS; i++) dice_roll(kit[i], i, 1, dice_rng(s));
    for (int i = 0; i < STRUT_PADS; i++) {
        memcpy(s->pad[i].p, kit[i], sizeof(kit[i]));
        /* each pad's own rolls start again from the kit's */
        s->pad[i].dice = (dice_hist_t){ 0, 0 };
    }
    s->g[G_DICE] = (float)way;
}

/* A pick (SOUND, a factory kit) is a new step in a DICE's history: what was
 * there is kept, so DICE Back brings it back. Picks in a row, one knob
 * turning, share the step, so a long turn costs one step, not eight. */
static void dice_push(dice_hist_t *h, float *slots, const float *cur, size_t n) {
    memcpy(slots + (size_t)(h->at % DICE_SLOTS) * n, cur, n * sizeof(float));
    h->at = h->newest = h->at + 1;
}

void strut_sound(strut_t *s, int pad, int n, int again) {
    pad_t *p = &s->pad[pad];
    if (n <= 0 || n >= DICE_SOUNDS) return;
    if (!again) dice_push(&p->dice, &p->rolls[0][0], p->p, P_COUNT);
    dice_sound(p->p, n);
}

void strut_load_kit(strut_t *s, int k, int again) {
    if (k <= 0 || k >= DICE_KITS) return;
    float kit[STRUT_PADS][P_COUNT];
    for (int i = 0; i < STRUT_PADS; i++) memcpy(kit[i], s->pad[i].p, sizeof(kit[i]));
    if (!again) dice_push(&s->dice, &s->rolls[0][0][0], &kit[0][0], STRUT_PADS * P_COUNT);
    dice_kit(k, &kit[0][0], s->g);
    for (int i = 0; i < STRUT_PADS; i++) {
        memcpy(s->pad[i].p, kit[i], sizeof(kit[i]));
        s->pad[i].dice = (dice_hist_t){ 0, 0 };
    }
    s->g[G_KIT] = (float)k;
}

/* FLAM's spacing: 2 to 50 ms, nothing at zero. */
static int flam_gap(const float *p) {
    return (int)(0.002f * powf(25.0f, p[P_FLAM]) * STRUT_SR);
}

/* One hit of pad i at strength amp. */
static void strike(strut_t *s, int i, float amp) {
    pad_t *p = &s->pad[i];
    voice_t *v = &p->voice;
    /* CHOKE: the others in this pad's group fade out */
    const int group = (int)p->p[P_CHOKE];
    if (group > 0)
        for (int j = 0; j < STRUT_PADS; j++) {
            pad_t *o = &s->pad[j];
            if (j != i && (int)o->p[P_CHOKE] == group && o->voice.active) {
                if (!o->voice.choke_n) o->voice.choke_n = STRUT_CHOKE;
                o->flams = 0;
            }
        }
    if (!v->active || v->choke_n) {
        /* a pad fading from a choke starts afresh, from silence */
        *v = (voice_t){ 0 };
        v->gs = v->gw = v->gn = -1.0f;
        v->skin.tone_at = -1.0f;
    } else if (v->wave.env > 1e-4f && strut_fader(p->p[P_WAVE]) > 0.0f) {
        /* the Wave note this hit cuts fades out, not clicks off */
        v->old = v->wave;
        v->old_n = STRUT_DECLICK;
    }
    v->active = 1;
    s->seed = s->seed * 1664525u + 1013904223u;
    /* the hit's knobs as the modulators move them at its first instant,
     * so velocity on SNAP, say, shapes the strike itself */
    mod_hit(&v->mod, amp, s->seed ^ 0x6A09E667u);
    float q[P_COUNT];
    mod_out_t mo;
    mo.have_T = 0;
    mod_apply(p->p, &v->mod, s->bpm, q, &mo);
    skin_strike(&v->skin, q, s->seed, amp);
    wave_start(&v->wave, q, amp);
    /* a sample plays only once loaded, and only the one TABLE names */
    const int t = (int)q[P_N_TABLE] - NT_TABLES;
    const smp_t *sm = t >= 0 ? __atomic_load_n(&s->lib.ready[i], __ATOMIC_ACQUIRE) : NULL;
    v->noise.bank = &v->bank;
    noise_start(&v->noise, q, sm && sm->entry == t ? sm : NULL, s->seed, amp);
    /* Wave as Skin's hit: at least one of Wave's cycles, sized from Wave's
     * harmonics near PITCH. The floor keeps the hit itself, heard directly,
     * under full scale. */
    if (v->skin.kind == HIT_WAVE) {
        const int len = wave_strike_len(q, v->skin.len);
        if (len != v->skin.len) v->skin.len = len, v->skin.decay = expf(-5.0f / (float)len);
        skin_resize(&v->skin, 1.0f / fmaxf(wave_strike(q, skin_hz(q), v->skin.decay, len), 0.5f));
    }
    /* Noise as Skin's hit: sized from Noise's colour at PITCH */
    if (v->skin.kind == HIT_NOISE)
        skin_resize(&v->skin, 1.0f / noise_strike(&v->noise, q, skin_hz(q), v->skin.decay, v->skin.len));
}

/* FLAM's three hits rise to the one played: a grace note, a second, then
 * the hit, as a hand claps or a stick flams. */
static float flam_amp(int left) { return left == 2 ? 0.55f : left == 1 ? 0.75f : 1.0f; }

void strut_note_on(strut_t *s, int note, int vel) {
    const int i = note - STRUT_NOTE0;
    if (i < 0 || i >= STRUT_PADS || vel <= 0) return;
    pad_t *p = &s->pad[i];
    const float amp = powf((float)vel / 127.0f, 1.5f);
    if (p->p[P_FLAM] > 0.0f) {
        p->flams = STRUT_FLAMS - 1;
        p->flam_in = flam_gap(p->p);
        p->flam_amp = amp;
        strike(s, i, amp * flam_amp(p->flams));
    } else {
        p->flams = 0;
        strike(s, i, amp);
    }
    s->note_pad = i;
    s->note_at = s->now;
    pair(s);
}

/* A level knob, as a fader: off at zero, then 30 dB of travel to full, so
 * every detent is heard. 0.8 is -6 dB. */
float strut_fader(float x) {
    return x <= 0.0f ? 0.0f : powf(10.0f, 1.5f * (fminf(x, 1.0f) - 1.0f));
}

/* A block's sample loop for one set of the engines. Inlined with constant
 * np (Skin's partials) and which engines run, so each set is its own loop
 * that decides nothing per sample; and run on copies of the voice's state,
 * which the compiler can keep in registers. (Through the voice itself, every
 * sample reread and rewrote it: out might have aliased it.) */
typedef struct { float gs, ds, gw, dw, gn, dn; } gains_t;

typedef struct {
    skin_block_t s;
    wave_block_t w;
    noise_block_t n;
} blocks_t;

static inline __attribute__((always_inline)) void voice_loop(
    voice_t *restrict v, const blocks_t *restrict b, gains_t g, float *restrict out, int frames,
    const int np, const int skin, const int wave, const int noise) {
    skin_voice_t k = v->skin;
    wave_voice_t w = v->wave;
    noise_voice_t z = v->noise;
    const int by_noise = k.kind == HIT_NOISE;
    float rw = 0.0f, rn = 0.0f;
    for (int n = 0; n < frames; n++) {
        const float t = (float)(n + 1);
        float y = 0.0f;
        if (noise) y += (g.gn + g.dn * t) * noise_step(&z, &b->n, n, &rn);
        if (wave) y += (g.gw + g.dw * t) * wave_step(&w, &b->w, n, skin ? skin_body(&k) : 0.0f, &rw);
        if (skin) y += (g.gs + g.ds * t) * skin_step(&k, &b->s, by_noise ? rn : rw, np);
        out[n] += y;
    }
    v->skin = k;
    v->wave = w;
    v->noise = z;
}

/* Each set of engines that can run, as its own loop. */
static void voice_loops(voice_t *restrict v, const blocks_t *restrict b, gains_t g, float *restrict out,
                        int frames, int skin, int wave, int noise) {
    const int set = (skin ? (b->s.np == 1 ? 1 : 2) : 0) * 4 + wave * 2 + noise;
    switch (set) {
    case 1: voice_loop(v, b, g, out, frames, 0, 0, 0, 1); break;
    case 2: voice_loop(v, b, g, out, frames, 0, 0, 1, 0); break;
    case 3: voice_loop(v, b, g, out, frames, 0, 0, 1, 1); break;
    case 4: voice_loop(v, b, g, out, frames, 1, 1, 0, 0); break;
    case 5: voice_loop(v, b, g, out, frames, 1, 1, 0, 1); break;
    case 6: voice_loop(v, b, g, out, frames, 1, 1, 1, 0); break;
    case 7: voice_loop(v, b, g, out, frames, 1, 1, 1, 1); break;
    case 8: voice_loop(v, b, g, out, frames, SKIN_PARTIALS, 1, 0, 0); break;
    case 9: voice_loop(v, b, g, out, frames, SKIN_PARTIALS, 1, 0, 1); break;
    case 10: voice_loop(v, b, g, out, frames, SKIN_PARTIALS, 1, 1, 0); break;
    case 11: voice_loop(v, b, g, out, frames, SKIN_PARTIALS, 1, 1, 1); break;
    default: break;
    }
}

/* A level, gliding from the last block's. */
static void glide(float *last, float now, int frames, float *g, float *d) {
    *g = *last < 0.0f ? now : *last;
    *d = (now - *g) / (float)frames;
    *last = now;
}

/* One voice's stretch of samples, added into out; 0 once it has nothing
 * left to say. p is the pad's knobs as the modulators have moved them, and
 * mo what CURVE and Level make of each engine now. Skin runs while it
 * rings; Wave and Noise while they are heard or strike Skin. Wave follows
 * Skin's ring a sample late, which is what lets each feed the other (Skin
 * struck by Wave, Wave bent by Skin) without a loop. */
static int voice_chunk(voice_t *restrict v, const float *restrict p, float level, const mod_out_t *mo,
                       float *restrict out, int frames) {
    /* CURVE has ended an engine: what is left of it stops */
    if (mo->over[ENG_SKIN]) {
        for (int k = 0; k < SKIN_PARTIALS; k++) v->skin.zr[k] = v->skin.zi[k] = 0.0f;
        v->skin.n = v->skin.len;
    }
    if (mo->over[ENG_WAVE]) v->wave.env = 0.0f;
    if (mo->over[ENG_NOISE]) v->noise.env = 0.0f;
    gains_t g;
    glide(&v->gs, 2.4f * level * strut_fader(p[P_SKIN]) * mo->gain[ENG_SKIN], frames, &g.gs, &g.ds);
    glide(&v->gw, 2.4f * level * strut_fader(p[P_WAVE]) * mo->gain[ENG_WAVE], frames, &g.gw, &g.dw);
    glide(&v->gn, 2.4f * level * strut_fader(p[P_NOISE]) * mo->gain[ENG_NOISE], frames, &g.gn, &g.dn);
    const float gw1 = v->gw, gn1 = v->gn;

    const int skin_on = skin_alive(&v->skin);
    const int strikes = v->skin.n < v->skin.len;
    const int wave_on = g.gw > 0.0f || gw1 > 0.0f || (strikes && v->skin.kind == HIT_WAVE);
    const int noise_on = g.gn > 0.0f || gn1 > 0.0f || (strikes && v->skin.kind == HIT_NOISE);
    blocks_t b;
    int heard_w = 0, heard_n = 0;
    if (skin_on) skin_block(&v->skin, p, mo->hold[ENG_SKIN], &b.s);
    if (wave_on) heard_w = wave_block(&v->wave, p, frames, mo->hold[ENG_WAVE], &b.w) && gw1 > 0.0f;
    else wave_skip(&v->wave, p, frames, mo->hold[ENG_WAVE]);
    if (noise_on) heard_n = noise_block(&v->noise, p, frames, mo->hold[ENG_NOISE], &b.n) && gn1 > 0.0f;
    else noise_skip(&v->noise, p, frames, mo->hold[ENG_NOISE]);
    /* the note a hit cut, fading out underneath: a few milliseconds */
    if (v->old_n > 0) {
        wave_block_t ob;
        wave_block(&v->old, p, frames, 0, &ob);
        const int n1 = v->old_n < frames ? v->old_n : frames;
        float raw;
        for (int n = 0; n < n1; n++)
            out[n] += (g.gw + g.dw * (float)(n + 1)) * (float)(v->old_n - n) * (1.0f / STRUT_DECLICK)
                    * wave_step(&v->old, &ob, n, 0.0f, &raw);
        v->old_n -= n1;
    }
    /* with Skin silent, only what is heard runs */
    voice_loops(v, &b, g, out, frames, skin_on, wave_on && (skin_on || heard_w), noise_on && (skin_on || heard_n));
    return skin_alive(&v->skin) || heard_w || heard_n || v->old_n > 0;
}

/* One voice's block. While nothing moves, the whole block at once; while
 * a modulator or CURVE does, a stretch of MOD_SUB samples at a time, the
 * knobs moved anew for each. */
static int voice_render(voice_t *restrict v, const float *restrict p, float level, float bpm,
                        float *restrict out, int frames) {
    static const mod_out_t still = { { 1.0f, 1.0f, 1.0f }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, 0 };
    int alive = 1;
    if (!mod_any(p)) {
        alive = voice_chunk(v, p, level, &still, out, frames);
    } else {
        float q[P_COUNT];
        mod_out_t mo;
        mo.have_T = 0;
        for (int done = 0; done < frames && alive; done += MOD_SUB) {
            const int n = frames - done < MOD_SUB ? frames - done : MOD_SUB;
            mod_apply(p, &v->mod, bpm, q, &mo);
            alive = voice_chunk(v, q, level, &mo, out + done, n);
            v->mod.t += (float)n / STRUT_SR;
        }
        return alive;
    }
    v->mod.t += (float)frames / STRUT_SR;
    return alive;
}

/* One pad's block, or part of one, through its finish into l and r. */
static void pad_render(strut_t *s, pad_t *p, float *l, float *r, float *send, int frames) {
    voice_t *v = &p->voice;
    if (!v->active) return;
    float x[STRUT_MAX_BLOCK] = { 0 };
    /* the engines mix at LEVEL's default; LEVEL itself comes after the
     * finish, so it turns down a pad DRIVE has pushed to its ceiling */
    v->active = voice_render(v, p->p, LEVEL_REF, s->bpm, x, frames);
    /* choked: a straight fade to nothing, then the voice is done */
    if (v->choke_n) {
        for (int n = 0; n < frames; n++) {
            x[n] *= (float)(v->choke_n > n ? v->choke_n - n : 0) * (1.0f / STRUT_CHOKE);
        }
        v->choke_n -= frames;
        if (v->choke_n <= 0) v->active = 0;
    }
    /* A voice gone to inf or NaN would keep itself alive and, mixed in,
     * silence the room and every pad after it until Strut is reloaded.
     * Drop it instead, so the next hit starts from silence, and say so. */
    float sum = 0.0f;
    for (int n = 0; n < frames; n++) sum += x[n] * x[n];
    if (!isfinite(sum)) {
        v->active = 0;
        s->healed |= 1u << (p - s->pad);
        return;
    }
    finish_block_t fb;
    finish_block(p->p, &fb);
    float pl[STRUT_MAX_BLOCK] = { 0 }, pr[STRUT_MAX_BLOCK] = { 0 };
    finish_run(&v->fx, &fb, x, pl, pr, frames);
    /* the pad at its LEVEL, and its share of the room by its SPACE, gliding */
    const float s1 = p->p[P_SPACE], s0 = p->space_g < 0.0f ? s1 : p->space_g;
    const float g1 = strut_fader(p->p[P_LEVEL]) / LEVEL_REF, g0 = p->level_g < 0.0f ? g1 : p->level_g;
    p->space_g = s1, p->level_g = g1;
    for (int n = 0; n < frames; n++) {
        const float t = (float)(n + 1) / (float)frames, g = g0 + (g1 - g0) * t;
        l[n] += g * pl[n], r[n] += g * pr[n];
        send[n] += 0.5f * g * (pl[n] + pr[n]) * (s0 + (s1 - s0) * t);
    }
    s->sounding++;
}

void strut_render(strut_t *s, float *l, float *r, int frames) {
    memset(l, 0, sizeof(float) * frames);
    memset(r, 0, sizeof(float) * frames);
    memset(s->send, 0, sizeof(float) * frames);
    s->sounding = 0;
    /* tell the loader the samples the pads name */
    for (int i = 0; i < STRUT_PADS; i++) {
        const int t = (int)s->pad[i].p[P_N_TABLE] - NT_TABLES, w = t >= 0 ? t : -1;
        if (w != s->lib.want[i]) __atomic_store_n(&s->lib.want[i], w, __ATOMIC_RELAXED);
        const int m = (int)s->pad[i].p[P_N_MODE];
        if (m != s->lib.mode[i]) __atomic_store_n(&s->lib.mode[i], m, __ATOMIC_RELAXED);
    }
    for (int i = 0; i < STRUT_PADS; i++) {
        pad_t *p = &s->pad[i];
        /* FLAM's hits still to come, each at its sample */
        int done = 0;
        while (p->flams > 0 && p->flam_in < frames - done) {
            pad_render(s, p, l + done, r + done, s->send + done, p->flam_in);
            done += p->flam_in;
            p->flams--;
            strike(s, i, p->flam_amp * flam_amp(p->flams));
            p->flam_in = flam_gap(p->p);
        }
        if (p->flams > 0) p->flam_in -= frames - done;
        pad_render(s, p, l + done, r + done, s->send + done, frames - done);
        /* and the sample each voice plays, so it is not freed under it */
        const smp_t *u = p->voice.active ? p->voice.noise.smp : NULL;
        if (u != s->lib.used[i]) __atomic_store_n(&s->lib.used[i], u, __ATOMIC_RELEASE);
    }
    __atomic_store_n(&s->lib.blocks, s->lib.blocks + 1, __ATOMIC_RELEASE);
    s->now += (double)frames / STRUT_SR;
}

void strut_kit(strut_t *s, float *l, float *r, int frames) {
    if (kit_run(&s->kit, s->g, s->send, l, r, frames)) s->sounding++;
}

/* ---- the output (Quilt's, src/dsp/quilt.c) ---- */

/* Triangular noise of one 16-bit step. */
static inline float dither(uint32_t *d) {
    *d ^= *d << 13, *d ^= *d >> 17, *d ^= *d << 5;
    const float a = (float)(*d & 0xFFFF), b = (float)(*d >> 16);
    return (a + b) * (1.0f / 65536.0f) - 1.0f;
}

/* Soft above 0.7 of full scale, so a stack of pads rounds off rather than
 * clips. (Rejected: soft above half scale, as to 0.11.1: with the make-up
 * below, every kick's peak would have been squashed.) */
static inline float limit(float x) {
    float a = fabsf(x);
    if (a > 0.7f) a = 0.7f + 0.3f * tanhf((a - 0.7f) * (1.0f / 0.3f));
    return copysignf(a, x);
}

/* The rounding's noise, shaped to the ear. The error each rounding makes is
 * fed back through these taps, so the noise left is filtered by 1 + sum h z^-k:
 * pushed out of 1 to 6 kHz, where hearing is keenest, up towards 20 kHz.
 * Designed by tools/noise_shape.py: the threshold of hearing (Terhardt 1979)
 * as the target, zero mean log gain (Gerzon and Craven 1989), made minimum
 * phase and cut to nine taps. Heard about 11 dB quieter than plain dither,
 * though it carries about 8 dB more power, all of it high. */
static const float SHAPE[STRUT_SHAPE] = {
    -1.69743f, 1.28417f, -0.07158f, -0.47574f, 0.2754f, 0.26476f, -0.182f, -0.0499f,
};

/* One sample to 16 bits: dg steps of triangular dither, and the shaped
 * feedback of e, the last errors, newest first. */
static inline int16_t to16(float x, float dg, uint32_t *d, float *e) {
    float fb = 0.0f;
    for (int k = 0; k < STRUT_SHAPE; k++) fb += SHAPE[k] * e[k];
    const float v = x * 32000.0f + dg * fb;
    float q = rintf(v + dg * dither(d));
    q = fminf(fmaxf(q, -32768.0f), 32767.0f);
    memmove(e + 1, e, sizeof(float) * (STRUT_SHAPE - 1));
    e[0] = q - v;       /* so the noise is the error filtered by 1 + sum h z^-k */
    return (int16_t)q;
}

/* VOL, then 16 bits, rounded with a step of triangular dither. Without it a
 * fading tail's last few steps become a gritty distortion whose harmonics
 * fold back down (heard on Quilt, 2026-10-05). The dither stays at full depth
 * while any voice sounds, because the grit lives in those last steps (Quilt's
 * fade below eight steps left it 11 dB proud there), and fades out over a
 * block once all have ended, so the kit rests in true silence. VOL glides
 * across the block, as the pads' levels do. */
void strut_output(strut_t *s, const float *l, const float *r, int16_t *out, int frames) {
    const float vol = s->g[G_VOL];
    const float g1 = vol <= -59.9f ? 0.0f : STRUT_MAKEUP * powf(10.0f, vol / 20.0f);
    const float g0 = s->vol_g < 0.0f ? g1 : s->vol_g;
    s->vol_g = g1;
    const float d0 = s->dither_g, d1 = s->sounding && g1 > 0.0f ? 1.0f : 0.0f;
    s->dither_g = d1;
    /* resting: forget the shaper's errors, so silence is exactly zero */
    if (d0 == 0.0f && d1 == 0.0f) memset(s->shape, 0, sizeof(s->shape));
    for (int i = 0; i < frames; i++) {
        const float x = (float)(i + 1) / (float)frames;
        const float g = g0 + (g1 - g0) * x, dg = d0 + (d1 - d0) * x;
        out[2 * i] = to16(limit(l[i] * g), dg, &s->dither, s->shape[0]);
        out[2 * i + 1] = to16(limit(r[i] * g), dg, &s->dither, s->shape[1]);
    }
}

/* ---- keys ---- */

/* "p05_s_pitch" -> pad 4, P_S_PITCH; -1 if it is not a pad key. */
static int pad_key(const char *key, int *pad) {
    if (key[0] != 'p' || key[1] < '0' || key[1] > '9' || key[2] < '0' || key[2] > '9' || key[3] != '_') return -1;
    const int n = (key[1] - '0') * 10 + (key[2] - '0');
    if (n < 1 || n > STRUT_PADS) return -1;
    for (int k = 0; k < P_COUNT; k++)
        if (!strcmp(key + 4, STRUT_PAD_PARAMS[k].key)) { *pad = n - 1; return k; }
    return -1;
}

static int global_key(const char *key) {
    for (int k = 0; k < G_COUNT; k++)
        if (!strcmp(key, STRUT_GLOBALS[k].key)) return k;
    return -1;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* An enum arrives as its option name or its index; both are accepted.
 * Anything else leaves the value alone. */
static void write_value(const param_def_t *d, float *v, const char *val) {
    if (d->kind == PK_ENUM) {
        const int n = param_noptions(d);
        for (int i = 0; i < n; i++)
            if (!strcmp(val, param_option(d, i))) { *v = (float)i; return; }
        char *end;
        const long i = strtol(val, &end, 10);
        if (end != val && *end == '\0' && i >= 0 && i < n) *v = (float)i;
        return;
    }
    char *end;
    const float x = strtof(val, &end);
    if (end == val) return;
    *v = clampf(d->kind == PK_INT ? roundf(x) : x, d->min, d->max);
}

static int read_value(const param_def_t *d, float v, char *buf, int len) {
    if (d->kind == PK_ENUM) return snprintf(buf, len, "%s", param_option(d, (int)v));
    if (d->kind == PK_INT) return snprintf(buf, len, "%d", (int)v);
    return snprintf(buf, len, "%.4f", (double)v);
}

/* ---- state ----
 *
 * The host saves a slot by reading `state` and loads it by writing it back
 * (on its loader thread, before any note). It is one flat JSON object of the
 * same keys set_param takes, holding only what differs from the defaults:
 *
 *   {"v":1,"p01_s_pitch":"0.3125","p03_n_table":"Kick 003","size":"0.2000"}
 *
 * Values are written as get_param serves them, so an enum is its option
 * name and a sample comes back by name even if the user's folder changes.
 * DICE is a turn, not a value, the MOD switches only choose a view, and Kit >
 * CHOKE is the focused pad's own, so none is kept: loading a kit never
 * rolls. Reading starts from the
 * defaults and ignores keys it does not know, so older and newer saves load. */

static int saved(int pad, int k) {
    if (pad >= 0) return k != P_DICE;
    return k != G_DICE && k != G_CHOKE && k != G_SKIN_VIEW && k != G_WAVE_VIEW && k != G_NOISE_VIEW;
}

static int put_value(char *buf, int len, int at, const char *key, const char *val) {
    if (at < 0) return -1;
    int n = snprintf(buf + at, (size_t)(len - at), ",\"%s\":\"", key);
    if (n < 0 || at + n >= len) return -1;
    at += n;
    for (const char *c = val; *c; c++) {
        if (at + 3 >= len) return -1;
        if (*c == '"' || *c == '\\') buf[at++] = '\\';
        buf[at++] = *c;
    }
    buf[at++] = '"';
    buf[at] = '\0';
    return at;
}

static int write_state(const strut_t *s, char *buf, int len) {
    char key[64], val[256];
    int at = snprintf(buf, (size_t)len, "{\"v\":1");
    if (at < 0 || at >= len) return -1;
    for (int i = 0; i < STRUT_PADS; i++)
        for (int k = 0; k < P_COUNT; k++) {
            const param_def_t *d = &STRUT_PAD_PARAMS[k];
            if (!saved(i, k) || s->pad[i].p[k] == d->def) continue;
            snprintf(key, sizeof(key), "p%02d_%s", i + 1, d->key);
            read_value(d, s->pad[i].p[k], val, sizeof(val));
            at = put_value(buf, len, at, key, val);
        }
    for (int k = 0; k < G_COUNT; k++) {
        const param_def_t *d = &STRUT_GLOBALS[k];
        if (!saved(-1, k) || s->g[k] == d->def) continue;
        read_value(d, s->g[k], val, sizeof(val));
        at = put_value(buf, len, at, d->key, val);
    }
    if (at < 0 || at + 2 > len) return -1;
    buf[at++] = '}';
    buf[at] = '\0';
    return at;
}

/* One "key":value pair from p, unescaped; a number is taken as written.
 * Returns where the next pair starts, or NULL at the end. */
static const char *next_pair(const char *p, char *key, int klen, char *val, int vlen) {
    while (*p && *p != '"' && *p != '}') p++;
    if (*p != '"') return NULL;
    int n = 0;
    for (p++; *p && *p != '"'; p++)
        if (n < klen - 1) key[n++] = *p;
    key[n] = '\0';
    if (!*p) return NULL;
    for (p++; *p == ' ' || *p == ':'; p++) {}
    n = 0;
    if (*p == '"') {
        for (p++; *p && *p != '"'; p++) {
            if (*p == '\\' && p[1]) p++;
            if (n < vlen - 1) val[n++] = *p;
        }
        if (!*p) return NULL;
        p++;
    } else {
        for (; *p && *p != ',' && *p != '}'; p++)
            if (n < vlen - 1 && *p != ' ') val[n++] = *p;
    }
    val[n] = '\0';
    return p;
}

static void read_state(strut_t *s, const char *json) {
    const char *p = strchr(json, '{');
    if (!p) return;
    for (int i = 0; i < STRUT_PADS; i++) {
        for (int k = 0; k < P_COUNT; k++) s->pad[i].p[k] = STRUT_PAD_PARAMS[k].def;
        s->pad[i].dice = (dice_hist_t){ 0, 0 };
    }
    for (int k = 0; k < G_COUNT; k++)
        if (saved(-1, k)) s->g[k] = STRUT_GLOBALS[k].def;
    s->dice = (dice_hist_t){ 0, 0 };
    char key[64], val[256];
    int pad, k;
    while ((p = next_pair(p, key, sizeof(key), val, sizeof(val)))) {
        if ((k = pad_key(key, &pad)) >= 0 && saved(pad, k))
            write_value(&STRUT_PAD_PARAMS[k], &s->pad[pad].p[k], val);
        else if ((k = global_key(key)) >= 0 && saved(-1, k))
            write_value(&STRUT_GLOBALS[k], &s->g[k], val);
    }
}

/* ---- the v2 API ---- */

static const host_api_v1_t *g_host;

static void *create_instance(const char *module_dir, const char *json_defaults) {
    (void)json_defaults;
    smp_catalogue(module_dir);      /* TABLE's list, once for every instance */
    strut_t *s = calloc(1, sizeof(*s));
    if (s) strut_init(s), smp_start(&s->lib);
    /* Reloading a module the host still holds hands back the old code (the
     * host opens the new synth before closing the old, and dlopen() matches
     * by path), so the log says which build is really playing. */
    if (g_host && g_host->log) g_host->log("strut " STRUT_VERSION " loaded");
    return s;
}

static void destroy_instance(void *instance) {
    strut_t *s = instance;
    if (s) smp_stop(&s->lib);
    free(s);
}

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    if (len < 3) return;
    if ((msg[0] & 0xF0) == 0x90) strut_note_on(instance, msg[1], msg[2]);
}

static void set_param(void *instance, const char *key, const char *val) {
    strut_t *s = instance;
    int pad, k;
    /* a pad press or focus move between two picks still counts as one turn */
    const int was = s->picking;
    if (strcmp(key, "pad") && strcmp(key, "pad_press")) s->picking = 0;
    if (!strcmp(key, "pad")) {
        const int n = atoi(val);
        if (n >= 1 && n <= STRUT_PADS) focus(s, n - 1);
    } else if (!strcmp(key, "pad_press")) {
        strut_press(s);
    } else if (!strcmp(key, "state")) {
        read_state(s, val);
    } else if ((k = pad_key(key, &pad)) >= 0 && k == P_SOUND) {
        float n = 0;
        write_value(&STRUT_PAD_PARAMS[k], &n, val);
        strut_sound(s, pad, (int)n, was == pad + 1);
        s->picking = pad + 1;
    } else if (k >= 0) {
        write_value(&STRUT_PAD_PARAMS[k], &s->pad[pad].p[k], val);
        if (k == P_DICE) strut_dice(s, pad, (int)s->pad[pad].p[k]);
    } else if ((k = global_key(key)) >= 0) {
        write_value(&STRUT_GLOBALS[k], k == G_CHOKE ? &s->pad[s->focus].p[P_CHOKE] : &s->g[k], val);
        if (k == G_DICE) strut_dice(s, -1, (int)s->g[k]);
        if (k == G_KIT) strut_load_kit(s, (int)s->g[k], was == -1), s->picking = -1;
    }
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    strut_t *s = instance;
    int pad, k;
    /* the host asks often, off the voices' path, so a dropped sound is told here */
    if (s->healed) {
        char msg[96];
        snprintf(msg, sizeof(msg), "strut: dropped a sound gone bad (pads bitmask 0x%x, bit 16 the room)", s->healed);
        if (g_host && g_host->log) g_host->log(msg);
        s->healed = 0;
    }
    if (!strcmp(key, "ui_hierarchy")) return strut_contract_hierarchy(buf, buf_len);
    if (!strcmp(key, "chain_params")) return strut_contract_params(buf, buf_len);
    if (!strcmp(key, "pad")) return snprintf(buf, buf_len, "%d", s->focus + 1);
    if (!strcmp(key, "state")) return write_state(s, buf, buf_len);
    if ((k = pad_key(key, &pad)) >= 0) return read_value(&STRUT_PAD_PARAMS[k], s->pad[pad].p[k], buf, buf_len);
    if ((k = global_key(key)) >= 0)
        return read_value(&STRUT_GLOBALS[k], k == G_CHOKE ? s->pad[s->focus].p[P_CHOKE] : s->g[k], buf, buf_len);
    return -1;
}

static int get_error(void *instance, char *buf, int buf_len) {
    (void)instance;
    (void)buf;
    (void)buf_len;
    return 0;
}

static void render_block(void *instance, int16_t *out, int frames) {
    strut_t *s = instance;
#if defined(__aarch64__)
    /* Flush denormals to zero while Strut renders, so a long tail never falls
     * into slow subnormal arithmetic; the host's own mode is put back after. */
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr | (1ull << 24)));
#endif
    float l[STRUT_MAX_BLOCK], r[STRUT_MAX_BLOCK];
    /* the tempo, for RATE's synced half; 120 if the host does not say */
    if (g_host && g_host->get_bpm) {
        const float bpm = g_host->get_bpm();
        if (bpm >= 20.0f && bpm <= 400.0f) s->bpm = bpm;
    }
    for (int done = 0; done < frames;) {
        const int n = frames - done < STRUT_MAX_BLOCK ? frames - done : STRUT_MAX_BLOCK;
        strut_render(s, l, r, n);
        strut_kit(s, l, r, n);
        float sum = 0.0f;   /* the same for the room and the finish, which every pad passes through */
        for (int i = 0; i < n; i++) sum += l[i] * l[i] + r[i] * r[i];
        if (!isfinite(sum)) {
            memset(&s->kit, 0, sizeof(s->kit));
            for (int i = 0; i < STRUT_PADS; i++) s->pad[i].voice.active = 0;
            memset(l, 0, sizeof(float) * (size_t)n);
            memset(r, 0, sizeof(float) * (size_t)n);
            s->healed |= 1u << STRUT_PADS;
        }
        strut_output(s, l, r, out + 2 * done, n);
        done += n;
    }
#if defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" ::"r"(fpcr));
#endif
}

static plugin_api_v2_t api = {
    .api_version = 2,
    .create_instance = create_instance,
    .destroy_instance = destroy_instance,
    .on_midi = on_midi,
    .set_param = set_param,
    .get_param = get_param,
    .get_error = get_error,
    .render_block = render_block,
};

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
    g_host = host;
    return &api;
}
