# Working on Strut

This file is the handoff for Claude sessions working on Strut, including cloud
sessions. Read it, then DESIGN.md, before doing anything.

## What this is

Strut is a drum instrument (`sound_generator`) for Schwung, the module host on
Ableton Move. It has sixteen pads, each its own drum, built from three engines:

- **Skin**, a resonator;
- **Wave**, a wavetable and analogue oscillator;
- **Noise**, a noise table or sample.

There is one modulator per engine, behind a MOD switch on that engine's page.
The goal is a very wide palette from very few, easy-to-remember controls, and a
sound that is really good.

DESIGN.md has the plan, the control surface and the **Build order**. Work
through the Build order in order. Strike through each step as it lands, and keep
DESIGN.md saying what actually runs versus what is planned.

## Rules from the user (follow exactly)

- **Never name the commercial drum synth that inspired Strut, or its maker,
  anywhere:** files, commits, comments, PRs, issues. Describe ideas in our own
  words, and do not copy text from its manual.
- **Never copy or read GPL code** (for example OpenWurli or Surge sources).
  Write from papers and first principles.
- **Labels are real words of five letters or fewer** (`PITCH`, `RING`,
  `METAL`). The header carries the full name. No made-up abbreviations. Check
  each label draws as typed with the host's label fitter (see Quilt's
  `tests/plan.test.mjs`, `labelVerbatim`).
- **Explain plainly.** The user is a musician first: say what something does
  and why in ordinary words before offering technical choices. Use they/them
  for the user.
- **Brief progress updates** while working.
- **Commit and push each batch of work** to `main`, with clear messages.
- **Ask before anything outward-facing:** releases, tags, catalog PRs, making
  the repo public, posting anywhere. Releases always need an explicit
  go-ahead.
- **Ask before downloading** anything beyond the repo's own dependencies and
  the Schwung checkout.
- **Instruments need these before release:**
  - sensible knob minimums (no knob setting gives silence or nonsense);
  - a voicing pass with the user listening;
  - level-matched presets;
  - under a quarter of the Move's CPU at full load.
- **Do not open PRs against Schwung** unless the user asks. Work within what
  the host already does.

## House style (from the sibling modules)

- Pure C11, plugin API v2, `-Wall -Wextra -Werror`, no allocation or locks on
  the audio thread.
- No JavaScript UI. The pages come from `ui_hierarchy` and `chain_params`,
  served by `get_param`.
- **DESIGN.md** shape:
  - the pitch under the title;
  - a status line;
  - Module ID;
  - Lineage (real papers);
  - concept sections that argue for decisions;
  - control-surface tables;
  - Implementation notes;
  - Build order;
  - Testing.

  Record **rejected alternatives inline, with the reason**, so they are not
  re-proposed.
- `help.json` is shipped, and every line is at most 20 characters.
- Comments explain *why*, briefly. Match the density of the existing code.

## The host (Schwung)

You need a checkout of the host to test against:

```bash
git clone --depth 1 https://github.com/charlesvestal/schwung .schwung
```

`tests/run.sh` uses `../schwung` if it exists, otherwise `.schwung`.

What matters most in the host:

- **`docs/MODULES.md`:**
  - *Parameter Item Types*;
  - *Naming a parameter*;
  - the drum-rack section with `child_key_template`, `child_index_param`,
    `child_press_param` and *Live presses*;
  - *A focus answer may carry a CHANGE TOKEN*.
- **`src/shared/param_pages/page_plan.mjs`** turns the contract into pages.
  Hidden knobs close up before the 8-per-page chunking.
- **`src/shared/param_pages/validate_contract.mjs`**: the tests run it, and its
  errors fail them.
- **`src/shared/param_pages/page_controller.mjs`:**
  - `flipsOnClick`: a two-option enum flips on a jog click;
  - the enum "peek" (the full-screen option list on turn), which a param can
    turn off with `"peek": false`.

Known host behaviour, learned on Quilt:

- **Gates.**
  - `visible_if` goes on hierarchy levels, or on a level's `params`/`knobs`
    entries. It is silently ignored in `chain_params`.
  - A gate is one comparison: `equals`, `not_equals`, `gt`, `lt`, `truthy` or
    `falsey`. `equals` is an exact string match, so serve enums as option
    names (`options_as_string: true`).
  - The grid re-plans immediately only when the key just written is itself a
    gate key. Other gates are re-read on a pad press or focus change, and at
    most four keys then.
- **Size.** `chain_params` and `ui_hierarchy` share 128 KB buffers. Keep the
  total well under that; measure it in the tests.
- **The binary.** A synth slot always loads `dsp.so`, whatever module.json
  says.
- **Logs.** Device logs are in `/data/UserData/schwung/debug.log`.

## Tests and builds

```bash
bash tests/run.sh          # native build, black-box tests, host planner + validator
./scripts/build.sh         # aarch64 dsp.so + dist/strut-module.tar.gz (needs aarch64-linux-gnu-gcc or Docker)
```

The CI (`.github/workflows/ci.yml`) runs the tests and uploads a device build,
the `strut-module` artifact, on every push. This session cannot reach the Move.
To try a build on the device, the user downloads the artifact and runs:

```bash
gh run download -n strut-module -D /tmp/strut && scripts/install.sh /tmp/strut/strut-module.tar.gz
```

Tell the user when a build is worth trying, and what to listen for.

## The sibling repos (for reference, same owner)

- **`kliegsablaze/quilt`**, a soft-instrument `sound_generator`, the closest
  relative. Worth reading:
  - `src/dsp/contract.c`: building the contract in C, gates, label rules;
  - `src/dsp/mod.c`: the modulation overlay that never writes a knob;
  - `tests/plan.test.mjs`: planning every layout and fitting labels;
  - its DESIGN.md: host findings.

  Its plate reverb (`src/dsp/fx.c`) and modal resonators are ours and may be
  reused.
- `kliegsablaze/ragtag`: loading WAVs off the audio thread.

## Handing back

When the user moves work back to their local machine:

- Leave DESIGN.md's status line current.
- Strike through finished Build order steps.
- Push everything.
- Leave a short **"Where things stand"** note at the bottom of this file:
  - what works;
  - what was tried on the device;
  - what is next;
  - open questions for the user.
