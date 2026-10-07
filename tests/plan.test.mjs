// Plans Strut's pages with the host's own planner, in every combination of
// the three MOD switches, runs the host's contract validator, and fits every
// cell label with the host's own fitter.
// Usage: node plan.test.mjs <dump dir> <schwung checkout>
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const [dir, schwung] = process.argv.slice(2);
const here = path.dirname(fileURLToPath(import.meta.url));
const load = (rel) => import(pathToFileURL(path.join(schwung, "src/shared/param_pages", rel)));
const { planPages } = await load("page_plan.mjs");
const { validateContract } = await load("validate_contract.mjs");
const { labelVerbatim, HEADER_MIN_LEFT, HEADER_GAP } = await load("render_page_movy.mjs");
const { fontWidth4x5 } = await load("font4x5.mjs");
const { resolveChildKey } = await load("child_key.mjs");

const hierarchyText = fs.readFileSync(path.join(dir, "ui_hierarchy.json"), "utf8");
const chainText = fs.readFileSync(path.join(dir, "chain_params.json"), "utf8");
const hierarchy = JSON.parse(hierarchyText);
const chainParams = JSON.parse(chainText);
const moduleJson = JSON.parse(fs.readFileSync(path.join(here, "../src/module.json"), "utf8"));
const byKey = Object.fromEntries(chainParams.map((p) => [p.key, p]));

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FAIL " + msg); } };

// DESIGN.md, Pads and focus: the whole contract stays well inside the
// host's 128 KB buffers, and inside the chain's 256-entry table.
const bytes = hierarchyText.length + chainText.length;
check(bytes < 32 * 1024, `the contract is ${bytes} bytes, budget 32 KB`);
check(chainParams.length <= 128, `${chainParams.length} chain_params entries, the chain keeps 256`);

// CLAUDE.md: every cell is a real word of five letters or fewer, drawn as typed.
for (const p of chainParams) {
    check(typeof p.name === "string" && p.name.length > 0, `${p.key} declares a header name`);
    check(typeof p.short_name === "string" && p.short_name.length <= 5, `${p.key}: cell "${p.short_name}" is 1 to 5 letters`);
    const drawn = labelVerbatim(p.short_name);
    check(drawn === p.short_name.toUpperCase(), `${p.key}: cell "${p.short_name}" draws as "${drawn}"`);
    check(p.short_name.toUpperCase() !== "MOVE", `${p.key}: no label is MOVE`);
    if (p.type === "enum") check(p.options_as_string === true, `${p.key}: served as option names, so gates match`);
}

// The chain types p05_tune through module.json's template, not the served
// hierarchy (chain_params.c, parse_child_templates). They must agree.
const racks = Object.entries(hierarchy.levels).filter(([, l]) => l.child_key_template);
const staticRoot = moduleJson.capabilities.ui_hierarchy.levels.root;
for (const f of ["child_key_template", "child_count", "child_index_base", "child_index_digits"])
    for (const [name, l] of racks) check(l[f] === staticRoot[f], `${name}.${f} matches module.json (${l[f]} vs ${staticRoot[f]})`);
check(racks.length === 5, `Pad, Skin, Wave, Noise and Finish are per pad, got ${racks.map(([n]) => n)}`);
check(!hierarchy.levels.kit.child_key_template, "Kit is not per pad");

// Every pad key a page can address is a key the module serves.
const padKeys = racks.flatMap(([, l]) => l.knobs).filter((k) => !/_view$/.test(k));
for (const [name, l] of racks) for (const k of l.knobs) {
    const first = resolveChildKey(l, 0, k), last = resolveChildKey(l, 15, k);
    if (/_view$/.test(k)) check(first === k && last === k, `${name}: ${k} is one key for every pad`);
    else check(/^p01_/.test(first) && /^p16_/.test(last), `${name}: ${k} resolves to ${first} .. ${last}`);
}

// The shadow UI's gate comparison (compareConditionValue): an exact string match.
const views = ["skin_view", "wave_view", "noise_view"];
const visibleFor = (values) => (cond) => {
    const v = values[cond.param];
    if (v === undefined) return true;
    if (cond.equals !== undefined) return String(v) === String(cond.equals);
    if (cond.not_equals !== undefined) return String(v) !== String(cond.not_equals);
    return true;
};
const engines = { skin: "skin_view", wave: "wave_view", noise: "noise_view" };
const order = ["root", "skin", "wave", "noise", "finish", "kit"];

