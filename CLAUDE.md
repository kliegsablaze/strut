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
- **Name the page with every control** in instructions to the user, as
  page > knob: "Wave > TABLE to Metal", "Pad > NOISE up" (2026-10-07).
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
- **Make no requests to Schwung**: no PRs, issues or proposals (the user
  declined one for the host's volume-stage rounding, 2026-10-07). Work within
  what the host already does.

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

`tests/run.sh` uses `../schwung` if it exists, otherwise `.schwung`. Test against the
release the Move runs (`git -C .schwung fetch --depth 1 origin tag v1.7.3 &&
git -C .schwung checkout v1.7.3`), not the newest commit.

**On the device, "no sound and the pages do not follow" first means MIDI is
off for the slot** (2026-10-07): page-follow pairs a press with its note, so
it needs notes too.

**After installing, the Move must restart** (2026-10-07; `install.sh` does it): a slot already playing
Strut keeps the old code, because the host opens the new `dsp.so` before
closing the old and `dlopen()` returns the copy it has. Strut logs
`strut <version> loaded` to the debug log, so check which build played.

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
To try a build on the device, the user runs this from the repo; it waits for
the newest CI run to finish (stopping if it fails), then installs it:

```bash
git pull && id=$(gh run list -L 1 -b main --json databaseId -q '.[0].databaseId') && gh run watch $id --exit-status && rm -rf /tmp/strut && gh run download $id -n strut-module -D /tmp/strut && scripts/install.sh /tmp/strut/strut-module.tar.gz
```

To measure the CPU on the Move, the same with the bench:
`... && rm -rf /tmp/strut-bench && gh run download $id -n strut-bench -D /tmp/strut-bench && scripts/bench.sh /tmp/strut-bench/bench-aarch64`.
Name the run (`$id`) in every download: without it, `gh run download` can
take an older run's artifact, and did once (2026-10-07), while commits were
landing on two branches.

`install.sh` then restarts the Move (`reboot` as root, the host installer's
way) and waits for it to come back; if root login is not set up it says to
restart by hand. Tell the user when a build is worth trying, and what to
listen for.

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

## Where things stand (2026-10-07, 0.3.1)

- **What works:**
  - all three engines: Skin, Wave and Noise (eight noise tables), with
    Skin bending Wave and Wave or Noise striking Skin;
  - the Pad page: SKIN, WAVE and NOISE are the engines' levels, LEVEL the
    pad's, TUNE, DECAY and COLOR;
  - the Finish page: PAN, CHOKE, FLAM, DRIVE, CRUSH, LOW, HIGH.
  - Tests: 1129 checks, plus the host's planner and validator.
- **Tried on the device:**
  - Skin, its grit fixed (0.0.6), and page titles;
  - Noise "sounds great" (0.2.2);
  - CPU, all three engines on every pad at their dearest: 12 %;
  - load time: 0.51 s (0.2.2), after work on the tables for the Move's
    memory (DESIGN.md, *Tables are built at load*).
  - every effect on every pad: 18.7 % (0.3.0); 0.3.1 runs them in one
    loop, a third of the cost on a laptop, not yet measured on the Move.
  - Not yet: Finish by ear.
- **Next:** build step 7, the modulators (the MOD views). Samples are step
  9: the library in `src/samples/` (208 sounds, from another session) is
  in DESIGN.md, *The sample library*.
- **Open questions for the user:**
  - how TABLE should reach 216+ options at step 9 (one long list, the host's
    file browser, or a folder and a number);
  - whether Wave's and Noise's loudness against Skin's suits them (the
    voicing pass, step 11).
