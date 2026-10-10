// Draws Strut's knob pictures (src/canvas.js) through the host's own widget
// registry, frame context and viz resolver, on every page in both views of
// each engine. Usage: node widgets.test.mjs <dump dir> <schwung checkout>
import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";
import { pathToFileURL, fileURLToPath } from "node:url";

const [dir, schwung] = process.argv.slice(2);
const here = path.dirname(fileURLToPath(import.meta.url));
const load = (rel) => import(pathToFileURL(path.join(schwung, "src/shared/param_pages", rel)));
const { planPages } = await load("page_plan.mjs");
const { buildMetaIndex } = await load("param_meta.mjs");
const { resolveViz } = await load("viz.mjs");
const { frameCtx } = await load("frame_ctx.mjs");
const { registerOverlayWidgets, getWidget, clearWidgets } = await load("widget_registry.mjs");

const hierarchy = JSON.parse(fs.readFileSync(path.join(dir, "ui_hierarchy.json"), "utf8"));
const chainParams = JSON.parse(fs.readFileSync(path.join(dir, "chain_params.json"), "utf8"));
const byKey = Object.fromEntries(chainParams.map((p) => [p.key, p]));
const metaIndex = buildMetaIndex({ hierarchy, chainParams });
const moduleJson = JSON.parse(fs.readFileSync(path.join(here, "../src/module.json"), "utf8"));

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FAIL " + msg); } };

check(moduleJson.capabilities.canvas_script === "canvas.js", "module.json declares canvas.js");

// Loaded the way the host loads it: a script over a bare global.
const g = {};
g.globalThis = g;
vm.runInNewContext(fs.readFileSync(path.join(here, "../src/canvas.js"), "utf8"), g);
const ov = g.canvas_overlay;
clearWidgets();
const { registered } = registerOverlayWidgets(ov);
check(registered.includes("custom:strut"), `custom:strut registers, got ${registered}`);
const impl = getWidget("custom:strut");

// Every control names the picture, and has one.
for (const p of chainParams) {
    check(p.viz && p.viz.kind === "custom:strut", `${p.key} declares the Strut picture`);
    check(typeof ov._D[ov._roleOf(p.key)] === "function", `${p.key} has a picture`);
}
// The file's option lists are the ones Strut serves (a renamed kit or table
// would otherwise draw as its first option).
for (const p of chainParams.filter((q) => q.type === "enum")) {
    const role = ov._roleOf(p.key);
    const list = p.key === "s_curve" ? ov._LISTS.s_curve : role === "aim" ? ov._LISTS[p.key[0] + "_aim"] : role === "kit_dice" ? ov._LISTS.dice : ov._LISTS[role] || ov._LISTS[p.key];
    const served = p.key === "n_table" ? p.options.slice(0, list ? list.length : 0) : p.options;
    check(list && list.join() === served.join(), `${p.key}: canvas.js knows its options (${served.length})`);
}

/* A host ctx that paints a 128x64 screen, so a picture can be compared. */
function screen() {
    const px = new Uint8Array(128 * 64);
    let calls = 0;
    return { px, calls: () => calls, textWidth: (s) => String(s).length * 5,
        fillRect(x, y, w, h, c) { calls++; for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) px[j * 128 + i] = c ? 1 : 0; },
        print() {} };
}
function draw(key, values, nowMs, touched) {
    const s = screen();
    const f = frameCtx(s, { x: 0, y: 0, w: 32, h: 15 });
    impl.draw(f, { group: { kind: "custom:strut", keys: [key] }, values, nowMs, touched });
    return { lit: s.px.reduce((a, b) => a + b, 0), clipped: f.clipped(), calls: s.calls(), px: s.px };
}
function value(key, u) {
    const m = byKey[key];
    if (m.type === "enum") return m.options[Math.round(u * (m.options.length - 1))];
    const lo = m.min ?? 0, hi = m.max ?? 1, v = lo + u * (hi - lo);
    return m.type === "int" ? Math.round(v) : v;
}

const views = ["skin_view", "wave_view", "noise_view"];
const visibleFor = (values) => (cond) => {
    const v = values[cond.param];
    if (v === undefined) return true;
    if (cond.equals !== undefined) return String(v) === String(cond.equals);
    if (cond.not_equals !== undefined) return String(v) !== String(cond.not_equals);
    return true;
};
let worstCalls = 0, worstKey = "", cells = 0;
for (const view of ["Sound", "Mod"]) {
    const vals0 = Object.fromEntries(views.map((v) => [v, view]));
    const pages = planPages({ hierarchy, chainParams, visible: visibleFor(vals0) }).pages.filter((p) => Array.isArray(p.keys));
    for (const page of pages) {
        // The host's resolver gives every cell its own one-cell picture.
        const { groups } = resolveViz({ keys: page.keys, metaIndex });
        for (const k of page.keys) {
            const grp = groups.find((x) => x.keys.includes(k));
            check(grp && grp.kind === "custom:strut" && grp.slotSpan === 1, `${view}: ${k} draws as its own Strut cell`);
        }
        for (const k of page.keys) {
            const m = byKey[k];
            const us = m.type === "enum" ? m.options.map((_, i) => i / Math.max(1, m.options.length - 1)) : [0, 0.37, 0.5, 1];
            for (const u of us) {
                ov._reset();
                const vals = { ...vals0, w_table: "Fold", s_kind: "LFO", [k]: value(k, u) };
                let r;
                try { r = draw(k, vals, 1000); } catch (e) { check(false, `${k}@${u} throws ${e}`); continue; }
                cells++;
                check(r.clipped === 0, `${k}@${u} draws outside its frame (${r.clipped})`);
                check(r.lit >= 5, `${k}@${u} draws a picture (${r.lit} px)`);
                if (r.calls > worstCalls) { worstCalls = r.calls; worstKey = `${k}@${u}`; }
            }
        }
    }
}

