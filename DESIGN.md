# Strut

*Sixteen drums, each one built from three engines and played from eight knobs.*

**Status:** all three engines sound, with each pad's finish, 0.3.1 (2026-10-07). Every proposed knob,
on every page and both views of each engine page, is declared, kept per pad
and planned by the host's own planner in the tests. **Skin**, the resonator,
**Wave**, the oscillator, and **Noise**, the noise source, are built and play
on every pad, mixed by SKIN, WAVE, NOISE, TUNE, DECAY and LEVEL; Skin's ring
can bend Wave (FM), and Wave or Noise can strike Skin (see *How Skin works*,
*How Wave works*, *How Noise works*); each pad then has Pad COLOR and the
Finish page (*How Finish works*). Noise's samples, the modulators, the kit's
effects, SOUND and DICE are still the plan; their knobs are kept but do
nothing yet. On the
Move, all three engines at their dearest take 11.1 % of the CPU (0.2.0).

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
  - Mipmapped wavetables, one table per octave, so a high note does not
    alias (used; see *How Wave works*).
  - V. Välimäki, J. Pekonen, J. Nam. *Perceptually informed synthesis of
    bandlimited classical waveforms using integrated polynomial
    interpolation.* JASA 131(1), 2012 (polyBLEP; considered, not used).
  - M. R. Schroeder. *Synthesis of Low-Peak-Factor Signals and Binary
    Sequences with Low Autocorrelation.* IEEE Trans. Information Theory
    16(1), 1970. The phases that keep a many-harmonic cycle from spiking.
  - G. E. Peterson, H. L. Barney. *Control Methods Used in a Study of the
    Vowels.* JASA 24(2), 1952. The Vowel table's formants.