for (let mask = 0; mask < 8; mask++) {
    const values = Object.fromEntries(views.map((v, i) => [v, mask & (1 << i) ? "Mod" : "Sound"]));
    const plan = planPages({ hierarchy, chainParams, visible: visibleFor(values) });
    const pages = plan.pages.filter((p) => Array.isArray(p.keys));
    const all = pages.flatMap((p) => p.keys);
    const tag = views.map((v) => values[v][0]).join("");

    check(pages.map((p) => p.level).join() === order.join(), `${tag}: one page each, in order, got ${pages.map((p) => p.level)}`);
    check(new Set(all).size === all.length, `${tag}: no knob appears twice`);
    const hidden = Object.entries(engines).flatMap(([lv, v]) => hierarchy.levels[lv].params
        .filter((e) => e.visible_if && e.visible_if.equals !== values[v]).map((e) => e.key));
    check(padKeys.every((k) => all.includes(k) !== hidden.includes(k)), `${tag}: every pad key not hidden is on a page`);
    for (const [level, view] of Object.entries(engines)) {
        const page = pages.find((p) => p.level === level);
        const want = hierarchy.levels[level].params
            .filter((e) => e.visible_if && e.visible_if.equals === values[view]).map((e) => e.key);
        check(page && page.keys.length === 8, `${tag} ${level}: 8 cells, got ${page && page.keys}`);
        check(page && page.keys[7] === view, `${tag} ${level}: MOD is cell 8`);
        check(page && page.keys.slice(0, 7).join() === want.join(), `${tag} ${level}: the ${values[view]} view's seven`);
    }
    /* no Selected Pad page: Kit's PAD cell stands in for it */
    const pickers = plan.pages.filter((p) => p.childOf);
    check(pickers.length === 0, `${tag}: no pad picker page, got ${pickers.length}`);
    const kit = pages.find((p) => p.level === "kit");
    check(kit && kit.keys[kit.keys.length - 1] === "pad", `${tag}: Kit's last cell is PAD`);
    check([...(plan.conditionKeys || [])].sort().join() === [...views].sort().join(),
          `${tag}: the gates are the three MOD switches, got ${[...(plan.conditionKeys || [])]}`);
    if (mask === 0 || mask === 7) {
        const cells = pages.map((p) => `${p.level}[${p.keys.map((k) => byKey[k].short_name.toUpperCase()).join(" ")}]`);
        console.log(`${tag} ${cells.join(" ")}`);
    }
}
check(byKey.skin_view.options.join() === "Sound,Mod", "MOD is Sound or Mod, so a click flips it");

// Every page title fits the header whole, next to the slot's title.
const pageRoom = 128 - 4 - HEADER_MIN_LEFT - HEADER_GAP;
for (const [k, l] of Object.entries(hierarchy.levels)) {
    const w = fontWidth4x5(String(l.name).toUpperCase());
    check(w <= pageRoom, `page "${l.name}" (${k}) is ${w} px, the header has ${pageRoom}`);
}
// A pad's page is titled "<child_label> <pad>", not by its level's name
// (page_controller.mjs, pageLabel), so each rack's child_label names its page.
{
    const plan = planPages({ hierarchy, chainParams, visible: visibleFor({ skin_view: "Sound", wave_view: "Sound", noise_view: "Sound" }) });
    const titles = plan.pages.filter((p) => p.childLevel && Array.isArray(p.keys))
        .map((p) => `${p.childLevel.child_label} 16`);
    check(titles.join() === "Pad 16,Skin 16,Wave 16,Noise 16,Finish 16", `pad pages are titled by page, got ${titles}`);
    for (const t of titles) check(fontWidth4x5(t.toUpperCase()) <= pageRoom, `"${t}" fits the header`);
    check(!plan.pages.some((p) => p.childOf), "no Selected Pad page");
}

const { findings } = validateContract({ id: "strut", hierarchy, chainParams, capabilities: moduleJson.capabilities });
for (const f of findings) if (f.level !== "info") console.log(`  validate ${f.level}: ${f.rule} — ${f.message}`);
check(!findings.some((f) => f.level === "error"), "the host's validator reports no errors");

console.log(`contract: ${bytes} bytes, ${chainParams.length} keys declared, ${padKeys.length * 16 + chainParams.length - padKeys.length} addressable (${padKeys.length} per pad)`);
console.log(fails ? `FAIL: plan.test ${fails} failed` : "ok: plan.test");
process.exit(fails ? 1 : 0);