// A sample's name, not a noise table, still draws.
ov._reset();
check(draw("n_table", { n_table: "808 Kick" }, 1000).lit >= 5, "a sample in Noise > TABLE draws");

// No answer yet: nothing at all, never a picture of a made-up value.
ov._reset();
check(draw("tune", {}, 1000).lit === 0, "an unanswered value draws nothing");

// Small and centred: every picture keeps a clear margin inside its cell.
for (const k of Object.keys(byKey)) {
    ov._reset();
    const r = draw(k, { [k]: value(k, 1), w_table: "Analog" }, 1000);
    let x0 = 99, x1 = -1;
    for (let y = 0; y < 15; y++) for (let x = 0; x < 32; x++) if (r.px[y * 128 + x]) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); }
    check(x0 >= 2 && x1 <= 29, `${k} stays clear of its cell's edges (${x0}..${x1})`);
    check(Math.abs((x0 + x1) / 2 - 15.5) <= 5, `${k} sits near the middle of its cell (${x0}..${x1})`);
}

// Different settings look different.
const looks = (k, a, b, extra) => { ov._reset(); const p = draw(k, { ...extra, [k]: a }, 1000).px; ov._reset(); const q = draw(k, { ...extra, [k]: b }, 1000).px; return p.some((x, i) => x !== q[i]); };
for (const k of Object.keys(byKey)) {
    const m = byKey[k];
    if (m.type === "enum") for (let i = 1; i < m.options.length && i < 60; i++)
        check(looks(k, m.options[i - 1], m.options[i]), `${k}: ${m.options[i - 1]} and ${m.options[i]} look different`);
    else check(looks(k, value(k, 0.2), value(k, 0.8)), `${k}: 20% and 80% look different`);
}
check(looks("w_wave", 0.5, 0.5, { w_table: "Analog" }) === false, "a still WAVE replays");
{
    ov._reset(); const a = draw("w_wave", { w_wave: 0.5, w_table: "Analog" }, 1000).px;
    ov._reset(); const b = draw("w_wave", { w_wave: 0.5, w_table: "Metal" }, 1000).px;
    check(a.some((x, i) => x !== b[i]), "Wave > WAVE draws the table Wave > TABLE picks");
}

// A still picture replays exactly what it drew.
ov._reset();
const a = draw("s_metal", { s_metal: 0.6 }, 1000);
const b = draw("s_metal", { s_metal: 0.6 }, 1040);
check(a.px.every((x, i) => x === b[i] || x === b.px[i]), "a still picture replays the same pixels");

// Time controls sleep until turned or touched, then move.
ov._reset();
const t0 = draw("s_rate", { s_rate: 0.3, s_kind: "LFO" }, 1000);
const t1 = draw("s_rate", { s_rate: 0.3, s_kind: "LFO" }, 1200);
check(t0.px.every((x, i) => x === t1.px[i]), "RATE holds still while nobody touches it");
const t2 = draw("s_rate", { s_rate: 0.3, s_kind: "LFO" }, 1400, true);
const t3 = draw("s_rate", { s_rate: 0.3, s_kind: "LFO" }, 1550, true);
check(t2.px.some((x, i) => x !== t3.px[i]), "RATE moves while its knob is touched");
draw("s_rate", { s_rate: 0.4, s_kind: "LFO" }, 5000);
const t4 = draw("s_rate", { s_rate: 0.4, s_kind: "LFO" }, 5300);
const t5 = draw("s_rate", { s_rate: 0.4, s_kind: "LFO" }, 5450);
check(t4.px.some((x, i) => x !== t5.px[i]), "RATE moves just after its knob is turned");

// RATE says its speed in figures: note values right of centre, time left.
const rateLit = (v) => { ov._reset(); return draw("s_rate", { s_rate: v }, 1000).px; };
check(rateLit(0.5).some((x, i) => x !== rateLit(0.6)[i]), "RATE's figures change from 1/8 to 1/8T");
check(rateLit(-0.5).some((x, i) => x !== rateLit(0.5)[i]), "RATE draws free and in-time speeds differently");

console.log(`  ${cells} cells drawn; widest: ${worstCalls} host calls (${worstKey})`);
check(worstCalls <= 100, `every cell reaches the host in at most 100 runs, worst ${worstCalls}`);
console.log(fails ? `FAIL: widgets.test ${fails} failed` : "ok: widgets.test");
process.exit(fails ? 1 : 0);
