// Plans Strut's pages with the host's own planner and runs the host's
// contract validator. Usage: node plan.test.mjs <dump dir> <schwung checkout>
import fs from "node:fs";
import path from "node:path";
import { pathToFileURL } from "node:url";

const [dir, schwung] = process.argv.slice(2);
const load = (rel) => import(pathToFileURL(path.join(schwung, "src/shared/param_pages", rel)));
const { planPages } = await load("page_plan.mjs");
const { validateContract } = await load("validate_contract.mjs");

const hierarchy = JSON.parse(fs.readFileSync(path.join(dir, "ui_hierarchy.json"), "utf8"));
const chainParams = JSON.parse(fs.readFileSync(path.join(dir, "chain_params.json"), "utf8"));

let fails = 0;
const check = (ok, msg) => { if (!ok) { fails++; console.log("FAIL " + msg); } };

const plan = planPages({ hierarchy, chainParams, visible: () => true });
const pages = plan.pages.filter((p) => Array.isArray(p.keys));
check(pages.length > 0, "the planner makes at least one knob page");
for (const p of pages) console.log(`${p.title || p.level || "?"}: [${p.keys.join(" ")}]`);

const report = validateContract({ hierarchy, chainParams });
const errors = ((report && report.findings) || []).filter((f) => f.level === "error");
for (const e of errors) console.log("FAIL validate: " + JSON.stringify(e));
fails += errors.length;

console.log(fails ? `FAIL: plan (${fails})` : "ok: plan");
process.exit(fails ? 1 : 0);
