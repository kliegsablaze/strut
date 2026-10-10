/*
 * Strut's knob pictures (DESIGN.md, Knob pictures). Schwung loads this file
 * for any module whose chain_params name a `custom:` viz kind, and calls
 * drawCell for each cell with a frame whose (0,0) is the cell. A pad page's
 * keys arrive bare ("tune", not "p05_tune"), so one picture serves every pad.
 *
 * The same rules as Quilt's, so the two read as one hand:
 *
 *   SMALL AND CENTRED. Each picture is drawn in a 32x12 design space and
 *     scaled to 80% about the cell's centre before it becomes pixels, so it
 *     sits in the middle of its cell with room around it.
 *   ONE PEN. Everything is a one-pixel line. What rings (a drumhead, a
 *     wave) is a plain line, what plays it (stick, mallet, brush) an
 *     outline, and noise is dotted.
 *   MOTION MEANS TIME. Only controls that are about time move: the DECAYs,
 *     Skin's RING, the modulators' RATE and SPACE. They move while their
 *     knob is touched (when the host says so) or turned, and hold still
 *     otherwise. Every value eases to a new one in 140 ms.
 *
 * A still picture is drawn once and replayed; see pen().
 * The frame is a 32x15 Movy cell.
 */
(function () {
const W = 32, PIC = 11;
/* How long a time control keeps moving after its knob was last turned. */
const AWAKE_MS = 1500;
const clamp01 = (v) => (v < 0 ? 0 : v > 1 ? 1 : v);
const TAU = Math.PI * 2;

/* ---------------------------------------------------------------- memory -- */
/* The renderer is stateless; these remember just enough to ease a value and
 * to keep a running phase continuous when its rate changes. */
const tweens = new Map();
function eased(key, v, now) {
    let t = tweens.get(key);
    if (!t || typeof now !== "number") { t = { from: v, to: v, t0: -1e9 }; tweens.set(key, t); return v; }
    if (t.to !== v) { t.from = shown(t, now); t.to = v; t.t0 = now; }
    return shown(t, now);
}
function shown(t, now) {
    const k = clamp01((now - t.t0) / 140);
    const e = 1 - (1 - k) * (1 - k) * (1 - k);
    return t.from + (t.to - t.from) * e;
}
/* A time control moves only while it is AWAKE: its knob touched (when the
 * host says so) or turned within AWAKE_MS. Asleep, its phase holds still. */
const lastTurn = new Map();
let awake = true;
function noteValue(key, v, now) {
    const l = lastTurn.get(key);
    if (!l) { lastTurn.set(key, { v, t: -1e9 }); return; }
    if (l.v !== v) { l.v = v; l.t = now; }
}
const phases = new Map();
function phase(key, hz, now) {
    let p = phases.get(key);
    if (!p || typeof now !== "number") { p = { ph: 0, t: now || 0 }; phases.set(key, p); }
    const dt = awake ? Math.min(0.25, Math.max(0, (now - p.t) / 1000)) : 0;
    p.ph = (p.ph + hz * dt) % 1; p.t = now;
    return p.ph;
}
/* A small fixed hash, so noise is the same picture every frame. */
function hash(i) { let x = (i * 2654435761) >>> 0; x ^= x >>> 15; x = Math.imul(x, 2246822519) >>> 0; x ^= x >>> 13; return (x >>> 0) / 4294967296; }
function hashText(s) { let h = 7; for (const ch of String(s)) h = (Math.imul(h, 31) + ch.charCodeAt(0)) >>> 0; return h; }

/* ------------------------------------------------------------- the pen -- */
function px(c, x, y) { c.fillRect(Math.round(x), Math.round(y), 1, 1, 1); }
function hline(c, x0, x1, y) { c.fillRect(Math.min(x0, x1), y, Math.abs(x1 - x0) + 1, 1, 1); }
function vline(c, x, y0, y1) { c.fillRect(x, Math.min(y0, y1), 1, Math.abs(y1 - y0) + 1, 1); }
function dots(c, x0, x1, y, every) { for (let x = x0; x <= x1; x += every) px(c, x, y); }
function curve(c, x0, x1, f) {            /* y = f(x), joined so steep runs stay solid */
    let py = null;
    for (let x = x0; x <= x1; x++) {
        const y = Math.round(f(x));
        if (py !== null && Math.abs(y - py) > 1) c.line(x - 1, py, x, y, 1); else px(c, x, y);
        py = y;
    }
}
function dotted(c, x0, x1, f, every) { for (let x = x0; x <= x1; x += every || 2) px(c, x, f(x)); }
function box(c, x0, y0, x1, y1) { hline(c, x0, x1, y0); hline(c, x0, x1, y1); vline(c, x0, y0, y1); vline(c, x1, y0, y1); }
function spark(c, x, y) { px(c, x, y); px(c, x - 1, y - 1); px(c, x + 1, y - 1); px(c, x - 1, y + 1); px(c, x + 1, y + 1); }
function arrowR(c, x, y) { px(c, x - 1, y - 1); px(c, x - 1, y + 1); px(c, x - 2, y - 2); px(c, x - 2, y + 2); }
function arrowL(c, x, y) { px(c, x + 1, y - 1); px(c, x + 1, y + 1); px(c, x + 2, y - 2); px(c, x + 2, y + 2); }

/* ------------------------------------------------------------- motifs -- */
/* An amplitude band: the outline of a sound's loudness along time, hatched
 * inside (or speckled, for noise). */
function band(c, x0, x1, mid, env, fill) {
    for (let x = x0; x <= x1; x++) {
        const a = Math.max(0, Math.round(env(x)));
        px(c, x, mid - a); px(c, x, mid + a);
        if (fill === "speckle") { for (let y = mid - a + 1; y < mid + a; y++) if (hash(x * 31 + y) < 0.3) px(c, x, y); }
        else if (x % 4 === 0) for (let y = mid - a + 2; y <= mid + a - 2; y += 2) px(c, x, y);
    }
}
function waveTone(c, x0, x1, mid, amp, cyc, shape, ph) {
    curve(c, x0, x1, (x) => {
        const u = ((x - x0) / (x1 - x0)) * cyc + (ph || 0);
        const s = Math.sin(u * TAU);
        return mid - amp * shape(s, u);
    });
}
const sine = (s) => s;
/* A struck thing ringing: a sine dying away over L pixels. */
function ring(c, x0, x1, mid, amp, L, per) {
    curve(c, x0, x1, (x) => mid - amp * Math.exp(-(x - x0) / L) * Math.sin((x - x0) * TAU / (per || 5)));
}
/* A strike and its exponential tail, with a spark riding it at the speed it
 * really falls while the knob is awake. */
function fall(c, s, L, sec) {
    const x0 = 4, f = (x) => 10 - 9 * Math.exp(-3 * (x - x0) / L);
    vline(c, x0, 1, 10); curve(c, x0, 28, f);
    const ph = phase(s.key, 1 / (sec + 0.6), s.now), u = ph * (sec + 0.6) / sec;
    if (awake && u < 1) { const x = Math.round(x0 + u * 24); spark(c, x, Math.round(f(x))); }
}
/* A filter's response over the spectrum, hatched under: y(x) on 1..11. */
function response(c, f) {
    curve(c, 3, 28, f);
    for (let x = 4; x <= 28; x += 4) for (let y = Math.round(f(x)) + 3; y <= 11; y += 3) px(c, x, y);
}
const lowpass = (fc) => (x) => 3 + (x > fc ? Math.min(8, (x - fc) * 0.9) : 0);
const highpass = (fc) => (x) => 3 + (x < fc ? Math.min(8, (fc - x) * 0.9) : 0);
/* Partials as stems, heights h(i). */
function ladder(c, xs, hs, base) {
    xs.forEach((x, i) => { const h = Math.round(hs[i]); if (h > 0) vline(c, Math.round(x), base - h + 1, base); });
}
/* A recorded sound seen as its waveform, the same shape for the same seed. */
function blob(i, seed) {
    const u = i / 24;
    const env = u < 0.08 ? u / 0.08 : Math.exp(-(u - 0.08) * (1.5 + 2 * hash(seed)));
    return 1 + 4 * env * (0.55 + 0.45 * hash(i * 7 + seed));
}
function sampleShape(c, x0, x1, seed, solid) {   /* outlined and hatched where it plays, dotted where not */
    for (let x = x0; x <= x1; x++) {
        const a = Math.round(blob(x - 4, seed));
        if (solid(x)) { px(c, x, 6 - a); px(c, x, 6 + a); if (x % 3 === 0) vline(c, x, 6 - a, 6 + a); }
        else if (x % 2 === 0) { px(c, x, 6 - a); px(c, x, 6 + a); }
    }
}

/* ------------------------------------------------------------- the lists -- */
/* params.c's and dice.c's option lists. tests/widgets.test.mjs checks they
 * match what Strut serves. */
const SOUNDS = ["Own", "Kick 1", "Kick 2", "Kick 3", "Kick 4", "Kick 5", "Kick 6", "Kick 7", "Snare 1", "Snare 2",
    "Snare 3", "Snare 4", "Clap 1", "Clap 2", "Clap 3", "Rim 1", "Rim 2", "Hat 1", "Hat 2", "Hat 3", "Hat 4", "Open 1",
    "Open 2", "Cymbal 1", "Cymbal 2", "Tom 1", "Tom 2", "Tom 3", "Tom 4", "Perc 1", "Perc 2", "Perc 3", "Perc 4",
    "Bell 1", "Bell 2", "Bell 3", "Bass 1", "Bass 2", "FX 1", "FX 2", "FX 3"];
const KITS = ["Own", "Strut", "Dry", "Hall", "Dust", "Hard", "Tape", "Tin", "Club", "Soft", "Cave", "Grit", "Glass"];
const WAVE_TABLES = ["Analog", "Sync", "Fold", "Sweep", "Vowel", "Hollow", "Metal", "Glass"];
const NOISE_TABLES = ["White", "Pink", "Brown", "Hiss", "Wires", "Metal", "Crackle", "Grit"];
const LISTS = {
    sound: SOUNDS, kit: KITS, w_table: WAVE_TABLES, n_table: NOISE_TABLES,
    s_hit: ["Click", "Soft", "Burst", "Wave", "Noise"], s_mode: ["Low", "Band", "High"],
    n_mode: ["Sample", "Resynth", "Noise"],
    kind: ["Envelope", "LFO", "Random", "Velocity"],
    curve: ["Natural", "Ping", "Soft", "Hold", "Swell", "Clap"], s_curve: ["Natural", "Ping", "Soft", "Hold"],
    s_aim: ["Pitch", "Ring", "Snap", "Metal", "Tone", "Level"],
    w_aim: ["Pitch", "Wave", "FM", "Ring", "Level"],
    n_aim: ["Pitch", "Color", "Start", "Loop", "Level"],
    view: ["Sound", "Mod"], dice: ["Back", "Roll"], choke: ["Off", "A", "B", "C", "D"],
};
/* The pictures that slide in from the side you turned towards, as Quilt's
 * TYPE does: each option is a different thing, not more of one. */
const SLIDES = { sound: 1, kit: 1, w_table: 1, n_table: 1 };

/* ------------------------------------------------------------ drawers -- */
/* Each takes (c, v, s): v is the eased value in 0..1 (an enum's index over
 * its last), and s carries the option name, the raw value, the page's other
 * values and the time. Each draws its picture only; the pen scales it. */
const D = {};

/* PAD ------------------------------------------------------------------- */
/* SOUND draws the drum, sliding in. Each is a 22x11 sketch from x. */
const ICON = {
    own(c, x) { box(c, x + 6, 1, x + 16, 10); c.drawCircle(x + 11, 6, 2, 1); },     /* the pad as it is */
    kick(c, x) { c.drawCircle(x + 11, 5, 5, 1); c.drawCircle(x + 11, 5, 1, 1); c.line(x + 7, 9, x + 5, 11, 1); c.line(x + 15, 9, x + 17, 11, 1); },
    snare(c, x) { box(c, x + 3, 4, x + 19, 8); for (let i = x + 6; i < x + 19; i += 4) px(c, i, 6); dots(c, x + 4, x + 18, 10, 2);
        c.line(x + 6, 0, x + 10, 3, 1); c.line(x + 16, 0, x + 12, 3, 1); },
    clap(c, x) { [9, 7, 5].forEach((h, i) => vline(c, x + 4 + i * 3, 10 - h, 10)); dotted(c, x + 13, x + 20, (xx) => 10 - 5 * Math.exp(-(xx - x - 12) / 3), 2); hline(c, x + 2, x + 20, 11); },
    rim(c, x) { box(c, x + 5, 5, x + 17, 10); c.line(x + 1, 2, x + 21, 6, 1); },
    hat(c, x) { c.line(x + 3, 5, x + 11, 3, 1); c.line(x + 11, 3, x + 19, 5, 1); c.line(x + 3, 6, x + 11, 8, 1); c.line(x + 11, 8, x + 19, 6, 1); vline(c, x + 11, 0, 2); vline(c, x + 11, 9, 11); },
    open(c, x) { c.line(x + 3, 3, x + 11, 1, 1); c.line(x + 11, 1, x + 19, 3, 1); c.line(x + 3, 7, x + 11, 9, 1); c.line(x + 11, 9, x + 19, 7, 1); vline(c, x + 11, 10, 11);
        dots(c, x + 6, x + 16, 5, 2); },
    cymbal(c, x) { c.line(x + 1, 4, x + 11, 1, 1); c.line(x + 11, 1, x + 21, 4, 1); hline(c, x + 1, x + 21, 4); vline(c, x + 11, 5, 10); c.line(x + 11, 10, x + 8, 11, 1); c.line(x + 11, 10, x + 14, 11, 1); },
    tom(c, x) { box(c, x + 6, 3, x + 16, 10); hline(c, x + 6, x + 16, 4); c.line(x + 18, 0, x + 13, 3, 1); },
    perc(c, x) { c.line(x + 8, 2, x + 6, 10, 1); c.line(x + 14, 2, x + 16, 10, 1); hline(c, x + 8, x + 14, 2); hline(c, x + 6, x + 16, 10); hline(c, x + 10, x + 12, 0); vline(c, x + 10, 0, 2); vline(c, x + 12, 0, 2); },
    bell(c, x) { c.drawArc(x + 11, 5, 4, 270, 180, 1); vline(c, x + 7, 5, 8); vline(c, x + 15, 5, 8); c.line(x + 7, 8, x + 5, 10, 1); c.line(x + 15, 8, x + 17, 10, 1); hline(c, x + 5, x + 17, 10); vline(c, x + 11, 0, 1); },
    bass(c, x) { curve(c, x + 1, x + 21, (xx) => 6 - 5 * Math.exp(-(xx - x - 1) / 12) * Math.sin((xx - x - 1) / 20 * TAU * 1.5)); },
    fx(c, x) { c.line(x + 14, 0, x + 8, 6, 1); hline(c, x + 8, x + 14, 6); c.line(x + 14, 6, x + 8, 11, 1); },
};
function soundIcon(name) { const r = String(name).toLowerCase().split(" ")[0]; return ICON[r] || ICON.own; }
/* a sliding picture: the old one leaving, the new one arriving */
function slide(c, s, drawAt) {
    const tw = s.tween;
    if (tw && tw.k < 1 && tw.from != null) {
        const d = Math.round((1 - tw.k) * 26) * tw.dir;
        drawAt(offset(c, d - 26 * tw.dir), tw.from); drawAt(offset(c, d), s.opt);
    } else drawAt(c, s.opt);
}
/* Own is the pad as it is; the rest are numbered within their kind. */
D.sound = (c, v, s) => slide(c, s, (cc, name) => {
    const n = String(name).split(" ")[1];
    soundIcon(name)(cc, n ? 1 : 5);
    if (n) cc.text(n, 4, 24);
});

D.tune = (c, v) => {                             /* more cycles, higher */
    const st = v * 48 - 24;
    waveTone(c, 3, 28, 6, 4, 2 * Math.pow(2, st / 16), sine);
};
D.decay = (c, v, s) => {                         /* every engine's fall, a quarter to four times */
    const k = Math.pow(4, v * 2 - 1);
    fall(c, s, 7 * k, 0.25 * k);
};
const tilt = (c, v) => {                         /* darker left, thinner right, flat in the middle */
    const b = v * 2 - 1;
    response(c, b < 0 ? lowpass(28 + b * 22) : highpass(3 + b * 22));
};
D.color = tilt;
/* The three engines' levels: each engine's own sound, as large as its fader. */
D.skin = (c, v) => { const a = v * 5; if (a < 0.5) dotted(c, 3, 28, () => 6, 2); else { vline(c, 3, 6 - Math.round(a), 6); ring(c, 3, 28, 6, a, 9, 6); } };
D.wave = (c, v) => { const a = v * 5; if (a < 0.5) dotted(c, 3, 28, () => 6, 2); else waveTone(c, 3, 28, 6, a, 3, (x, u) => 1 - 2 * (u % 1)); };
D.noise = (c, v) => {
    const a = Math.round(v * 5);
    if (a < 1) { dotted(c, 3, 28, () => 6, 2); return; }
    for (let x = 3; x <= 28; x++) for (let y = 6 - a; y <= 6 + a; y++) if (hash(x * 13 + y * 7) < 0.3) px(c, x, y);
    px(c, 3, 6); px(c, 28, 6);
};
D.space = (c, v, s) => {                         /* a sound in a room: the more space, the more echoes roll outward */
    vline(c, 3, 5, 7); for (let y = 0; y <= 11; y += 3) px(c, 28, y);
    const n = Math.ceil(v * 6 - 0.05), ph = phase(s.key, 0.6, s.now), o = offset(c, 0);
    for (let i = 0; i < n; i++) {
        const r = Math.round(3 + (i + ph) * 4);
        if (r > 26) continue;
        if (i < 3) o.drawArc(4, 6, r, 35, 110); else arcDots(o, 4, 6, r);
    }
};
function arcDots(o, x, y, r) { for (let a = 35; a <= 145; a += 90 / r) { const t = a * Math.PI / 180; if (Math.round(a * r / 60) % 2) o.fillRect(Math.round(x + Math.sin(t) * r), Math.round(y - Math.cos(t) * r), 1, 1, 1); } }

/* MOD: which view the engine page shows. The one on is drawn, the other dotted. */
D.view = (c, v) => {
    const sound = (x) => 6 - 4 * Math.exp(-(x - 4) / 4) * Math.sin((x - 4) * 1.3);
    const mod = (x) => 6 - 3 * Math.sin((x - 18) / 10 * TAU);
    if (v < 0.5) { vline(c, 4, 2, 6); curve(c, 4, 13, sound); dotted(c, 18, 28, mod, 2); }
    else { dotted(c, 4, 13, sound, 2); curve(c, 18, 28, mod); }
};

/* SKIN ------------------------------------------------------------------ */
D.s_pitch = (c, v) => {                          /* a smaller drum rings higher */
    const w = Math.round(22 - v * 16), h = Math.round(8 - v * 4), l = 16 - (w >> 1), r = l + w;
    box(c, l, 10 - h, r, 10); hline(c, l, r, 11 - h);
    for (let x = l + 2; x < r - 1; x += 3) px(c, x, 11 - (h >> 1));
};
D.s_ring = (c, v, s) => {                        /* how long the body rings */
    const L = 2 + v * 26, f = (x) => 6 - 5 * Math.exp(-(x - 4) / L);
    vline(c, 4, 1, 6); ring(c, 4, 28, 6, 5, L, 6);
    const sec = 0.1 + v * 2.5, ph = phase(s.key, 1 / (sec + 0.6), s.now), u = ph * (sec + 0.6) / sec;
    if (awake && u < 1) { const x = Math.round(4 + u * 24); spark(c, x, Math.round(f(x)) - 1); }
};
const HIT = {                                    /* what strikes the head */
    Click(c) { c.line(8, 0, 15, 8, 1); px(c, 17, 9); px(c, 13, 9); },
    Soft(c) { c.line(19, 6, 27, 0, 1); c.drawCircle(16, 6, 3, 1); },
    Burst(c) { for (const x of [12, 16, 20]) c.line(14, 0, x, 8, 1); },   /* a brush */
    Wave(c) { waveTone(c, 8, 24, 4, 3, 2, sine); },
    Noise(c) { for (let y = 1; y <= 7; y++) for (let x = 9; x <= 23; x++) if (hash(x * 17 + y * 5) < 0.28) px(c, x, y); },
};
D.s_hit = (c, v, s) => {
    hline(c, 4, 27, 10); vline(c, 4, 9, 11); vline(c, 27, 9, 11);
    (HIT[s.opt] || HIT.Click)(c);
};
D.s_snap = (c, v) => {                           /* the strike's length: a sharp click to a broad thud */
    const w = 0.6 + v * 5, h = 9 - v * 4, mid = 6 + w;
    curve(c, 3, 28, (x) => 10 - h * Math.exp(-((x - mid) * (x - mid)) / (2 * w * w)));
};
D.s_metal = (c, v) => {                          /* the two partials climb from a drumhead's to a metal bar's */
    const at = (r) => 5 + (r - 1) * 5;
    const r1 = 1.59 + v * (2.76 - 1.59), r2 = 2.14 + v * (5.40 - 2.14);
    ladder(c, [5, at(r1), at(r2)], [9, 5 + v * 3, 4 + v * 4], 11);
    dots(c, 3, 28, 11, 3);
};
D.s_tone = (c, v) => response(c, lowpass(6 + v * 22));
D.s_mode = (c, v, s) => {                        /* the filter around the pitch */
    const m = s.opt;
    response(c, m === "High" ? highpass(12) : m === "Band" ? (x) => 3 + Math.min(8, Math.abs(x - 16) * 0.7) : lowpass(20));
    vline(c, 16, 0, 1);
};

/* WAVE ------------------------------------------------------------------ */
D.w_pitch = (c, v) => waveTone(c, 3, 28, 6, 4, 1 + v * 5, sine);
D.w_bend = (c, v) => {                           /* where the pitch starts, and how it comes home */
    const b = v * 2 - 1, a = 5 * Math.sign(b) * Math.sqrt(Math.abs(b));
    dots(c, 4, 28, 6, 3);
    curve(c, 4, 28, (x) => 6 - a * Math.exp(-(x - 4) / 6));
};
D.w_decay = (c, v, s) => fall(c, s, 1 + v * 20, 0.05 + v * 2);
/* Wave's tables, after tables.c's recipes: u runs 0..1 along the table, x
 * 0..1 along one cycle. Peaks are scaled to the picture, not to loudness. */
const VOWELS = [[300, 870, 2240], [570, 840, 2410], [730, 1090, 2440], [530, 1840, 2480], [270, 2290, 3010]];
function harmonics(t, u) {
    const a = new Array(33).fill(0);
    for (let h = 1; h <= 32; h++) {
        if (t === "Sweep") { const x = h / Math.pow(2, 5 * u); a[h] = 1 / h / Math.sqrt((1 - x * x) * (1 - x * x) + x * x / 25); }
        else if (t === "Vowel") {
            const vv = u * 4, i = vv >= 4 ? 3 : Math.floor(vv), w = vv - i, f = 110 * h; a[h] = 1 / h;
            for (let k = 0; k < 3; k++) { const fc = VOWELS[i][k] * Math.pow(VOWELS[i + 1][k] / VOWELS[i][k], w), bw = [80, 100, 140][k];
                a[h] *= fc * fc / Math.sqrt((fc * fc - f * f) * (fc * fc - f * f) + bw * bw * f * f); }
        } else if (t === "Hollow") { if (h & 1) a[h] = Math.pow(h, -(2 - 1.4 * u)); }
        else if (t === "Metal") { const cc = Math.pow(2, 1.5 + 4.5 * u), x = Math.log2(h / cc);
            if (h === 1) a[h] = 0.3; else if (isPrime(h)) a[h] = Math.exp(-x * x / 0.72) / Math.sqrt(h); }
        else if (t === "Glass") { const n = Math.round(Math.sqrt(h)); if (n * n === h) a[h] = Math.pow(n, -(2.5 - 2 * u)); }
    }
    return a;
}
function isPrime(n) { if (n < 2) return false; for (let d = 2; d * d <= n; d++) if (n % d === 0) return false; return true; }
const saw = (x) => 2 * (x - Math.floor(x)) - 1;
function tableShape(t, u) {                     /* a function of x, peak 1 */
    let f;
    if (t === "Sync") { const r = Math.pow(2, 3 * u); f = (x) => saw(r * x); }
    else if (t === "Fold") { const g = 1 + 7 * u; f = (x) => Math.sin(Math.PI / 2 * g * Math.sin(TAU * x)); }
    else if (t === "Analog" || !WAVE_TABLES.includes(t)) {
        if (u < 0.25) f = (x) => (1 - u * 4) * Math.sin(TAU * x) + u * 4 * (1 - 4 * Math.abs(((x + 0.25) % 1) - 0.5));
        else if (u < 0.5) { const k = (u - 0.25) * 4; f = (x) => (1 - k) * (1 - 4 * Math.abs(((x + 0.25) % 1) - 0.5)) + k * saw(x + 0.5); }
        else { const k = u < 0.75 ? (u - 0.5) * 4 : 1, pw = u < 0.75 ? 0.5 : 0.5 - 0.45 * (u - 0.75) * 4;
            f = (x) => saw(x + 0.5) - k * saw(x + 0.5 + pw); }
    } else {
        const a = harmonics(t, u);
        f = (x) => { let y = 0; for (let h = 1; h <= 32; h++) if (a[h]) y += a[h] * Math.sin(TAU * (h * x + hash(h + 3))); return y; };
    }
    let peak = 1e-9; for (let i = 0; i < 64; i++) peak = Math.max(peak, Math.abs(f(i / 64)));
    return (x) => f(x) / peak;
}
/* Each shape is worked out once (to a hundredth of the table) and kept: a
 * recipe is 32 harmonics, too dear to sum again every frame of a turn. */
const shapes = new Map();
function tableWave(c, t, u, x0, x1, cyc) {
    const id = t + "|" + Math.round(u * 100) + "|" + x0 + "|" + x1 + "|" + cyc;
    let ys = shapes.get(id);
    if (!ys) {
        const f = tableShape(t, Math.round(u * 100) / 100), n = x1 - x0 + 1;
        ys = [];
        for (let x = x0; x <= x1; x++) {        /* each pixel's furthest point, so a narrow pulse still shows */
            let y = 0;
            for (let k = 0; k < 8; k++) { const q = f(((x - x0 + k / 8) / n) * cyc); if (Math.abs(q) > Math.abs(y)) y = q; }
            ys.push(6 - 4.5 * y);
        }
        if (shapes.size > 200) shapes.clear();
        shapes.set(id, ys);
    }
    curve(c, x0, x1, (x) => ys[x - x0]);
}
D.w_table = (c, v, s) => slide(c, s, (cc, name) => tableWave(cc, name, 0.5, 4, 27, 2));
D.w_wave = (c, v, s) => tableWave(c, s.values.w_table || "Analog", v, 4, 27, 2);
D.w_fm = (c, v) => {                             /* Skin bends Wave: the cycles bunch and stretch */
    const k = v * v * 4;
    curve(c, 3, 28, (x) => { const u = (x - 3) / 25; return 6 - 4.5 * Math.sin(TAU * 2 * u + k * Math.sin(TAU * 2 * u)); });
};
D.w_ring = (c, v) => {                           /* Wave times a sine: slow swells left, fast beating right */
    const r = v * 2 - 1, mix = Math.min(1, Math.abs(r) * 10), m = 1.5 * Math.pow(2, 2 * r);
    curve(c, 3, 28, (x) => { const u = (x - 3) / 25, sw = Math.sin(TAU * 6 * u);
        return 6 - 4.5 * sw * (1 - mix + mix * Math.cos(TAU * m * u)); });
};

/* NOISE ----------------------------------------------------------------- */
D.n_pitch = (c, v) => {                          /* faster is shorter, and higher */
    const st = v * 96 - 48, L = Math.max(4, Math.min(24, 12 * Math.pow(2, -st / 24)));
    dots(c, 4, 28, 6, 3);
    band(c, 4, Math.round(4 + L), 6, (x) => 5 * Math.exp(-2.5 * (x - 4) / L), "speckle");
};
D.n_mode = (c, v, s) => {                        /* your sample played, rebuilt from tones, or turned to noise */
    if (s.opt === "Resynth") { ladder(c, [5, 8, 11, 14, 17, 20, 23, 26], [8, 5, 7, 4, 5, 3, 3, 2], 11); dots(c, 3, 28, 11, 3); }
    else if (s.opt === "Noise") { for (let x = 3; x <= 28; x++) for (let y = 1; y <= 11; y++) if (hash(x * 19 + y * 3) < 0.22 * blob(x - 4, 3) / 3) px(c, x, y); }
    else sampleShape(c, 4, 28, 3, () => true);
};
D.n_decay = (c, v, s) => {
    const L = 2 + v * 24, env = (x) => 5 * Math.exp(-2.5 * (x - 4) / L);
    band(c, 4, 28, 6, env, "speckle");
    const sec = 0.05 + v * 2, ph = phase(s.key, 1 / (sec + 0.6), s.now), u = ph * (sec + 0.6) / sec;
    if (awake && u < 1) { const x = Math.round(4 + u * 24); spark(c, x, 6 - Math.round(env(x)) - 1); }
};
/* Noise's tables as a stretch of each signal. */
const NOISE = {
    White: (x) => 6 + (hash(x) - 0.5) * 10,
    Pink: (x) => 6 + ((hash(x) - 0.5) * 4 + (hash(x >> 1) - 0.5) * 3 + (hash((x >> 2) + 50) - 0.5) * 4),
    Brown: (x) => { let y = 0; for (let i = 3; i <= x; i++) y += (hash(i + 90) - 0.5) * 2.2; return 6 + Math.max(-5, Math.min(5, y)); },
    Hiss: (x) => 6 + (x % 2 ? 1 : -1) * (0.5 + hash(x + 7) * 2),
    Wires: (x) => 6 + ((x > 7 && x < 13) || (x > 18 && x < 23) ? (x % 2 ? 1 : -1) * 3 * hash(x) : 0),
    Metal: (x) => 6 + [5, 7, 11].reduce((a, p) => a + (Math.sin(x * TAU / p) > 0 ? 1.4 : -1.4), 0),
    Crackle: (x) => 6 - ([6, 11, 19, 24].includes(x) ? 5 * (0.5 + hash(x)) : 0),
    Grit: (x) => 6 + (hash(Math.floor(x / 3) + 40) - 0.5) * 9,
};
D.n_table = (c, v, s) => slide(c, s, (cc, name) => {
    const f = NOISE[name];
    if (f) curve(cc, 3, 28, (x) => Math.max(1, Math.min(11, f(x))));
    else sampleShape(cc, 4, 28, hashText(name), () => true);  /* one of your samples, its own shape */
});
D.n_color = tilt;
D.n_start = (c, v) => {                          /* where in the sample it starts */
    const at = 4 + Math.round(v * 22);
    sampleShape(c, 4, 28, 3, (x) => x >= at);
    vline(c, at, 0, 11); px(c, at + 1, 0); px(c, at + 1, 1); px(c, at + 2, 0);
};
D.n_loop = (c, v) => {                           /* how much of it loops; all of it is no loop */
    if (v > 0.99) { sampleShape(c, 4, 28, 3, () => true); return; }
    const end = 5 + Math.round(v * 21);
    sampleShape(c, 4, 28, 3, (x) => x <= end);
    vline(c, end, 0, 11); dots(c, 5, end - 1, 0, 2); arrowL(c, 4, 0);
};

/* MOD VIEWS (all three engines) --------------------------------------- */
function modShape(kind, u) {                     /* one cycle of the modulator, -1..1 */
    const p = u - Math.floor(u), n = Math.floor(u);
    if (kind === "LFO") return Math.sin(p * TAU);
    if (kind === "Random") return hash(n + 11) * 2 - 1;
    if (kind === "Velocity") return 0.6;
    return Math.exp(-5 * p) * 2 - 1;             /* Envelope: falls from the top each hit */
}
D.kind = (c, v, s) => {
    const k = s.opt;
    if (k === "Velocity") {                      /* harder and harder strikes */
        for (let i = 0; i < 5; i++) { const x = 6 + i * 5, h = 2 + i * 2; vline(c, x, 11 - h, 11); hline(c, x - 1, x + 1, 11 - h); }
    } else if (k === "LFO") { waveTone(c, 4, 27, 6, 4, 2, sine); vline(c, 4, 9, 11); }
    else if (k === "Random") {                   /* a new value each hit, held */
        let py = null;
        for (let i = 0; i < 4; i++) { const y = Math.round(2 + hash(i + 11) * 8), x0 = 4 + i * 6;
            hline(c, x0, x0 + 5, y); if (py !== null) vline(c, x0, py, y); vline(c, x0, 10, 11); py = y; }
    } else {                                     /* Envelope: falls from the top on each hit */
        for (const x0 of [4, 16]) { vline(c, x0, 1, 10); curve(c, x0, x0 + 11, (x) => 10 - 9 * Math.exp(-(x - x0) / 2.5)); }
    }
};
/* RATE: the modulator at its speed, and the speed in plain figures. Fastest
 * at the centre; free time left, note values at the tempo right. */
const NOTES = ["1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1BAR", "2BAR", "4BAR"];
function rateText(r) {
    if (r > 0) return NOTES[Math.min(11, Math.round(r * 11))];
    const t = 0.005 * Math.pow(800, -r);
    return t < 1 ? Math.round(t * 1000) + "MS" : t.toFixed(1) + "S";
}
D.rate = (c, v, s) => {
    const r = v * 2 - 1, slow = Math.abs(r), cyc = 1 + (1 - slow) * 3;
    const kind = s.values[s.param[0] + "_kind"] || "Envelope";
    const ph = phase(s.key, 0.3 + (1 - slow) * 2.5, s.now);
    curve(c, 4, 27, (x) => 1.5 - 1.5 * modShape(kind, ((x - 4) / 23) * cyc - ph * cyc));
    c.text(rateText(r), 9);
};
D.curve = (c, v, s) => {                         /* the shape of the engine's own fall */
    const k = s.opt;
    hline(c, 4, 28, 11);
    if (k === "Ping") { vline(c, 4, 1, 10); curve(c, 4, 28, (x) => 10 - 9 * Math.exp(-(x - 4) / 2.5)); }
    else if (k === "Soft") { c.line(4, 10, 8, 1, 1); curve(c, 8, 28, (x) => 10 - 9 * Math.exp(-(x - 8) / 5)); }
    else if (k === "Hold") { vline(c, 4, 1, 10); hline(c, 4, 15, 1); curve(c, 15, 28, (x) => 10 - 9 * Math.exp(-(x - 15) / 1.5)); }
    else if (k === "Swell") { curve(c, 4, 25, (x) => 10 - 9 * Math.exp(-(25 - x) / 5)); vline(c, 25, 1, 10); }
    else if (k === "Clap") {                     /* three slaps, then the tail */
        for (const a of [4, 8, 12]) { vline(c, a, 1, 10); curve(c, a, a + 4, (x) => 10 - 9 * Math.exp(-(x - a) / 0.8)); }
        vline(c, 16, 1, 10); curve(c, 16, 28, (x) => 10 - 9 * Math.exp(-(x - 16) / 4));
    }
    else { vline(c, 4, 1, 10); curve(c, 4, 28, (x) => 10 - 9 * Math.exp(-(x - 4) / 5)); }
};
/* AIM: an arrow into a small picture of the knob it moves. */
const TARGET = {
    Pitch(c) { vline(c, 20, 1, 11); arrowUp(c, 20, 1); arrowDown(c, 20, 11); },
    Ring(c, e) { if (e === "w") { curve(c, 13, 28, (x) => 6 - 4 * Math.sin((x - 13) * 1.3) * Math.cos((x - 13) * 0.2)); }
        else { vline(c, 13, 2, 6); ring(c, 13, 28, 6, 4, 5, 4); } },
    Snap(c) { hline(c, 13, 28, 10); curve(c, 13, 28, (x) => 10 - 8 * Math.exp(-((x - 17) * (x - 17)) / 3)); },
    Metal(c) { ladder(c, [15, 20, 26], [9, 6, 5], 11); },
    Tone(c) { curve(c, 13, 28, lowpass(20)); },
    Level(c) { for (let x = 13; x <= 28; x++) { const y = Math.round(10 - (x - 13) * 0.55); px(c, x, y); px(c, x, 11); if (x % 3 === 0) vline(c, x, y, 11); } },
    Wave(c) { tableWave(c, "Analog", 0.6, 13, 28, 1.5); },
    FM(c) { curve(c, 13, 28, (x) => { const u = (x - 13) / 15; return 6 - 4 * Math.sin(TAU * 1.5 * u + 2 * Math.sin(TAU * 1.5 * u)); }); },
    Color(c) { curve(c, 13, 28, (x) => 3 + (x - 13) * 0.45); },
    Start(c) { for (let x = 18; x <= 28; x++) { const a = Math.round(blob((x - 13) * 1.6, 3)); px(c, x, 6 - a); px(c, x, 6 + a); } vline(c, 18, 0, 11); },
    Loop(c) { c.drawArc(20, 6, 4, 0, 300, 1); px(c, 21, 1); px(c, 22, 2); px(c, 22, 0); },
};
function arrowUp(c, x, y) { px(c, x - 1, y + 1); px(c, x + 1, y + 1); px(c, x - 2, y + 2); px(c, x + 2, y + 2); }
function arrowDown(c, x, y) { px(c, x - 1, y - 1); px(c, x + 1, y - 1); px(c, x - 2, y - 2); px(c, x + 2, y - 2); }
D.aim = (c, v, s) => {
    hline(c, 3, 9, 6); arrowR(c, 9, 6);
    (TARGET[s.opt] || TARGET.Level)(c, s.param[0]);
};
D.depth = (c, v) => {                            /* how far, and which way, from the middle */
    const d = Math.round((v * 2 - 1) * 12);
    vline(c, 16, 2, 10);
    if (d) { const x1 = 16 + d, sg = Math.sign(d); hline(c, 16, x1, 5); hline(c, 16, x1, 7); vline(c, x1, 4, 8); px(c, x1 + sg, 6); }
    dots(c, 4, 28, 11, 3);
};

/* FINISH ---------------------------------------------------------------- */
function wedge(c, n) {                           /* a fader: a wedge filled to the level */
    for (let x = 3; x <= 28; x++) { const h = Math.round(1 + (x - 3) * 0.4); const y0 = 11 - h;
        if ((x - 3) / 25 <= n && n > 0.01) { px(c, x, y0); px(c, x, 11); if (x % 3 === 0) vline(c, x, y0, 11); } else if (x % 2 === 0) px(c, x, y0); }
}
D.level = (c, v) => wedge(c, v);
D.pan = (c, v) => {                              /* where the pad sits, left to right */
    const x = 16 + Math.round((v * 2 - 1) * 11);
    hline(c, 4, 28, 10); vline(c, 4, 8, 11); vline(c, 28, 8, 11); px(c, 16, 11);
    c.drawCircle(x, 5, 2, 1); vline(c, x, 8, 9);
};
D.flam = (c, v) => {                             /* one hit becomes three, this far apart */
    const d = v * 9;
    hline(c, 3, 28, 11);
    [9, 7, 5].forEach((h, i) => vline(c, Math.round(6 + i * d), 11 - h, 10));
};
D.drive = (c, v) => {                            /* the wave's shoulders flatten */
    const k = 1 + v * 6;
    waveTone(c, 3, 28, 6, 4.5, 2, (x) => Math.tanh(x * k) / Math.tanh(k));
};
D.crush = (c, v) => {                            /* fewer steps in time and in level */
    const st = 1 + Math.round(v * 5), q = 0.3 + v * 2.2;
    curve(c, 3, 28, (x) => { const xs = 3 + Math.floor((x - 3) / st) * st; return 6 - Math.round(4.5 * Math.sin((xs - 3) / 25 * 2 * TAU) / q) * q; });
};
const smooth = (u) => { u = clamp01(u); return u * u * (3 - 2 * u); };
const shelf = (c, v, low) => {                   /* the lows or the highs lifted or cut */
    const g = (v * 2 - 1) * 4.5;
    dots(c, 3, 28, 6, 3);
    curve(c, 3, 28, (x) => 6 - g * (low ? 1 - smooth((x - 8) / 12) : smooth((x - 11) / 12)));
};
D.low = (c, v) => shelf(c, v, true);
D.high = (c, v) => shelf(c, v, false);
function die(c, x, y, pips) {                    /* a die, 9 across */
    box(c, x, y, x + 8, y + 8);
    const at = { 1: [[4, 4]], 3: [[2, 2], [4, 4], [6, 6]], 5: [[2, 2], [6, 2], [4, 4], [2, 6], [6, 6]] }[pips];
    for (const [dx, dy] of at) px(c, x + dx, y + dy);
}
const roll = (n) => (c, v, s) => {               /* right rolls, left steps back */
    if (n === 1) die(c, 12, 1, 5); else { die(c, 9, 3, 3); hline(c, 15, 21, 0); vline(c, 21, 0, 6); hline(c, 18, 20, 0); px(c, 19, 2); px(c, 17, 1); px(c, 21, 1); }
    if (s.opt === "Back") { hline(c, 3, 7, 6); arrowL(c, 3, 6); }
    else { hline(c, 24, 28, 6); arrowR(c, 28, 6); }
};
D.dice = roll(1);

/* KIT ------------------------------------------------------------------- */
D.size = (c, v) => {                             /* the room, in perspective */
    const d = 2 + Math.round(v * 9);
    hline(c, 16 - d - 2, 16 + d + 2, 11); c.line(16 - d - 2, 11, 16 - d, 11 - d, 1); c.line(16 + d + 2, 11, 16 + d, 11 - d, 1);
    hline(c, 16 - d, 16 + d, 11 - d); px(c, 16, 10);
};
D.glue = (c, v) => {                             /* soft hits pulled up nearer loud ones */
    const hs = [9, 3, 7, 2, 8, 4];
    hline(c, 3, 28, 11);
    hs.forEach((h, i) => { const x = 5 + i * 4, g = Math.round(h + (8 - h) * v * 0.75); vline(c, x, 11 - g, 11); hline(c, x - 1, x + 1, 11 - g); });
};
D.warm = (c, v) => {                             /* thicker and rounder, the top softened */
    const k = 1 + v * 3, f = (x) => { const u = (x - 3) / 25; return 6 - 4 * Math.tanh(Math.sin(TAU * 2 * u) * k) / Math.tanh(k); };
    curve(c, 3, 28, f);
    if (v > 0.25) curve(c, 3, 28, (x) => f(x) + 1);
};
D.vol = (c, v) => {                              /* a wedge, filled to the level, with a mark at 0 dB */
    wedge(c, v);
    const z = 3 + Math.round(25 * 60 / 66); vline(c, z, 0, 1);
};
D.choke = (c, v, s) => {                         /* a hit in the group stops the one still ringing */
    if (s.opt === "Off" || !s.opt) { vline(c, 4, 2, 6); ring(c, 4, 28, 6, 4, 12, 4); vline(c, 16, 2, 6); return; }
    vline(c, 4, 2, 6); ring(c, 4, 13, 6, 4, 12, 4); vline(c, 14, 1, 11);
    c.text(s.opt, 5, 20);
};
D.kit = (c, v, s) => slide(c, s, (cc, name) => {
    const i = KITS.indexOf(name), x = i > 0 ? 1 : 5;
    /* a kit from the front: the kick, two toms on it, a cymbal each side */
    cc.drawCircle(x + 11, 7, 4); cc.drawCircle(x + 8, 2, 2); cc.drawCircle(x + 14, 2, 2);
    cc.line(x, 1, x + 4, 0, 1); cc.line(x + 18, 0, x + 22, 1, 1); vline(cc, x + 2, 2, 11); vline(cc, x + 20, 2, 11);
    if (i > 0) cc.text(String(i), 4, 23 + (i < 10 ? 1 : 0));
});
D.kit_dice = roll(2);
D.pad = (c, v) => {                              /* the pad the other pages edit, as the Move lays them out */
    const n = Math.round(v * 15);
    for (let i = 0; i < 16; i++) {           /* in the screen's own pixels, so each pad keeps its place */
        const x = 6 + (i % 4) * 3, y = 11 - Math.floor(i / 4) * 3;
        c.rawRect(x, y, i === n ? 2 : 1, i === n ? 2 : 1);
    }
    c.text(String(n + 1), 5, 20);
};

/* ------------------------------------------------------------ routing -- */
function roleOf(key) {
    if (/_view$/.test(key)) return "view";
    if (key === "kit_choke") return "choke";
    const m = /^([swn])_(kind|rate|curve|aim|depth)\d?$/.exec(key);
    return m ? m[2] : key;
}
function listOf(key) {
    const role = roleOf(key);
    if (role === "aim") return LISTS[key[0] + "_aim"];
    if (role === "kit_dice") return LISTS.dice;
    return LISTS[role] || LISTS[key];
}
/* Every number's range; the rest are 0..1. Bipolar knobs are -1..1. */
const RANGE = { tune: [-24, 24], s_pitch: [-12, 60], w_pitch: [-12, 60], n_pitch: [-48, 48], low: [-18, 18], high: [-18, 18],
    vol: [-60, 6], pad: [1, 16] };
const BIPOLAR = { decay: 1, color: 1, rate: 1, depth: 1, w_bend: 1, w_ring: 1, n_color: 1, pan: 1 };

/* A value as 0..1 and, for an enum, its option name; NaN when unanswered. */
function read(key, raw) {
    const t = raw == null ? "" : String(raw).trim();
    if (t === "") return { v: NaN };
    const list = listOf(key);
    if (list) {
        let i = list.indexOf(t);
        if (i < 0 && Number.isFinite(Number(t))) i = Math.round(Number(t));
        if (key === "n_table" && (i < 0 || i >= list.length)) return { v: 1, opt: t };   /* a sample */
        if (i < 0 || i >= list.length) i = 0;
        return { v: i / Math.max(1, list.length - 1), opt: list[i], i };
    }
    const n = Number(t); if (!Number.isFinite(n)) return { v: NaN };
    if (BIPOLAR[roleOf(key)]) return { v: clamp01((n + 1) / 2) };
    const r = RANGE[key]; return { v: clamp01(r ? (n - r[0]) / (r[1] - r[0]) : n) };
}
/* What else on the page a picture reads, so a still one is redrawn when it changes. */
const DEPENDS = { w_wave: ["w_table"], s_rate: ["s_kind"], w_rate: ["w_kind"], n_rate: ["n_kind"] };
const slides = new Map();

function drawCell(ctx, { values, group, nowMs, touched }) {
    const key = group && group.keys && group.keys[0];
    if (!key || !values) return;
    const role = roleOf(key);
    const fn = D[role]; if (!fn) return false;
    const sid = (group.tile != null ? group.tile + "|" : "") + key;
    const raw = values[key];
    const r = read(key, raw);
    if (!Number.isFinite(r.v)) return;   /* no answer yet: no picture of a made-up value */
    const now = typeof nowMs === "number" ? nowMs : 0;
    const v = r.opt !== undefined || key === "pad" ? r.v : eased(sid, r.v, now);
    noteValue(sid, r.v, now);
    awake = !!(touched || (group && group.touched)) || now - lastTurn.get(sid).t < AWAKE_MS;
    const s = { key: sid, param: key, raw, opt: r.opt, values, now };
    if (SLIDES[role]) {
        let tw = slides.get(sid); if (!tw) { tw = { name: null, i: 0, from: null, t0: -1e9, dir: 1 }; slides.set(sid, tw); }
        if (tw.name !== r.opt) { if (tw.name !== null) { tw.from = tw.name; tw.t0 = now; tw.dir = (r.i ?? 0) >= tw.i ? 1 : -1; } tw.name = r.opt; tw.i = r.i ?? 0; }
        s.tween = { from: tw.from, k: clamp01((now - tw.t0) / 160), dir: tw.dir };
    }
    /* A still picture is drawn once and replayed as runs; only a moving one
     * (awake, easing, or sliding) is drawn again each frame. */
    const moving = awake || v !== r.v || (s.tween && s.tween.k < 1);
    const sig = String(raw) + "|" + (DEPENDS[key] || []).map((k) => values[k]).join("|") + "|" + ctx.width + "x" + ctx.height;
    const hit = stills.get(sid);
    if (!moving && hit && hit.sig === sig) { replay(ctx, hit.runs); return; }
    const p = pen(ctx.width, ctx.height, SCALE);
    fn(p, v, s);
    const runs = p.runs();
    replay(ctx, runs);
    if (moving) stills.delete(sid); else stills.set(sid, { sig, runs });
}

/* ----------------------------------------------------------- the canvas -- */
/* The same pen, shifted and clipped to the picture, for a sliding one. */
function offset(c, dx) {
    return { fillRect: (x, y, w, h, col) => {
            let a = x + dx, b = a + w; if (a < 0) a = 0; if (b > W) b = W; if (b <= a) return;
            if (y > PIC) return; c.fillRect(a, y, b - a, Math.min(h, PIC + 1 - y), col); },
        line(x0, y0, x1, y1) { lineVia(this, x0, y0, x1, y1); },
        drawCircle(x, y, r) { arcVia(this, x, y, r, 0, 360); },
        drawArc(x, y, r, a, sw) { arcVia(this, x, y, r, a, sw); },
        text(str, y, x) { if (dx === 0) c.text(str, y, x); } };
}
function lineVia(c, x0, y0, x1, y1) {
    let dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (let g = 0; g < 512; g++) { c.fillRect(x0, y0, 1, 1, 1); if (x0 === x1 && y0 === y1) break; const e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
}
function arcVia(c, x, y, r, start, sweep) {
    const plot = (dx, dy) => { if (sweep < 360) { let a = Math.atan2(dx, -dy) * 180 / Math.PI; if (a < 0) a += 360; let d = a - start; if (d < 0) d += 360; if (d > sweep) return; } c.fillRect(x + dx, y + dy, 1, 1, 1); };
    for (let dy = -r; dy <= r; dy++) { const dx = Math.round(Math.sqrt(r * r - dy * dy)); plot(dx, dy); if (dx) plot(-dx, dy); }
    for (let dx = -r; dx <= r; dx++) { const dy = Math.round(Math.sqrt(r * r - dx * dx)); plot(dx, dy); if (dy) plot(dx, -dy); }
}

/* The pen draws into a bitmap the size of the frame, so every shape is clipped
 * once, overlapping strokes cost nothing, and the result goes to the host as a
 * few horizontal runs rather than a call per pixel. */
const stills = new Map();
/* Pictures are drawn in a 32x12 design space and scaled down by SCALE about
 * its centre BEFORE they become pixels: a line stays one pixel, a circle stays
 * round, and the picture sits small in the middle of its cell. */
const SCALE = 0.8;
const GLYPH = {
    0: "111101101101111", 1: "010110010010111", 2: "111001111100111", 3: "111001111001111", 4: "101101111001001",
    5: "111100111001111", 6: "111100111101111", 7: "111001001001001", 8: "111101111101111", 9: "111101111001111",
    "/": "001001010100100", ".": "00001", T: "111010010010010", B: "110101110101110", A: "010101111101101",
    R: "110101110101101", M: "101111111101101", S: "111100111001111", C: "111100100100111", D: "110101101101110",
};
function pen(w, h, k) {
    const w0 = w;
    const bm = new Uint8Array(w * h);
    const cx = w / 2 - 0.5, cy = h / 2 - 0.5;
    const tx = (x) => Math.round(cx + (x - 15.5) * k), ty = (y) => Math.round(cy + (y - 5.5) * k);
    const plot = (x, y) => { if (x >= 0 && y >= 0 && x < w && y < h) bm[y * w + x] = 1; };
    const raw = { fillRect(x, y, rw, rh) { for (let j = y; j < y + rh; j++) for (let i = x; i < x + rw; i++) plot(i, j); } };
    const p = {
        width: W, height: PIC + 1,
        fillRect(x, y, rw, rh) {
            const x0 = tx(x), x1 = tx(x + Math.max(1, rw) - 1), y0 = ty(y), y1 = ty(y + Math.max(1, rh) - 1);
            for (let j = y0; j <= y1; j++) for (let i = x0; i <= x1; i++) plot(i, j);
        },
        line(x0, y0, x1, y1) { lineVia(raw, tx(x0), ty(y0), tx(x1), ty(y1)); },
        drawArc(x, y, r, a, sw) { arcVia(raw, tx(x), ty(y), Math.max(1, Math.round(r * k)), a, sw); },
        drawCircle(x, y, r) { arcVia(raw, tx(x), ty(y), Math.max(1, Math.round(r * k)), 0, 360); },
        rawRect(x, y, rw, rh) { raw.fillRect(x, y, rw, rh); },
        /* Words are not scaled: GLYPH's 3x5 figures from raw row y, centred,
         * or from raw column x. */
        text(str, y, at) {
            const tw = [...str].reduce((a, ch) => a + (ch === "." ? 2 : 4), 0) - 1;
            let x = at != null ? at : Math.round(tw > 0 ? (w0 - tw) / 2 : 0);
            for (const ch of str) {
                const g = GLYPH[ch]; if (!g) { x += 4; continue; }
                const gw = ch === "." ? 1 : 3;
                for (let r = 0; r < 5; r++) for (let i = 0; i < gw; i++) if (g[r * gw + i] === "1") plot(x + i, y + r);
                x += gw + 1;
            }
        },
        runs() {
            const out = [];
            for (let j = 0; j < h; j++) for (let i = 0; i < w; i++) {
                if (!bm[j * w + i]) continue;
                const a = i; while (i < w && bm[j * w + i]) i++;
                out.push(a, j, i - a);
            }
            return out;
        },
    };
    return p;
}
function replay(ctx, runs) { for (let i = 0; i < runs.length; i += 3) ctx.fillRect(runs[i], runs[i + 1], runs[i + 2], 1, 1); }

const overlay = { /* for tests */ _reset() { tweens.clear(); phases.clear(); slides.clear(); lastTurn.clear(); stills.clear(); },
    widgetKinds: ["custom:strut"], drawCell, _D: D, _roleOf: roleOf, _LISTS: LISTS };
globalThis.canvas_overlay = overlay;
})();