- **The noise source** is the hiss, the metal and the sample: a loop that
  repeats without ever sounding pitched, or a recording, played straight as a
  sampler plays it or rebuilt by resynthesis.
  - R. J. McAulay, T. F. Quatieri. *Speech Analysis/Synthesis Based on a
    Sinusoidal Representation.* IEEE Trans. ASSP 34(4), 1986. A sound
    measured as a set of sine waves whose pitch and level drift over time,
    then rebuilt from them.
  - X. Serra, J. O. Smith. *Spectral Modeling Synthesis.* CMJ 14(4), 1990.
    The same, plus what is left over as shaped noise. Keep a sound's spectrum,
    throw away its phase, and it becomes noise of the same colour. This is how
    the built-in noise tables are made, and how a sample is resynthesised.
  - T. I. Laakso, V. Välimäki, M. Karjalainen, U. K. Laine. *Splitting the
    Unit Delay.* IEEE Signal Processing Magazine 13(1), 1996. Reading a sample
    between its stored points, so it can be played at any pitch (Noise reads
    its loops through six points, Lagrange's curve).
  - N. Guttman, B. Julesz. *Lower Limits of Auditory Periodicity Analysis.*
    JASA 35(4), 1963. A repeated stretch of noise is heard as repeating
    even when it is a second or more long: why Noise's loops are three
    seconds.
  - IEC 61672, the A weighting: the ear's sensitivity across frequency, by
    which the noise tables are set to the same loudness.
  - The 808 and 909 hi-hats: six square oscillators at inharmonic
    frequencies through band-pass filters (Werner, above).
- **Drum physics**, for the presets and the randomiser's sense of what a
  drum is:
  - T. D. Rossing. *Science of Percussion Instruments.* World Scientific, 2000.
  - J. D. Parker, V. Zavalishin, E. Le Bivic. *Reducing the Aliasing of
    Nonlinear Waveshaping Using Continuous-Time Convolution.* DAFx 2016.
    DRIVE's curve, smoothed by its area between samples.
  - J. Bilbao. *Numerical Sound Synthesis.* Wiley, 2009. Chapter 11
    (membranes and plates).
- **The output**, 16 bits with the rounding noise shaped to the ear:
  - E. Terhardt. *Calculating Virtual Pitch.* Hearing Research 1, 1979. Its
    formula for the threshold of hearing is the shaper's target.
  - M. A. Gerzon, P. G. Craven. *Optimal Noise Shaping and Dither of Digital
    Signals.* AES 87th Convention, 1989. A shaper can move noise around
    but not remove it: its mean log gain is at least zero.

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
| **Noise** | Noise | a loop of coloured noise, or your own sample, played straight or resynthesised | snares, hats, cymbals, claps, texture, and pitched hits cut from melodic samples |

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
page. Strut keeps that list short. You pick the pad the pages edit by
tapping it, or with the Kit page's **PAD** knob; there is no Selected Pad
page (see below):

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

**Each pad page's header reads "Skin 3", not "Skin".** On the device the
header shows the pad, not the page (`page_controller.mjs`, `pageLabel`): it
is the rack's `child_label` and the pad's number, and the level's name is
used only in the list of pages. With every rack labelled "Pad", every page
read "Pad 3" and the user could not tell where they were (2026-10-07). So each
rack's `child_label` is its page's name: Pad 3, Skin 3, Wave 3, Noise 3,
Finish 3. The tests check the titles, and that the widest, "Finish 16",
fits the header.

**No Selected Pad page.** The host plans a list of the sixteen pads before
the pad pages unless some page has a cell for the focus itself
(`page_plan.mjs`, `childPickerNeeded`). The user asked why it was needed
when tapping a pad picks it (2026-10-07), so the Kit page's last cell is
**PAD**, the focus, and the list is gone. Tapping is the usual way; PAD is
the way when the host's page-follow is off. The tests plan every view and
check there is no picker and PAD is Kit's last cell.

(Rejected: a real pop-up page. Schwung has no knob page that opens over another.
Its "doors" are canvas pages that hand the jog to the module, which would need a
JavaScript UI for every modulation page.)

---

## Control surface

Labels are real words of five letters or fewer. The header shows the full name.
"Bi" means bipolar, with the centre meaning no change.

Every key below is declared and kept (`src/dsp/params.c`). The ranges and
option lists are **provisional** until the voicing pass sets them by ear:
the SOUND list is stand-in names, and DICE is a plain number until its
turn-to-roll gesture (step 10).
The keys are the bare names in this table with an engine prefix where pages
share a word: `s_` Skin, `w_` Wave, `n_` Noise (`s_pitch`, `w_decay`).

### Pad (the focused pad)

| Knob | Key | Label | Behaviour |
|---|---|---|---|
| 1 | `pNN_sound` | SOUND | Picks a starting sound for this pad from the library (Kick, Snare, Hat…), replacing its engines. |
| 2 | `pNN_tune` | TUNE | Bi. Moves all three engines' pitch together, ±24 semitones. |
| 3 | `pNN_decay` | DECAY | Bi. Lengthens or shortens all three envelopes together. |
| 4 | `pNN_color` | COLOR | Bi. Darker to the left (low-pass), thinner to the right (high-pass), the whole pad (see *How Finish works*). |
| 5 | `pNN_skin` | SKIN | Skin's level. A fader: off fully left, then 30 dB of travel; 0.8 is −6 dB. So are WAVE, NOISE and LEVEL. |
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

#### How Skin works (built, 0.0.3)

- **PITCH** is semitones from A1 (55 Hz), from −12 to +60: 27.5 Hz to
  1.76 kHz, and the Pad page's TUNE moves it ±24 more.
- **HIT** and **SNAP** make the strike. SNAP sets its length, 0.2 ms to 50 ms.
  - **Click:** a sharp pulse that falls away; longer SNAP is a duller thud.
  - **Soft:** a smooth bump, a felt mallet; longer is softer.
  - **Burst:** a short burst of noise, falling away; the snare's hit.
  - **Wave** strikes with Wave's own sound (see *How Wave works*).
  - **Noise** strikes with Noise's sound (see *How Noise works*).
- **The ring.** The strike drives a phasor resonator (Mathews and Smith): one
  complex multiply a sample, ringing at PITCH and dying away by RING. The
  strike is scaled so the body always rings at the same level, whatever the
  strike, its length or the pitch: the strike's own spectrum at PITCH is
  worked out from its formula when the hit starts, and divided out. A soft
  strike still sounds softer, because its click and its upper partials are
  weaker. A burst rings at its expected level, so each hit is a little
  different, as on a real drum.
- **RING** is the time to fall 60 dB, 15 ms to 4 s, and Pad DECAY scales it by
  a quarter to four. It is never shorter than two and a half cycles of the
  pitch: a 55 Hz drum rung for 15 ms is a click with no pitch.
- **METAL** adds two partials above the body. At zero they sit at a
  drumhead's ratios over its fundamental (1.59 and 2.14, Rossing); turning it
  moves them up to a free metal bar's (2.76 and 5.40) and makes them louder
  and longer. A partial above the top of the audio range is left out.
- **MODE** filters the ring and the strike together, at the pitch: **Low** is
  a low-pass an octave above it (a round thump, partials softened), **Band** a
  band-pass on it (a pure ping), **High** a high-pass an octave below it (the
  click and the partials forward). A 12 dB trapezoidal state-variable filter
  (Zavalishin), as is **TONE**: a low-pass from 150 Hz to 18 kHz.
- **CURVE** (the Mod view) is Natural, the ring's own fall, until step 7.
- **Velocity** sets how hard the hit is, on a gentle curve (to the power
  1.5): it is in the strike, not a level on the output, so each hit's
  strength stays in the ring after it, and a harder hit bends Wave further
  through FM.
- **One voice a pad (0.1.0).** A new hit strikes the same drum again. What
  still rings goes on; the hit adds to it, so a roll builds and swells, and
  a hit landing against the ring partly stops it, as on a real drum. Wave
  restarts its note on each hit; the note it cuts fades out over 256 samples
  (6 ms), so it does not click. Tested: a hit in step with the ring builds
  it by more than 4 dB, one against it takes it down by more than 6; a soft
  hit keeps a loud ring; a restarted Wave note has no step. If the hit's
  scale changes (another SNAP or HIT), what rings is rescaled to keep its
  level. (Rejected: two voices a pad, each hit taking the other, the old one
  ringing under the new. It doubled the cost, 14.1 % of the Move's CPU for
  Skin and Wave at their dearest, and a struck drum is one drum. The user
  chose this over computing four voices at once, which would keep the
  sound and is still open if room runs short.)
- **Levels glide.** A voice's level moves across a block, not in one step, so
  turning SKIN or LEVEL while a drum rings does not crackle.

Rejected for the ring: the state-variable filter itself as the resonator. How
loud it rings for a given strike depends on its pitch and resonance, so every
PITCH and RING setting would need its own level correction; the phasor rings at
the size of the strike's spectrum, and stays well behaved when PITCH moves
mid-ring. Rejected for the strike's level: measuring each strike by running it
once before playing it. Sixteen pads hit together cost 315 µs that way on a
laptop, likely over a millisecond on the Move; the formulas cost almost
nothing.

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

#### How Wave works (built, 0.0.7)

- **PITCH** is semitones from A1 (55 Hz), −12 to +60, as Skin's, so the two
  start in tune with each other; the Pad page's TUNE moves both.
- **TABLE** and **WAVE.** A table is a row of single cycles, and WAVE sweeps
  along it, blending the two nearest. Every cycle is scaled to the same
  loudness, so a sweep neither swells nor dips (measured: every table and
  position within about 3 dB).
  - **Analog:** sine, triangle, saw, square, then the pulse narrows to 5 %.
    Square and pulse are a saw less a shifted saw, so they are as clean as
    the saw and stay at its loudness.
  - **Sync:** a saw restarted every cycle, running 1 to 8 times as fast: the
    tearing sweep of hard sync.
  - **Fold:** a sine folded back on itself more and more, as a wavefolder
    does.
  - **Sweep:** a saw through a resonant low-pass whose peak climbs from the
    first harmonic to the 32nd.
  - **Vowel:** a voice moving through u, o, a, e, i (the formants of
    Peterson and Barney, 1952, at 110 Hz).
  - **Hollow:** odd harmonics only, from a soft triangle-like tone to a
    bright, reedy one.
  - **Metal:** a weak fundamental under a band of prime-numbered harmonics
    that climbs: no simple ratios, so it clangs.
  - **Glass:** harmonics only at the squares, 1, 4, 9, 16…, the spacing of a
    vibrating bar's, growing brighter.
  - Recipe tables share one set of phases per table (Schroeder, 1970), which
    keeps a many-harmonic cycle from piling up into one tall spike, and
    morphing never cancels a harmonic. Sync and Fold are drawn in time and
    measured, so their phases are their own.
- **No aliasing.** Each cycle is kept at ten brightnesses an octave apart,
  512 harmonics down to one. A block reads the brightest whose top harmonic
  stays under 45 % of the sample rate at the highest pitch the block reaches,
  FM's swing and the ring's sidebands included, and fades over a block when
  a sweep crosses into another. Tested: a 7 kHz saw shows nothing at its
  folded-back frequencies to −70 dB.
- **DECAY** is the time to fall 60 dB, 15 ms to 4 s, scaled by Pad DECAY, and
  never under two and a half cycles, as Skin's RING. The start fades in over
  32 samples (0.7 ms), against a click.
- **BEND** starts the pitch up to four octaves away (finer near the centre:
  48 × b × |b| semitones) and brings it home with a twentieth of the note's
  length, about 25 ms on a half-second kick, a slow dive on a long zap. Right
  starts high, left starts low.
- **FM** is Skin's ring bending Wave's frequency: a swing of up to four
  times Wave's own frequency (the knob squared), through zero. Skin's ring is
  sized to about one, so the depth means the same on any Skin; it fades as
  Skin rings out, which is the growl that settles into a tone. Skin rings,
  and bends Wave, even with SKIN down.
- **RING** multiplies Wave by a sine from two octaves below Wave's pitch to
  two above; off at the centre, fading in over the first tenth of a turn.
  The sine starts at its peak, so a slow one does not mute the attack.
- **Wave as Skin's hit.** Skin's strike is Wave's sound, before Wave's own
  fall or level, for SNAP's length but at least one of Wave's cycles (a
  shorter slice of a low wave is nearly nothing). It is sized like any hit:
  Wave's harmonics near Skin's PITCH, with their phases, through the
  strike's shape, worked out when the hit starts. Tested over every table
  with three positions and three SNAPs: within 5.5 dB.
- **Skin and Wave run a sample at a time together**, and Wave follows Skin's
  ring one sample late, so each can feed the other without a loop.
- **Levels.** WAVE is a fader like SKIN. A Wave alone at the same fader
  peaks a little under a Skin; the voicing pass will match them.

Rejected for Wave:

- **Band-limited steps (polyBLEP, Välimäki et al.)** for the analogue
  shapes. Tables already cover them cleanly, the pulse is two saws, and one
  mechanism serves every table.
- **Mixing two brightnesses by the octave's fraction** on every sample. It
  read the tables twice as often, and to stay clean it had to keep the top
  harmonic under a quarter of the rate, losing an octave of brightness.
- **Phase modulation** for FM. Linear FM keeps the swing a fixed ratio of
  Wave's frequency, so the knob means the same at any pitch.

### Noise (the noise source or sample)

| Knob | Sound view | | Mod view | |
|---|---|---|---|---|
| 1 | PITCH | speed: ±48 semitones, with TUNE | KIND | |
| 2 | MODE | samples only: Sample, Resynth or Noise (below) | RATE | |
| 3 | DECAY | how long it falls | CURVE | |
| 4 | TABLE | noise tables, then your samples | AIM | |
| 5 | COLOR | bi: low-pass left, high-pass right | DEPTH | |
| 6 | START | sample start (samples only) | AIM | |
| 7 | LOOP | loop length; full = no loop (samples only) | DEPTH | |
| 8 | MOD | | MOD | |

Noise's destinations: Pitch, Color, Start, Loop, Level.

**Noise is also a sampler and a resynthesiser.** Past the noise tables, TABLE
lists your own samples, and MODE says how one is played:

- **Sample** plays it straight, as recorded. Speed and pitch move together, as
  on a classic sampler.
- **Resynth** plays it rebuilt. When the sample loads, Strut measures it as a
  set of drifting sine waves plus the noise left over (McAulay and Quatieri;
  Serra and Smith), and plays that back. It sounds close to the recording, but
  now pitch and length are separate: PITCH no longer speeds it up, and DECAY
  shortens or lengthens it without changing its pitch. LOOP over a short slice
  holds one moment of the sound still, as a drone.
- **Noise** keeps only the colour: the sample's spectrum as it changes over
  time, with every pitch taken out. A recorded cymbal becomes a cymbal-coloured
  hiss; a chord becomes a wash in its key's colours.

Melodic material becomes rhythm either way: put one chord or bass sample on
four pads, tune them apart and play them as a riff. With a sample chosen:

- **PITCH** is in semitones from the pitch it was recorded at (0 = as
  recorded). Pad TUNE moves it too, so a pad can be tuned to a note.
- **DECAY** fades the sample out (in Resynth it sets the length instead);
  turned fully right it plays to its end at its own length.
- **START** picks where in the sample to begin, so one long sample can feed
  several pads, each from its own slice.
- **LOOP** fully right plays the sample once. Lower, it repeats a slice of that
  length from START, for stutters and drones.
- **COLOR** filters it, as for noise.

On a noise table, MODE does nothing: the tables are already noise.

Noise has no BEND, unlike Wave: MODE took its cell. A pitch sweep at the start
(a tape-stop or a dive) is Noise's modulator, KIND Envelope aimed at Pitch.

#### How Noise works (built, 0.2.0)

- **TABLE** picks a loop of noise. Each is a colour drawn as a level at
  every frequency and given random phases, which is noise of exactly that
  colour (keep the spectrum, lose the phase, as Serra and Smith). Three
  are drawn in time instead, because their character is in their shape:
  - **White:** every frequency alike.
  - **Pink:** falling 3 dB an octave, as much in each octave; warmer.
  - **Brown:** falling 6 dB an octave from 60 Hz: rumble.
  - **Hiss:** white with its top lifted 12 dB from about 5 kHz: bright air.
  - **Wires:** a band from 1.5 to 10 kHz scattered with sixty narrow peaks:
    a snare's rattle.
  - **Metal:** six square waves at unrelated pitches, as an analogue drum
    machine makes its hats, with the shimmer above 5 kHz brought forward.
    The pitches are our own.
  - **Crackle:** small clicks, 3000 a second, as dust on a record.
  - **Grit:** random steps held 1 to 20 samples, on eight levels: a coarse,
    digital noise.
  Every table is set to the same loudness by the ear's weighting (A), then
  held under full scale. Metal and Crackle, whose peaks are tall for their
  level, come out about 5 dB quieter; the voicing pass will decide.
- **The loops are three seconds**, so a long tail does not audibly repeat
  (Guttman and Julesz), and each hit starts somewhere new in it, so no two
  hits are the same noise. A hit on a pad whose noise is still sounding adds
  to it as two noises do, by power, and reads on from where it is (so a
  roll never drops, and a soft hit on loud noise never quietens it).
- **PITCH** plays the loop faster or slower, a semitone a step, six octaves
  either way at most with TUNE: noise played up is brighter and thinner,
  down is darker. A sampler's pitch, not a filter.
  - **No folding back.** Each loop is kept at seven brightnesses an octave
    apart, as Wave's cycles are, and a note reads the brightest whose fold
    lands above 16 kHz, where it is only more hiss. So what is heard reaches
    14 to 20 kHz whatever the pitch, and a change of copy fades over a block.
  - **Read through six points** (Laakso et al.), from loops stored at twice
    the output's rate with nothing above 20 kHz, so slowed noise carries no
    hiss of the reading: its images are 40 dB down (tested: White an octave
    and two down crosses zero half and a quarter as often, to 5 %; at +7
    nothing shows above 18.5 kHz to −40 dB).
- **DECAY** is the time to fall 60 dB, 15 ms to 4 s, scaled by Pad DECAY.
  The noise starts at full strength: its first samples are its attack.
- **COLOR** is a two-pole filter: low-pass from 18 kHz down to 100 Hz to the
  left, high-pass from 30 Hz up to 11 kHz to the right, nothing at the
  centre, with a slight peak at the corner.
- **PITCH and COLOR keep the level.** What COLOR's filter takes from this
  table, and what a pitched-up copy leaves out, is worked out from the
  table's power in quarter-octave bands and made up, up to 60 dB. Turning
  either changes the colour, not the balance of the pad, and no setting
  falls silent (tested: every table at both ends of COLOR within 4 dB).
- **Noise as Skin's hit.** Skin's strike is Noise's sound through COLOR,
  before Noise's own fall or level, for SNAP's length. It is sized from
  Noise's colour around Skin's PITCH, gathered through the strike's own
  spectrum: a short strike hears a wide stretch of the colour, a long one
  only what is near the pitch, so Metal's lines drive a nearby pitch
  through a short hit and not a long one, as they would. Tested, the power
  of 48 hits on each table against Skin's own burst: within 2 dB.
- **MODE, START and LOOP** do nothing on a noise table; they are for
  samples (build step 9).

Rejected for Noise:

- **A cubic through four points, at the output's own rate.** Its images of
  a loop filled to near the top of the band were only 4 dB down: noise
  played an octave lower carried a second, hissy octave above it.
- **Copies exactly an octave apart**, nothing allowed to fold. Played a
  semitone up, a table lost everything from 10 to 20 kHz: Hiss without
  its hiss.
- **Loops as floats.** 16 MB where 16-bit numbers take 8, for no difference
  that can be heard (their floor is 78 dB under the most COLOR makes up).
- **A loop of a second and a half.** Half the memory and load, but a long
  tail, or a pitched-up one, repeats audibly.
- **Noise made live by a random number and filters.** It cannot give
  Metal's lines, Wires' peaks or a sample's colour, and costs a filter per
  colour per voice; a table gives any colour for one read.

#### The sample library (planned, build step 9)

Strut ships with 208 sounds in `src/samples/`, 50 MB, gathered in another
session (2026-10-07): 23 folders, one per kind (Kick, Snare, Rim, Clap, Hat,
Cymbal, Tom, Percussion, Bell, Mallet, Metal, Pluck, Wind, Bowed, Keys, Bass,
Cycle, Voice, Foley, Toy, Noise, Glitch, Ambient), each file named for its
kind and a number, `Kick 001.wav`.

- **All CC0 or public domain** (VCSL, The Open Source Drum Kit, Kenney's
  CC0 packs), or made here from them. `src/samples/SOURCES.md` records each
  file's origin, processing, loudness and peak. It stays with the library,
  and no sound is added without an entry there and a licence that allows
  commercial use with no attribution.
- **One-shots:** 44.1 kHz, 16-bit, mono or stereo, at most 4 s, silence
  trimmed, faded out, matched to about −18 LUFS (momentary maximum), peaks
  at −1 dBFS or lower. Strut needs no level matching of its own for them.
- **Cycle/** is different: 36 single cycles, each one period of exactly 2048
  samples, mono, looped. They are oscillators, not hits: always looped and
  pitched, never played once through. PITCH 0 is A1 (55 Hz), as Skin's and
  Wave's, so a cycle starts in tune with them; at its own rate a cycle is
  21.53 Hz. A cycle of 2048 samples holds up to 1024 harmonics, which fold
  back as soon as it is played higher, so each is built into copies an
  octave apart when loaded, as Wave's cycles are.
- **TABLE** lists the eight noise tables, then the library by folder, then
  your own samples (from a folder such as
  `/data/UserData/schwung/samples/strut/`). MODE (Sample, Resynth, Noise),
  START and LOOP work as above.
  - Open question for step 9: 216 options and more is a long turn of one
    knob. The alternatives are the host's file browser (`filepath`), or a
    folder and a number as two choices; measured against the contract's
    size and tried on the device.
- **Kept in memory on demand.** All 50 MB as 16-bit numbers is too much to
  hold. A pad loads the sample its TABLE names when TABLE changes, off the
  audio thread, and lets it go when nothing uses it; pads naming the same
  file share one copy. Sixteen pads of the longest stereo samples are at
  most 11 MB. Until a sample has loaded, its pad plays nothing (never
  stale or half-loaded data); a preset's samples load as it is chosen.
  How to load off the audio thread without a thread whose library call
  the Move's C library might not have (see *Tables are built at load*) is
  step 9's first question: Ragtag's way, or work the host already does on
  its own thread (`set_param`).
- **Shipping:** `scripts/build.sh` and `scripts/install.sh` copy
  `src/samples/` into the module (`dist/strut/samples/`, and the same on
  the Move), and the release tarball carries it. Measure the tarball and
  the install's time over SSH.
- **In the presets** (step 10): the SOUND list and the factory kits use the
  library, and DICE rolls from it by role, a kick pad from Kick/.

Rejected: time-stretching in Sample mode. It costs CPU on every pad and smears
the attack a drum needs; Resynth is the way to change length and pitch apart.
Rejected: a second TABLE entry per sample for each way of playing it. It
triples the list and hides that the three are one sample.

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

#### How Finish works (built, 0.3.0)

Each pad's three engines are mixed, then go through Pad COLOR and the
Finish page in this order: COLOR, DRIVE, CRUSH, LOW, HIGH, then PAN into
stereo. Each is skipped when at rest, so a pad that uses none costs only
its PAN.

- **Pad COLOR:** a two-pole filter on the whole pad. Low-pass from 20 kHz
  down to 200 Hz to the left, high-pass from 20 Hz up to 4 kHz to the
  right, nothing at the centre. Narrower than Noise's COLOR, so at its ends
  a pad is dark or thin, not gone; and it does not make up the level it
  takes, as Noise's does, because darker and quieter belong together on a
  whole drum.
- **PAN:** as loud anywhere in power, and exactly as before at the centre.
- **CHOKE:** Off, or group A to D. A hit fades every other pad in its
  group out over 5 ms (straight to nothing; quick enough to read as
  stopped, slow enough not to click), and cancels their FLAM hits still to
  come. A choked pad hit again starts afresh.
- **FLAM:** one hit becomes three, 2 to 50 ms apart (none at zero), rising
  to the hit played: a grace note at 55 %, a second at 75 %, then the hit.
  Each is a real hit, on its own sample, so a roll builds on Skin as a
  played one does. Claps, flams, ruffs.
- **DRIVE:** pushed up to 30 dB into a curve that rises straight through
  zero and levels off smoothly, then brought back down so a loud hit stays
  about as loud while its quiet parts come up. Its fold-back is smoothed by
  taking the curve's area between samples (Parker, Zavalishin and Le Bivic,
  2016), so a high whine is not folded down under a kick.
- **CRUSH:** from 16 bits down to 4, and from every sample kept to every
  16th held, together: grit, then ring, then a broken toy.
- **LOW** and **HIGH:** gentle shelves, below about 200 Hz and above about
  4 kHz, ±18 dB. The corner moves with the gain, so a cut is a lift turned
  upside down (with it fixed, a cut of 18 dB reached only 12 at 60 Hz). A
  lift of 18 dB on a loud kick is past full scale; the output's limiter
  rounds it.
- **DICE** waits for step 10.
- **CPU.** 0.3.0 ran each effect as its own loop over the block: on the
  Move, every effect on every pad took the worst case from 11.9 % to 18.7 %
  (2026-10-07). COLOR, LOW and HIGH are filters whose next sample waits on
  the last, so one after another the processor waited on each in turn.
  0.3.1 runs them all in one loop, a sample at a time, and works on them
  together: on a laptop, every effect from +48 to +16 µs a block. On the
  Move, still to measure.

Rejected for Finish:

- **tanh for DRIVE.** The area under it, needed to smooth its fold-back,
  is log cosh: an exponential and a logarithm a sample a pad, which cost a
  third as much as all three engines. The cubic's area is a few sums.
- **Plain DRIVE, no smoothing.** Pushed 30 dB, a plain curve folds the top
  of a bright sound back down as a whine.
- **FLAM at the block's edge.** Hits rounded to the 128-sample block would
  wander by up to 3 ms, which a flam of 5 ms cannot afford.

Noise's mod view uses Wave's curves (with Swell); the design did not say, and
a swelling noise is a reverse cymbal.

### Kit (all pads)

| Knob | Label | Behaviour |
|---|---|---|
| 1 | SPACE | Room send, set per kit. Per-pad sends are a later choice. |
| 2 | SIZE | Room size. |
| 3 | GLUE | Kit compression, transient-aware, one knob. |
| 4 | WARM | Saturation of the whole kit. |
| 5 | VOL | Kit volume, in dB. |
| 6 | PAD | The pad the other pages edit, 1 to 16 (header: Selected Pad). Tapping a pad picks it too. |

(Rejected: SWING. Move's own sequencer swings.)

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
  time with every pad sounding. `tools/bench.c` plays all sixteen pads with the
  longest rings and strikes; `scripts/bench.sh` builds it for the Move and
  runs it there over ssh, as Quilt's does.
  - On a laptop (2026-10-07), all 32 voices ringing: Skin alone **1.5 %** of
    a block; Skin and Wave at Wave's dearest (the pulse, FM, the ring, the
    longest fall, Wave striking Skin) **3.9 %**, 190 µs for a block in which
    all sixteen pads are hit. The Move's cores are several times slower.
  - **On the Move (2026-10-07, 0.0.7):** Skin **8.6 %**, Skin and Wave
    **13.8 %**, all 32 voices, measured by the user with the bench below (a
    first run read 6.1 %: the bench shares the Move with whatever it is
    doing). Over half the budget before Noise, the modulators and the
    effects, so 0.0.8 makes them cheaper without changing a sample's worth
    of sound:
    - **Fused multiply-adds.** `-std=c11` forbids the compiler to fuse a
      multiply and an add, which the Move's cores do in one step, and almost
      all of Strut is multiply-adds. The device build now says
      `-ffp-contract=fast`, and `-O3`. On a laptop that can fuse, about 30 %
      off Skin and Wave together.
    - **METAL at zero runs one resonator, not three.**
    - **One sample loop for each pairing** of Skin and Wave, so the loop
      decides nothing per sample.
    - **Wave's strike is sized by turning one angle** from harmonic to
      harmonic, not a sine and cosine each: 8 calls, not 400.
    - The bench now plays METAL up (Skin's dearest), runs five times and
      reports the middle and the spread, and names the processor and its
      clock.
  - **The Move's processor** is a quad Cortex-A72 at 1.5 GHz (`CPU part
    0xd08`, a Raspberry Pi compute module). Building the tables there takes
    350 ms at load.
  - **0.0.8 on the Move:** Skin with METAL up **7.6 %** (0.0.7 was 8.6 % with
    METAL down, a third of the work), Skin and Wave **14.1 %**: Wave itself
    no cheaper.
  - **0.0.9: the voice's state in registers.** Counting instructions
    (valgrind) and reading the ARM code (clang) showed every sample
    rereading and rewriting each voice's state in memory, two dozen loads a
    sample: the output pointer might have pointed into the voice, as far as
    the compiler knew, and a partial count decided at run time kept Skin's
    resonators in an array. Now each block runs on a copy of the voice in
    locals, with `restrict`, and one loop for each pairing of engines and
    each partial count, chosen when the block starts. Skin's ARM loop touches
    memory only for the output; Wave's, for its table reads. Measured on the
    Move: no change (7.9 % and 14.1 %); the Move's GCC had evidently kept
    them in registers already, and the cost is the arithmetic itself, about
    84 cycles a voice a sample. Kept: it costs nothing and is the shape any
    further saving builds on.
  - **0.1.0: one voice a pad** (see *How Skin works*), half the worst case:
    1.9 % on a laptop for Skin and Wave at their dearest, from 3.7 %. **On
    the Move: Skin 3.2 %, Skin and Wave 6.8 %**, all sixteen pads at their
    dearest (2026-10-07). About a quarter of the budget, leaving the rest
    for Noise, the modulators and the effects.
  - **0.2.0: Noise.** On the Move (2026-10-07): Skin 3.3 %, Skin and Wave
    7.8 %, **all three 11.1 %**, every pad at its dearest; the block in which
    all sixteen are hit at once 1.28 ms of its 2.9. Noise costs about what
    Wave does: its six-point read is most of it, the points loaded and made
    floats four at a time (noise.h). Its strike sizing, the onset's
    share, was halved in 0.2.1 (each band edge worked out once, in single
    precision).
  - The CI builds the bench for the Move
    (the `strut-bench` artifact, static, so it runs whatever the Move's C
    library); `scripts/bench.sh <that file>` runs it there.
  - Denormals: a laptop without them flushed ran 30 % slower once Wave
    struck Skin; the Move flushes them (`render_block`).
- **Tables are built at load, not shipped.** Wave's tables are computed once
  per process (the first `create_instance`, shared by every slot) by inverse
  FFT: 115 cycles at ten brightnesses, 2.4 MB, 56 ms on a laptop. No data
  files. Noise's eight three-second loops are built the same way, two to
  each transform (one in its real part, one in its imaginary): 8 MB, as
  16-bit numbers.
  - **0.2.0 took 0.8 s to load on the Move** (2026-10-07; once 2.8 s, with
    the Move busy), against 0.3 s for Wave's alone, though only 0.26 s on a
    laptop. A simulation of the
    Move's caches (32 KB and 1 MB, cachegrind) found the transform waiting
    on memory: 25 million misses. Its turns were read with a stride, a
    whole cache line for each, and every stage swept 4 MB.
  - **0.2.1:** each stage's turns side by side; the stages up to 16k points
    done a block at a time, in the cache; spectra written straight into the
    transform's bit-reversed order, so the long transforms skip their
    scattered reordering; single precision (its error is 120 dB down, under
    the 16 bits kept), with each row of butterflies a function of its own so
    GCC runs it four at a time. Misses down to 3.3 million, instructions
    from 1.18 to 0.64 billion; 65 ms on a laptop for every table, Wave's
    included. On the Move: 0.76 to 0.83 s (Wave's 55 ms of it, from 300).
    The bench's breakdown there: Noise's spectra 261 ms, its transforms
    142, against 22 and 15 on a laptop. The Move is slow where a laptop is
    not: at memory reached out of order, and at dividing in double
    precision.
  - **0.2.2:** long transforms in four steps (Bailey, 1990: short ones down
    a grid's columns and along its rows, a strip at a time), so no pass
    scatters across megabytes. On the Move, built both ways twice each:
    540 to 640 ms against 660 to 700 stage by stage; a laptop finds it the
    other way round. The spectra lost their divisions in double: the A
    weighting worked out once for all eight tables, random phases from a
    sine table, Wires' rattle peaks added near themselves only, and Crackle
    and Grit measured by one transform. On a laptop, Noise's spectra 23 ms
    to 10. **On the Move: 0.51 s to load** (2026-10-07; processor 0.42 s:
    Wave 57 ms, Noise's spectra 55, its transforms 258, storing 24). The
    transforms are most of what is left.
  - Rejected: building Noise's tables on a thread of their own after
    loading. A thread's library call can need a newer C library than the
    Move's, which this session cannot check, and noise would be silent for
    its first seconds.
- **The output** is Quilt's, with one change. VOL (the Kit page's, wired
  early) glides across the block; a soft limiter above half scale rounds off a
  stack of pads instead of clipping; then 16 bits, rounded with one step of
  triangular dither, and denormals flushed to zero on the Move.
  - **The dither stays at full depth while any voice sounds**, and fades out
    over a block once all have ended. The user heard quantization grit on the
    first Skin build (2026-10-07), which had no dither. Quilt's dither fades
    out as the sound falls below eight steps; measured on a tom's tail, that
    left the distortion at its harmonics 11 dB above the error's floor, against
    18 dB with no dither and 0.6 dB (none) with dither kept on. The price is
    about a quarter of a second more tail, at −96 dB.
  - **The host rounds again after us.** Each slot is scaled by its volume
    and the Move's master volume, each time rounded back to 16 bits with no
    dither (`schwung_shim.c`). Below full volume our dither shrinks under a
    step and is rounded away, so the last few steps of a quiet tail grit
    there whatever Strut does. Simulated: below full volume, Strut's dither
    makes no difference to it. Heard (2026-10-07): with the Move's volume all
    the way up, the grit was mostly gone, and the dither's hiss was heard
    instead.
    - Strut cannot reach this: the host takes only 16 bits from a module
      (`render_block`'s `int16_t`), mixes in 16 bits, and does not tell a
      module the master volume. The real fix is dither, or floating point, in
      the host's volume stage, a change to Schwung that the user has
      declined to request (2026-10-07). Below full volume it stays. (Rejected: louder dither in Strut to survive the volume stage. It
      fixed moderate volumes in the simulation, not low ones, and adds hiss at
      full volume.)
  - **The dither is noise shaped** (0.0.6). Its rounding errors are fed back
    through nine taps (`SHAPE`, from `tools/noise_shape.py`) that follow the
    threshold of hearing, so the hiss heard at full volume moves out of 1 to
    6 kHz, where the ear is keenest. Measured on a tom's tail: 9 to 12 dB
    less noise below 6 kHz, 11 dB more above 12 kHz; about 11 dB quieter to
    the ear by the threshold's weighting. Its memory is cleared once the kit
    is resting, so silence is still exact zeros. It works only where the host
    passes our samples unchanged (full volume, centre pan); below that the
    host's own rounding decides.
- **Reinstalling needs a restart.** The host opens a new synth before
  closing the old one, and `dlopen()` matches by path, so a slot reloaded
  with Strut gets the copy already in memory. The user tested 0.0.4 and heard
  0.0.3's grit and dead SKIN knob (2026-10-07). `scripts/install.sh` now restarts
  the Move itself (`reboot` as root, as the host's installer does), and Strut logs `strut <version> loaded` to
  `/data/UserData/schwung/debug.log` when it starts.
- **The level knobs are faders.** SKIN, WAVE, NOISE and LEVEL began as the
  square of the knob, moving half a percent a detent; the user turned SKIN
  and heard nothing change (2026-10-07). Now off at zero, then 30 dB across the
  turn, about 3 dB a tenth.
- **Samples.** User WAVs come from a folder, for example
  `/data/UserData/schwung/samples/strut/`. The file browser is Schwung's
  `filepath` param type. Loading happens off the audio thread, as Ragtag does.
  - **Sample:** an interpolated read (cubic to start; Laakso et al.) at the
    rate PITCH, TUNE and the modulator give.
  - **Resynth and Noise:** the analysis (short FFT frames, peak tracking into
    partials, the residual's spectral envelope) runs when the sample loads, on
    the loading thread, never on the audio thread. Playback is a bank of sine
    oscillators plus filtered noise. Cap the partials (start at 32 a voice)
    and measure CPU at step 9; this is the dearest thing a pad can do.
  - A pad's two voices let a long melodic sample ring under the next hit.
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
3. ~~**Skin** engine~~ (0.0.3; 0.0.4 fixed the output's grit and made the
   levels faders; 0.0.5 titles each page and logs its version; 0.0.6 shapes the dither's
   noise). Heard on the device (2026-10-07): SKIN works and the grit is gone
   at full Move volume. Its CPU is inside step 4's measurement.
4. ~~**Wave** engine, its tables, and FM from Skin~~ (0.0.7). CPU on the
   Move: 6.8 % with Skin, every pad at its dearest (0.1.0, one voice a
   pad; two voices took 14.1 %). Still to do: hear it on the device.
5. ~~**Noise** engine and noise tables~~ (0.2.0). Samples come at step 9.
   CPU on the Move: 11.1 % with all three engines on every pad at their
   dearest. Load took 0.8 s (once 2.8, the Move busy); 0.2.2 builds the
   tables for the Move's memory and loads in 0.51 s. Still to do: hear it
   on the device.
6. ~~**Pad** page mix, TUNE/DECAY/COLOR, and Finish's effects~~ (0.3.0;
   DICE waits for step 10). On the Move, every effect on every pad: 18.7 %
   (0.3.0); 0.3.1 runs the effects in one loop. Still to do: hear it, and
   measure 0.3.1.
7. **Modulation** (the MOD views).
8. **Kit** page: room, glue, warmth.
9. Samples (see *The sample library*):
   - ship `src/samples/` in the module and the tarball; measure both;
   - TABLE past the noise tables: the library, then your own samples;
   - load on demand per pad, off the audio thread;
   - Noise's three ways to play a sample (Sample, Resynth, Noise; see
     *Noise*), START and LOOP; Cycle/ looped and pitched, in copies an
     octave apart;
   - CPU measured with resynthesis on every pad;
   - tests: every file parses, every Cycle file is 2048 samples, the
     tarball holds the library.
10. The SOUND library, factory kits and DICE, drawing on the sample library
    (DICE by role), `help.json` and README.
11. Voicing pass with the user listening on the device.
12. Release to the catalog (needs the user's go-ahead).

## Testing

- `tests/run.sh`:
  - black-box through the v2 API (no silence where sound is due, no clipping,
    no NaN, parameter sweeps);
  - the host's own planner and validator on the contract, in all eight
    combinations of the MOD switches; every label through the host's fitter;
    the contract's size; module.json's template against the served one.
    No Selected Pad page is planned; PAD is Kit's last cell.
  - Skin: PITCH within 1 % by zero crossings, RING within 10 % by the fall
    between two windows, and every Skin knob (and TUNE and DECAY) at its ends
    and middle, every option of each list: it sounds, stays finite, peaks
    under 0.9 and dies away. Across all of them the loudest and quietest peaks
    are within 7.7 dB.
  - Wave: PITCH within 1 %, DECAY within 10 %, BEND starting high and
    settling on PITCH; every Wave knob at its ends and middle, and every table
    at five WAVE positions, sounds, stays finite, peaks under 0.9 and dies
    away (within 11.9 dB of each other); a 7 kHz saw has nothing at its
    folded-back frequencies to −70 dB; Skin struck by every table rings
    within 5.5 dB; FM moves Wave's energy off its pitch.
  - Noise: every table at the ends and middle of PITCH, COLOR and DECAY
    sounds, stays finite, peaks under 0.9 and dies away; PITCH an octave and
    two down halves and quarters White's zero crossings, and +12 raises
    Wires'; at +7 nothing folds back above 18.5 kHz to −40 dB; DECAY within
    10 %; COLOR darkens and thins White and keeps every table within 4 dB;
    two hits are not the same noise; Skin struck by every table rings, over
    48 hits, within 3 dB of its own burst.
  - One voice a pad: a hit in step with the ring builds it, one against it
    stops it, a soft hit keeps a loud ring (and loud noise), a restarted
    Wave note has no step.
  - Finish: every Finish knob and Pad COLOR at its ends and middle, on a
    pad of all three engines, stays finite, sounds and dies away; PAN hard
    left or right is that side alone, at the centre's power to 0.1 dB;
    CHOKE silences a pad in its group, and leaves one in another ringing;
    FLAM gives three rising onsets its gap apart; DRIVE grows a sine's
    third harmonic and keeps a loud hit within 6 dB; CRUSH holds samples;
    LOW and HIGH lift and cut their ends of White noise by over 12 dB; Pad
    COLOR darkens and thins.
  - The faders (SKIN, WAVE, NOISE, LEVEL): off at zero, 2 to 4 dB each tenth of a turn. A quiet tail
    through the real 16-bit output stays within 1.5 steps of the exact
    signal and ends in true silence.
- `tools/demo.c` renders a few hand-set sounds and a groove to a WAV, for
  listening away from the Move (`build/tests/demo out.wav`).
- The CI builds a device tarball on every push.
- **Listening.** The user plays it on the Move.

## Later

- Knob pictures (Quilt's `canvas.js` approach).
- Per-pad room sends.
- Separate outputs.
- MPE / pad pressure as a modulation kind.
