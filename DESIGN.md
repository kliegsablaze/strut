# Strut

*Sixteen drums, each one built from three engines and played from eight knobs.*

**Status:** contract probe, 0.0.2 (2026-10-07). Every proposed knob, on every
page and both views of each engine page, is declared, kept per pad and planned
by the host's own planner in the tests. The sound is still the scaffold's
placeholder sine per pad, reading only TUNE, DECAY and LEVEL. None of the
engines exists yet: from **The idea in plain words** down, the pages and keys
are real, the sound behind them is the plan.

- **Module ID:** `strut`
- **Component type:** `sound_generator`, plugin API v2, pure C, no JavaScript UI
  (knob pictures may follow Quilt's `canvas.js` later, see *Later*).
- **Host:** Schwung 1.7.3 (the Move runs 1.7.3; the scaffold uses nothing newer).

---

## Lineage

What the three engines are, and the papers they lean on:

- **The resonator** is the analogue drum: a short exciter rings a filter that is
  close to oscillating. This is how the TR-808's kick and toms work.
  - K. J. Werner, J. S. Abel, J. O. Smith. *A Physically-Informed,
    Circuit-Bounded, Analog Synth Model of the Roland TR-808 Bass Drum.*
    DAFx 2014.
  - K. J. Werner. *Virtual Analog Modeling of Audio Circuitry Using Wave
    Digital Filters* (PhD thesis, Stanford 2016): the 808 cowbell and
    cymbal circuits.
  - M. Mathews, J. O. Smith. *Methods for Synthesizing Very High Q
    Parametrically Well Behaved Two Pole Filters.* SMAC 2003. The phasor
    resonator, already used in Quilt.
  - A. Zavalishin. *The Art of VA Filter Design* (2012/2018). The
    trapezoidal state-variable filter, stable under fast modulation.
- **The oscillator** is the tonal body: a wavetable, morphed by an envelope,
  frequency-modulated by the resonator.
  - V. Välimäki, J. Pekonen, J. Nam. *Perceptually informed synthesis of
    bandlimited classical waveforms using integrated polynomial
    interpolation.* JASA 131(1), 2012 (polyBLEP).
  - Mipmapped wavetables, one table per octave, so a high note does not
    alias.
- **The noise source** is the hiss, the metal and the sample: a loop that
  repeats without ever sounding pitched, or a recording played straight, as a
  sampler plays it.
  - X. Serra, J. O. Smith. *Spectral Modeling Synthesis.* CMJ 14(4), 1990.
    Keep a sound's spectrum, throw away its phase, and it becomes noise of the
    same colour. This is how the built-in noise tables are made.
  - T. I. Laakso, V. Välimäki, M. Karjalainen, U. K. Laine. *Splitting the
    Unit Delay.* IEEE Signal Processing Magazine 13(1), 1996. Reading a sample
    between its stored points, so it can be played at any pitch.
  - The 808 and 909 hi-hats: six square oscillators at inharmonic
    frequencies through band-pass filters (Werner, above).
- **Drum physics**, for the presets and the randomiser's sense of what a
  drum is:
  - T. D. Rossing. *Science of Percussion Instruments.* World Scientific, 2000.
  - J. Bilbao. *Numerical Sound Synthesis.* Wiley, 2009. Chapter 11
    (membranes and plates).

Effects are our own, built for drums: a tilt filter, drive, a bit crusher, a
two-band EQ, a transient shaper, and a kit-wide room and glue. The room can start
from Quilt's plate (Dattorro, *Effect Design Part 1*, JAES 1997).

---

## The idea in plain words

Every pad is its own drum. You build each drum from three sources, played
together:

| Engine | Page | What it is | Good for |
|---|---|---|---|
| **Skin** | Skin | a burst that sets a resonant filter ringing | kicks, toms, snare bodies, cowbells, zaps |
| **Wave** | Wave | an oscillator that sweeps through a table of waveforms | punchy bodies, tonal drums, bass hits, blips |
| **Noise** | Noise | a loop of coloured noise, or your own sample played straight | snares, hats, cymbals, claps, texture, and pitched hits cut from melodic samples |

The three can feed each other, and that is where the range comes from:

- **Skin's ringing frequency-modulates Wave.** A decaying FM source is what
  makes a body growl at the start and settle into a tone.
- **Skin can be struck by Wave or Noise** instead of its own burst. A noise
  burst through a high resonance is a snare; a wave through it is a metallic
  ping.

The **Pad** page mixes the three and gives the four knobs used most: TUNE,
DECAY, COLOR and LEVEL. A beginner never needs to leave it; the presets do the
rest. Each engine's page is there when you want to go further.

---

## Pads and focus

There are sixteen pads, notes 36 to 51, which is what a Move drum track sends.
The pages always show **the pad you last hit**:

- The host forwards Move's pad presses to Strut as a *vouch*, `pad_press = "1"`,
  without saying which pad. Strut pairs the vouch with the note it receives
  itself within 50 ms, in either order, and focuses that pad. A note with no
  vouch is the sequencer playing, and moves nothing. This is Schwung's
  `child_press_param` contract (docs/MODULES.md, *Live presses*). **It works in
  the scaffold.**
- The focused pad is `pad` (1 to 16), the template level's `child_index_param`.
  It is turnable, so the pad can be changed without hitting it.
- **To do:** answer `pad` as `"<count>:<pad>"` once the host reads that form
  for `child_index_param`. Otherwise hitting the same pad again after browsing
  away does not come back. Checked on Schwung 1.7.3: the change token is read
  for `focus_param` (the sibling shape) only; `childIndexFromWire` takes a
  plain number and treats `"3:5"` as no answer. So `pad` stays a plain number
  until the host reads the token there too.

**Decided at build step 2: template keys, each declared once.**

Every pad's knob is a real host parameter (`p05_tune`), so Move's automation
and per-step locks reach the right pad. But `chain_params` names each knob only
once, by its bare key (`tune`), and the hierarchy's rack template
(`p{index}_{key}`) multiplies it:

- **The UI** resolves `p05_tune` from `tune` through the template
  (`child_key.mjs`).
- **The chain** does too, for lanes, step locks, scenes and modulation
  (Schwung's CHAIN.md, *A rack's templated keys are typed by the CHAIN*). It
  reads the template from **module.json**, not from the served hierarchy, so
  module.json carries a copy of the rack's template fields, and the tests check
  the two agree.

What it measures: 58 knobs a pad, 937 addressable keys, 67 declared. The whole
contract is **14.6 KB** (hierarchy 5.3 KB, chain_params 9.4 KB) against a
128 KB buffer. Only the eight keys on screen are ever read, so the grid's read
rate does not depend on how many pads there are.

Rejected:

- **Listing all sixteen copies in `chain_params`** (`p01_tune` … `p16_level`),
  which the scaffold did. About 93 KB, which fits the buffer, but the chain
  keeps a **256-entry** table of a synth's parameters and drops the rest
  (`MAX_CHAIN_PARAMS`). With 937 entries every pad past the fourth would be
  invisible to automation, locks and modulation, with only a log line to say so.
- **Focused-pad keys** (`tune` meaning "the focused pad's tune"). Small, but
  automation recorded on one pad plays back on whichever pad is focused. That
  is a real defect for a drum machine.

A host note found on the way: Schwung's MODULES.md says `chain_params` over
64 KB will not load. That line is out of date; the buffer is 128 KB
(`SHADOW_PARAM_VALUE_LEN`), and the load check uses that. At 14.6 KB it does not
matter here.

---

## Pages, and the page within a page

Schwung shows eight knobs a page, and Shift+jog-click opens a list of every
page. Strut keeps that list short. Before them comes the host's **Selected Pad**
list, one for all the per-pad pages, because they share one focus (`pad`):

| Page | For | Shows |
|---|---|---|
| **Pad** (the host titles it **Main**) | the focused pad | the mix and the four big knobs |
| **Skin** | the focused pad | the resonator |
| **Wave** | the focused pad | the oscillator |
| **Noise** | the focused pad | the noise source or sample |
| **Finish** | the focused pad | pan, choke, flam, drive, crush, EQ, punch, dice |
| **Kit** | all pads | room, glue, kit volume |

**The page within a page.** Each engine page has a last knob, **MOD**. Turning
or clicking it swaps that page's other seven knobs for that engine's own
modulation. Turning it back, or clicking again, brings the sound knobs back.
Nothing else on screen moves, and you never leave the page.

How it works in Schwung, as Quilt's Modulation page does already:

- `MOD` is a two-option enum (`Sound`, `Mod`), for example `skin_view`. A
  two-option enum flips on a jog click (`flipsOnClick`), so it works by turning
  or clicking.
- The page's 14 other knobs are all declared on the one level. Seven are gated
  `visible_if: {"param": "skin_view", "equals": "Sound"}` and seven
  `equals: "Mod"`.
- Hidden knobs close up, so `MOD` stays in cell 8 either way.
- A gate re-plans the grid the moment its own knob writes it
  (`replanIfCondition`), so the swap is immediate.
- The `*_view` keys are UI state. They are not saved with a sound, and they
  are not per pad.

This is used three times only, once per engine, so the trick is always in the same
place and always means the same thing.

The switch is one key for the whole kit, not one per pad: the engine pages are
racks keyed `p{index}_{key}`, and `child_key_overrides` maps `skin_view` to
itself, so switching to Mod and hitting another pad keeps you in Mod. The tests
plan all eight combinations of the three switches and check that each engine
page has eight cells with MOD in cell 8.

The host titles the first page **Main** whatever the module calls it
(`page_plan.mjs`, so every module lands on a page with the same name). Strut
keeps that rather than adding an empty first level to get "Pad".

(Rejected: a real pop-up page. Schwung has no knob page that opens over another.
Its "doors" are canvas pages that hand the jog to the module, which would need a
JavaScript UI for every modulation page.)

---

## Control surface

Labels are real words of five letters or fewer. The header shows the full name.
"Bi" means bipolar, with the centre meaning no change.

Every key below is declared and kept (`src/dsp/params.c`). The ranges and
option lists are **provisional** until each engine's build step sets them by
ear: PITCH is ±48 semitones, the SOUND list and the TABLE lists are stand-in
names, and DICE is a plain number until its turn-to-roll gesture (step 10).
The keys are the bare names in this table with an engine prefix where pages
share a word: `s_` Skin, `w_` Wave, `n_` Noise (`s_pitch`, `w_decay`).

### Pad (the focused pad)

| Knob | Key | Label | Behaviour |
|---|---|---|---|
| 1 | `pNN_sound` | SOUND | Picks a starting sound for this pad from the library (Kick, Snare, Hat…), replacing its engines. |
| 2 | `pNN_tune` | TUNE | Bi. Moves all three engines' pitch together, ±24 semitones. |
| 3 | `pNN_decay` | DECAY | Bi. Lengthens or shortens all three envelopes together. |
| 4 | `pNN_color` | COLOR | Bi. Darker to the left (low-pass), thinner to the right (high-pass). |
| 5 | `pNN_skin` | SKIN | Skin's level. |
| 6 | `pNN_wave` | WAVE | Wave's level. |
| 7 | `pNN_noise` | NOISE | Noise's level. |
| 8 | `pNN_level` | LEVEL | The pad's level. |

### Skin (the resonator)

| Knob | Sound view | | Mod view | |
|---|---|---|---|---|
| 1 | PITCH | resonant frequency | KIND | Envelope, LFO, Random or Velocity |
| 2 | RING | resonance: how long the body rings | RATE | speed; synced right of centre, free left |
| 3 | HIT | the exciter: Click, Soft, Burst (Skin's own noise burst), Wave or Noise (the other two engines) | CURVE | Skin's envelope shape: Natural, Ping, Soft, Hold |
| 4 | SNAP | how long the exciter lasts | AIM | destination 1 (below) |
| 5 | METAL | two extra inharmonic partials, rising and louder | DEPTH | Bi |
| 6 | TONE | brightness, a 2-pole low-pass | AIM | destination 2 |
| 7 | MODE | Low, Band or High-pass resonator | DEPTH | Bi |
| 8 | MOD | Sound / Mod | MOD | Sound / Mod |

Skin's destinations: Pitch, Ring, Snap, Metal, Tone, Level.

### Wave (the oscillator)

| Knob | Sound view | | Mod view | |
|---|---|---|---|---|
| 1 | PITCH | | KIND | |
| 2 | BEND | pitch envelope depth, bi | RATE | |
| 3 | DECAY | | CURVE | Natural, Ping, Soft, Hold, Swell (inverted) |
| 4 | TABLE | Analog, then the spectral tables | AIM | |
| 5 | WAVE | position in the table; at the end of Analog, pulse width | DEPTH | |
| 6 | FM | Skin into Wave's frequency | AIM | |
| 7 | RING | ring-modulation; bi: −2 to +2 octaves | DEPTH | |
| 8 | MOD | | MOD | |

Wave's destinations: Pitch, Wave, FM, Ring, Level.

### Noise (the noise source or sample)

| Knob | Sound view | | Mod view | |
|---|---|---|---|---|
| 1 | PITCH | | KIND | |
| 2 | BEND | | RATE | |
| 3 | DECAY | | CURVE | |
| 4 | TABLE | noise tables, then samples | AIM | |
| 5 | COLOR | bi: low-pass left, high-pass right | DEPTH | |
| 6 | START | sample start (samples only) | AIM | |
| 7 | LOOP | loop length; full = no loop (samples only) | DEPTH | |
| 8 | MOD | | MOD | |

Noise's destinations: Pitch, Color, Start, Loop, Level.

**Noise is also a sampler.** Past the noise tables, TABLE lists your own
samples, and a sample plays **straight**: as recorded, not turned into noise.
That is what makes rhythmic parts out of melodic material: put one chord or
bass sample on four pads, tune them apart and play them as a riff. With a
sample chosen:

- **PITCH** is in semitones from the pitch it was recorded at (0 = as
  recorded). Speed and pitch move together, as on a classic sampler. Pad
  TUNE moves it too, so a pad can be tuned to a note.
- **BEND** still sweeps the pitch at the start: a tape-stop or a dive.
- **DECAY** fades the sample out; turned fully right it plays to its end.
- **START** picks where in the sample to begin, so one long sample can feed
  several pads, each from its own slice.
- **LOOP** fully right plays the sample once. Lower, it repeats a slice of that
  length from START, for stutters and drones.
- **COLOR** filters it, as for noise.

Rejected for 1.0: time-stretch, so pitch could move without changing length.
It costs CPU on every pad, smears the attack a drum needs, and a one-shot
rarely wants it. Also left for later: turning your own sample into noise of
its colour (the trick the built-in tables use), as a second way to play it.

### Finish (the focused pad)

| Knob | Label | Behaviour |
|---|---|---|
| 1 | PAN | Bi. |
| 2 | CHOKE | Off, or group A to D: a hit stops the others in its group (open/closed hats). |
| 3 | FLAM | One hit becomes three; how far apart (claps). |
| 4 | DRIVE | Saturation. |
| 5 | CRUSH | Fewer bits and a lower rate. |
| 6 | LOW | Bi. Low shelf, ±18 dB. |
| 7 | HIGH | Bi. High shelf, ±18 dB. |
| 8 | DICE | Turn right to roll a new sound for this pad; turn left to step back through the last eight. |

Noise's mod view uses Wave's curves (with Swell); the design did not say, and
a swelling noise is a reverse cymbal.

### Kit (all pads)

| Knob | Label | Behaviour |
|---|---|---|
| 1 | SPACE | Room send, set per kit. Per-pad sends are a later choice. |
| 2 | SIZE | Room size. |
| 3 | GLUE | Kit compression, transient-aware, one knob. |
| 4 | WARM | Saturation of the whole kit. |
| 5 | SWING | Rejected for now: Move's own sequencer swings. Listed so it is not re-proposed. |
| 6 | | |
| 7 | | |
| 8 | VOL | Kit volume, in dB. |

Kits are Schwung presets: the whole kit is saved in `state`, and the factory
kits are presets. SOUND (Pad, knob 1) is the per-pad library.

---

## Modulation

- Each engine has **one modulator** with two destinations. That makes three per
  pad.
- The **kinds** are:
  - **Envelope:** a one-shot from each hit.
  - **LFO:** retriggered by each hit, free or synced to the tempo.
  - **Random:** a new value for each hit.
  - **Velocity.**
- **Destinations** are only that engine's own knobs, plus Level. This keeps
  every list under eight items.
- As in Quilt, modulation **never writes a knob**: the knob shows what was set.
- The Pad page's TUNE and DECAY are the shared "move everything" controls.

(Rejected: a mod matrix with any source to any destination. It is the opposite
of minimal. Three small modulators, each with its
own engine, reach most of the same sounds.)

---

## Sounds, presets and the randomiser

- **The SOUND library.** About 40 starting sounds, each a full pad:
  - 8 kicks, 8 snares and claps, 8 hats and cymbals, 8 toms and percussion,
    and 8 effects and tonal hits.
  - Each is levelled to the same loudness. Quilt used −26.5 LUFS for
    instruments; for drums, agree a target with the user first. A peak level
    for each hit may suit better than LUFS.
- **Factory kits.** Twelve or so, each with 16 pads drawn from the library and
  then varied.
- **DICE.** The randomiser rolls within a profile chosen from the pad's role.
  For example, a kick rolls Skin with a low PITCH, Wave sometimes, and Noise
  rarely and short. It always keeps the result in a playable range:
  - level-matched;
  - no silence;
  - no ten-second tails on a hat.

  It remembers the last eight rolls; turning left walks back through them.

---

## Implementation notes

- **CPU.** Sixteen pads × three engines, at most two voices a pad, so a fast
  roll's tail can overlap the next hit. Target: under a quarter of the Move's
  time with every pad sounding. Measure on the device at build step 3, not
  last.
- **Tables are built at load, not shipped.** The analogue and spectral
  wavetables, and the noise tables, are computed in `create_instance` (inverse
  FFT of a shaped spectrum with random phase). No data files. Watch the load
  time.
- **Samples.** User WAVs come from a folder, for example
  `/data/UserData/schwung/samples/strut/`. The file browser is Schwung's
  `filepath` param type. Loading happens off the audio thread, as Ragtag does.
  Playback is the straight sampler above: an interpolated read (cubic to
  start; Laakso et al.) at the rate PITCH, TUNE and BEND give, with no
  stretching. A pad's two voices let a long melodic sample ring under the
  next hit.
- **Separate outputs.** Schwung offers `move_plugin_render_split` (per-voice
  buffers; see `plugin_api_v1.h`). Later, not 1.0.
- **module.json carries the rack template.** A synth's pages come only from
  `get_param("ui_hierarchy")`, but the chain reads rack templates from
  module.json's `capabilities.ui_hierarchy` to type `p05_tune`. Keep the two
  in step; `tests/plan.test.mjs` checks them.
- **Pad presses** (`child_press_param`) only arrive while the grid shows Strut.
  That is fine, because focus only matters on the grid.

---

## Build order

1. ~~Scaffold: v2 entry, 16 pads, placeholder voice, focus-follow, tests, build,
   CI artifact.~~ (0.0.1)
2. ~~**Probe the contract:** the full key set with dummy DSP, measured
   (14.6 KB), planned in every view with the host's planner, and the
   template-vs-focused decision made (template, declared once).~~ (0.0.2)
   Still to try on the device: automation and a step lock on pad 5 land on
   pad 5, and MOD swaps its page at once.
3. **Skin** engine, and measure CPU on the device (the user installs the CI
   artifact).
4. **Wave** engine, its tables, and FM from Skin.
5. **Noise** engine and noise tables. Samples come at step 9.
6. **Pad** page mix, TUNE/DECAY/COLOR, and Finish's effects.
7. **Modulation** (the MOD views).
8. **Kit** page: room, glue, warmth.
9. Samples and `filepath`: Noise as a straight sampler (see *Noise*).
10. The SOUND library, factory kits, DICE, `help.json` and README.
11. Voicing pass with the user listening on the device.
12. Release to the catalog (needs the user's go-ahead).

## Testing

- `tests/run.sh`:
  - black-box through the v2 API (no silence where sound is due, no clipping,
    no NaN, parameter sweeps);
  - the host's own planner and validator on the contract, in all eight
    combinations of the MOD switches; every label through the host's fitter;
    the contract's size; module.json's template against the served one.
    The validator's one warning, that `pad` is on no level, is expected: the
    host's Selected Pad list is how it is reached.
- The CI builds a device tarball on every push.
- **Listening.** The user plays it on the Move.

## Later

- Knob pictures (Quilt's `canvas.js` approach).
- Per-pad room sends.
- Separate outputs.
- MPE / pad pressure as a modulation kind.
