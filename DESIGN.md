# Strut

*Sixteen drums, each one built from three engines and played from eight knobs.*

**Status:** all three engines sound, with each pad's finish, each engine's modulator, the Kit page, the sample library in all three of Noise's modes, and DICE, 0.8.0; kits save and load through `state`, 0.8.1; LEVEL on Finish, SPACE per pad, CHOKE on the Kit page, 0.9.0 (2026-10-07); Pad > SOUND picks from 40 fixed sounds and Kit > KIT holds the factory kits (placeholders until the engines are done), 0.10.0 (2026-10-09); every knob draws a picture of what it does, 0.11.0 (2026-10-09); claps and rims rebuilt, CURVE's Clap, 0.11.1 (2026-10-09); 6 dB louder, harder DRIVE, LEVEL after the finish, seven kick styles, 0.12.0 (2026-10-09); DRIVE turns without crackle, 0.12.1 (2026-10-09); recorded claps snappier, rims without the thump, 0.12.2 (2026-10-10). Every proposed knob,
on every page and both views of each engine page, is declared, kept per pad
and planned by the host's own planner in the tests. **Skin**, the resonator,
**Wave**, the oscillator, and **Noise**, the noise source, are built and play
on every pad, mixed by SKIN, WAVE, NOISE, TUNE, DECAY and LEVEL; Skin's ring
can bend Wave (FM), and Wave or Noise can strike Skin (see *How Skin works*,
*How Wave works*, *How Noise works*); each pad then has Pad COLOR and the
Finish page (*How Finish works*), every engine its modulator and CURVE
(*Modulation*), and the whole kit GLUE, WARM and a room (*How the Kit page
works*). Noise plays the library's 208 samples, or your own, as recorded,
resynthesised or as their colour alone (*The sample library*, *How
Resynth and Noise work*). DICE rolls a pad, or the whole kit, by each
pad's place in it, with eight steps back (*How DICE works*, 0.8.0);
SOUND and KIT pick fixed sounds and kits, each undone by DICE Back
(*SOUND and KIT*, 0.10.0). On the Move, every pad at its dearest, with
every effect and modulator and the Kit page: about 22 % of the CPU (0.6.0);
with Resynth on every pad, 20.5 %, its runs reaching 22.3 % (0.7.0).

- **Module ID:** `strut`
- **Component type:** `sound_generator`, plugin API v2, pure C, no JavaScript UI
  but the knob pictures (`src/canvas.js`, see *Knob pictures*).
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

What it measures: 58 knobs a pad, 938 addressable keys, 68 declared. The whole
contract is **17.6 KB** (hierarchy 5.4 KB, chain_params 12.2 KB, 0.8.0) against a
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
- Hidden knobs close up, and `MOD` is listed first, so it is cell 1 either
  way. The Mod view reads MOD, KIND, RATE, CURVE over AIM, DEPTH, AIM,
  DEPTH, so each destination sits beside its depth on the lower row; the
  Sound view, MOD and its first three over its other four. (The user's
  layout, 2026-10-07. MOD was cell 8 first, then cell 4 of the Mod view
  only, before settling here.)
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
page has eight cells with MOD in cell 1, and the Mod view's lower row AIM,
DEPTH, AIM, DEPTH.

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

**The header's pad map follows every pad page.** The host lights the pad
the page on screen edits, finding it by note (`padIconNote`), so every pad
page names its pads' notes (`child_note_base`), not the Pad page alone. With
only the Pad page naming them, the map drew an empty box on the other four
(seen on the device, 2026-10-07; fixed in 0.4.0). The tests check every pad
page gives the map pads 1 to 16.

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
the SOUND list is stand-in names.
The keys are the bare names in this table with an engine prefix where pages
share a word: `s_` Skin, `w_` Wave, `n_` Noise (`s_pitch`, `w_decay`).

### Pad (the focused pad)

| Knob | Key | Label | Behaviour |
|---|---|---|---|
| 1 | `pNN_sound` | SOUND | Own (the pad as it is), then 40 starting sounds (Kick 1, Snare 2, Hat 3…), each replacing the pad's engines and keeping its place in the mix. |
| 2 | `pNN_tune` | TUNE | Bi. Moves all three engines' pitch together, ±24 semitones. |
| 3 | `pNN_decay` | DECAY | Bi. Lengthens or shortens all three envelopes together. |
| 4 | `pNN_color` | COLOR | Bi. Darker to the left (low-pass), thinner to the right (high-pass), the whole pad (see *How Finish works*). |
| 5 | `pNN_skin` | SKIN | Skin's level. A fader: off fully left, then 30 dB of travel; 0.8 is −6 dB. So are WAVE, NOISE and Finish > LEVEL. |
| 6 | `pNN_wave` | WAVE | Wave's level. |
| 7 | `pNN_noise` | NOISE | Noise's level. |
| 8 | `pNN_space` | SPACE | This pad's send to the room, after its Finish. 0 by default: dry. |

### Skin (the resonator)

| Knob | Sound view | | Mod view | |
|---|---|---|---|---|
| 1 | MOD | Sound / Mod | MOD | Sound / Mod |
| 2 | PITCH | resonant frequency | KIND | Envelope, LFO, Random or Velocity |
| 3 | RING | resonance: how long the body rings | RATE | speed; synced right of centre, free left |
| 4 | HIT | the exciter: Click, Soft, Burst (Skin's own noise burst), Wave or Noise (the other two engines) | CURVE | Skin's envelope shape: Natural, Ping, Soft, Hold |
| 5 | SNAP | how long the exciter lasts | AIM | destination 1 (below) |
| 6 | METAL | two extra inharmonic partials, rising and louder | DEPTH | Bi |
| 7 | TONE | brightness, a 2-pole low-pass | AIM | destination 2 |
| 8 | MODE | Low, Band or High-pass resonator | DEPTH | Bi |

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
- **CURVE** (the Mod view) shapes the ring's fall (see *Modulation*).
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
| 1 | MOD | | MOD | |
| 2 | PITCH | | KIND | |
| 3 | BEND | pitch envelope depth, bi | RATE | |
| 4 | DECAY | | CURVE | Natural, Ping, Soft, Hold, Swell (inverted), Clap |
| 5 | TABLE | Analog, then the spectral tables | AIM | |
| 6 | WAVE | position in the table; at the end of Analog, pulse width | DEPTH | |
| 7 | FM | Skin into Wave's frequency | AIM | |
| 8 | RING | ring-modulation; bi: −2 to +2 octaves | DEPTH | |

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
| 1 | MOD | | MOD | |
| 2 | PITCH | speed: ±48 semitones, with TUNE | KIND | |
| 3 | MODE | samples only: Sample, Resynth or Noise (below) | RATE | |
| 4 | DECAY | how long it falls | CURVE | |
| 5 | TABLE | noise tables, then your samples | AIM | |
| 6 | COLOR | bi: low-pass left, high-pass right | DEPTH | |
| 7 | START | sample start (samples only) | AIM | |
| 8 | LOOP | loop length; full = no loop (samples only) | DEPTH | |

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
- **DECAY** fades the sample out; turned fully right it plays to its end
  at its own length. In Resynth it sets the length instead: the sample's
  own at the centre, a quarter of it fully left, four times fully right.
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
  samples (*The sample library*).

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

#### The sample library (built, 0.6.0)

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
- **TABLE is one long list** (the user's choice, 2026-10-07): the eight
  noise tables, then the library by folder, drums first (Kick, Snare, Rim,
  Clap, Hat, Cymbal, Tom, Percussion, then the rest), then your own
  samples, every `.wav` in `/data/UserData/schwung/samples/strut/`, by name.
  216 options with the library alone; the host's full-screen list shows
  them as the knob turns. The list is read once, when Strut first loads, so
  a sample added to your folder appears the next time. A preset keeps a
  sample by its name, so it survives the list changing around it. The
  library's names added 2.5 KB to the contract (17.3 KB).
  (Rejected: the host's file browser, and a folder and a number as two
  knobs; the user chose one list.)
- **How a sample is kept.** Read as mono (stereo's sides averaged), then
  stored as a noise table is: at twice the output's rate (a half-band
  filter, 70 dB down past 24 kHz), as 16-bit numbers, and in seven copies
  an octave apart, each low-passed to its own top first (70 dB down from
  0.15 of its rate), with its power in quarter octaves. So a sample is read
  through Noise's own six points, plays high without folding back, and
  PITCH, COLOR, their level match and Skin's strike treat it as they treat
  noise. COLOR's level match makes up at most 12 dB on a sample (60 on a
  table): there COLOR is a filter, not a colour. A 4 s sample takes 1.3 MB
  and, on a laptop, 40 ms to prepare (14 ms on average across the library).
- **A cycle** (Cycle/, 2048 samples) is kept by its harmonics, each copy
  holding only those under its top, as Wave's are, set to an RMS of 0.2,
  looped and pitched: PITCH 0 is A1, 55 Hz (tested). START sets where in the
  cycle it begins, and a re-hit runs on, as an oscillator does.
- **Playing a one-shot.** From START; to its end, or with LOOP lower,
  round a slice of it, 2 ms long to all that is left, crossing back over
  the slice's last 4 ms (or half the slice, or as much as lies before it)
  so a loop does not click. With DECAY fully right it plays unfaded to its
  end; lower, it falls as noise does. A re-hit restarts it, the note it
  cut fading out over 256 samples. Pitch moves its speed with TUNE, as on a
  sampler (tested: +12 halves its length, START half way plays its second
  half, a loop repeats one slice apart).
- **Level.** The library is matched to −18 LUFS. Played back twice as loud
  (+6 dB), half of it is within 1 dB of White noise's loudness or louder
  (its loudest 400 ms against White's, −9.5 to +9.4 dB across all of it),
  peaking at 0.73 at the NOISE level's default.
- **Kept in memory on demand.** All 50 MB is too much to hold. Every host
  call, `set_param` included, runs on the audio thread (`plugin_api_v1.h`),
  so each instance has a loader thread of its own, as Ragtag does. Its
  first act is to drop to ordinary priority and off core 3, the host's rule
  for module threads. Schwung itself is built on Debian bookworm and starts
  threads, so the thread call needs nothing newer than the Move has (the
  worry under *Tables are built at load*). The loader:
  - loads what a pad's TABLE names once the knob has rested on it 60 ms, so
    turning through the list loads nothing on the way;
  - shares one copy between pads naming the same file;
  - hands it over by one pointer a pad; until it has, the pad's Noise
    plays nothing (never a stale or half-loaded sound);
  - keeps samples nobody uses while it holds under 16 MB, so going back is
    instant, and frees one only once two blocks have passed since a pad
    last had it and no voice is playing it, so the audio thread never
    reads a freed sample (tested, with 120 samples in turn).
- **Shipping:** `scripts/build.sh` copies `src/samples/` into the module;
  the tarball is 40 MB, and CI checks it holds every sample and SOURCES.md.
  `scripts/install.sh` sends the library only when it differs from the
  Move's (a stamp of its files' contents), into a folder beside it, then
  swaps it in. **On the Move** (2026-10-07): the library's 50 MB copied in
  2 s; a 4 s sample prepared in 181 ms, so a sample plays about a quarter
  second after TABLE comes to rest; samples looped on every pad cost no
  more than noise tables (19.2 %, against 18.9 % with tables in the same
  run).
- **In the presets** (step 10): the SOUND list and the factory kits use the
  library, and DICE rolls from it by role, a kick pad from Kick/.

Rejected: time-stretching in Sample mode. It costs CPU on every pad and smears
the attack a drum needs; Resynth is the way to change length and pitch apart.
Rejected: a second TABLE entry per sample for each way of playing it. It
triples the list and hides that the three are one sample.

#### How Resynth and Noise work (built, 0.7.0)

MODE's other two ways are made from the sample the first time a pad asks
for them, on the loader's thread, and kept with it. Until they are made, a
hit plays the sample as recorded; a note keeps the MODE it was struck
with. A cycle (Cycle/) is already an oscillator, with pitch and length
apart, so it plays as itself in every MODE.

- **Resynth's analysis** (McAulay and Quatieri; Serra and Smith). The
  sample, at twice the output's rate, is cut into frames 5.8 ms apart, each
  seen through a 46 ms window (Hann), wide enough to part two pitches 43 Hz
  apart. Each frame's peaks are placed between bins by a parabola through
  their log levels, and joined into at most 32 tracks: each track takes the
  nearest peak within 3 %, the loudest tracks first, and the peaks left
  over start new tracks in free slots, the loudest first. A slot rests a
  frame after its track ends, so the old track fades out at its own pitch
  rather than gliding to the new one.
  - **A long window rises early.** Seen through 46 ms, a sine that starts
    with a hit is already heard 23 ms before it. Each frame's sines are
    scaled by the level the window's middle half heard against the level
    the whole heard (at most 2, which a sine starting at the middle needs).
    Measured against the middle 5.8 ms alone, a bass's swing wobbled the
    scale and a kick's body came out 4 dB over.
- **The noise left over.** The sines are played through once at the
  sample's own pitch and length, and each 5.8 ms of the sample has their
  power taken out of its spectrum, bin by bin. What is left is given random
  phases and laid end to end (Serra and Smith's stochastic part), as a
  second table of the same length, with the same seven copies.
  (Rejected: subtracting the sines in time. Their phases are guessed, so
  the difference added as much as it took away.)
- **Playing Resynth.** Each sine is a pointer turned a step a sample, its
  step set each block from the frame reached, PITCH and TUNE; its level
  glides across the block to the frame's. Four sines share a vector, and
  four vectors turn side by side (one pointer's turn waits on its last, so
  one at a time left the processor idle). Sines over 19.8 kHz fall silent.
  The leftover noise is read in two grains 23 ms long, half a grain apart,
  each through a sine window so the two keep the power; each grain starts
  where the frames have got to, so the noise follows DECAY's length while
  PITCH sets how fast it is read (a drum tuned up is brighter, as on Skin).
  The grains are read along a straight line between samples, not six
  points: at twice the rate, its error is 50 dB under the noise at 5 kHz
  and 23 dB at 20 kHz, and is only more noise.
- **The hit itself is the sample.** Up to 10 ms past where the sample's
  hit begins (its first sound within 20 dB of its peak), Resynth plays the
  recording, then crosses over to the resynthesis by power across 12 ms.
  Without it, the sines' guessed phases summed at the attack peaked up to
  6 dB over the recording (a kick 1.06 against 0.73), and a sound starting
  late (Foley 013, 55 ms in) rose before its hit.
- **DECAY is the length** in Resynth, not a fade: the frames advance at a
  quarter to four times the sample's own speed, its own at the centre, and
  Pad DECAY scales it as it scales every fall. CURVE's Hold stops time:
  the sound holds where it is. LOOP repeats a slice of the frames from
  START; a slice shorter than a frame holds one moment still, as a drone.
  A re-hit carries on from where the sines are, gliding to the new frame,
  and the note it cut fades over 64 samples under the new attack.
- **Noise mode** is made the same way as Resynth's leftover noise, but
  from the whole sample, with each bin's power spread over two either side
  first, so a note becomes a band of noise around it and not a pitch
  (tested: a mallet that repeats itself at 0.99 does so at 0.25). It plays
  exactly as Sample mode does (START, LOOP, DECAY, PITCH as speed), at the
  same cost. It keeps nothing under about 170 Hz, where 5.8 ms is too short
  to hold a colour, so it is set to the sample's power, not to what it
  kept: kicks would otherwise lose 5 dB.
- **Level.** Across every one-shot in the library, against the sample
  itself (loudest 400 ms): Resynth −3.6 to +1.7 dB, half of them over
  +0.3; Noise −0.6 to +1.1 dB. Every one sounds, peaks under 0.95 and ends.
- **Cost.** Resynth adds up to 1.5 MB to a sample (2.8 MB in all). **On
  the Move** (2026-10-07): a 4 s sample's Resynth takes 496 ms to make and
  its Noise 190 (after 160 ms to load it), so the first switch of a long
  sample to Resynth waits about 0.7 s, playing the sample meanwhile.
  Resynth looped on every pad, with every effect and modulator: 20.5 %
  (runs 20.4 to 22.3 %), against 17.0 % for samples looped in the same
  run. So Resynth costs about 3.5 % more and stays under the 25 %
  allowed. (On a laptop the two cost the same; the Move's slower memory
  and narrower vectors show the sines.)

Rejected for Resynth:

- **Every sine's phase followed exactly** (McAulay and Quatieri's cubic
  phase). It would rebuild the attack too, but costs a cubic a sine a
  sample; the recording plays the attack instead, and after it no ear
  hears a phase.
- **Making all three ways when a sample loads.** Resynth more than
  quintuples a sample's preparation; most pads never ask for it.

### Finish (the focused pad)

| Knob | Label | Behaviour |
|---|---|---|
| 1 | LEVEL | The pad's level, a fader like SKIN. |
| 2 | PAN | Bi. |
| 3 | FLAM | One hit becomes three; how far apart (claps). |
| 4 | DRIVE | Saturation. |
| 5 | CRUSH | Fewer bits and a lower rate. |
| 6 | LOW | Bi. Low shelf, ±18 dB. |
| 7 | HIGH | Bi. High shelf, ±18 dB. |
| 8 | DICE | Turn right to roll a new sound for this pad; turn left to step back through the last eight. |

#### How Finish works (built, 0.3.0)

Each pad's three engines are mixed, then go through Pad COLOR and the
Finish page in this order: COLOR, DRIVE, CRUSH, LOW, HIGH, then LEVEL and
PAN into stereo. LEVEL comes last (0.12.0): the engines mix at its default,
so a pad DRIVE has pushed to its ceiling still turns down. Rejected: LEVEL
before the finish, as to 0.11.1; once DRIVE reached its ceiling, LEVEL
hardly moved it. Each is skipped when at rest, so a pad that uses none costs only
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
- **DRIVE:** pushed up to 40 dB into a curve that rises straight through
  zero and levels off smoothly, then brought back down so a loud hit stays
  about as loud while its quiet parts come up. Its ceiling rises 2.5 dB as
  it turns (0.12.0), to about 2 dB under full scale after the output's
  make-up: fully round, a kick is a loud square. Once a pad reaches the
  ceiling, its engines' faders no longer change its level; LEVEL does.
  Rejected: the level kept all the way with no make-up, as to 0.11.1; every
  driven pad peaked under -10.7 dB, and the user heard the kicks as too
  soft and not gnarly enough (2026-10-09). Its fold-back is smoothed by
  taking the curve's area between samples (Parker, Zavalishin and Le Bivic,
  2016), so a high whine is not folded down under a kick. A turn glides
  through each block a sample at a time, and the curve fades in over the
  knob's first 5% (0.12.1). Rejected: stepping once a block, as to 0.12.0;
  turned while a pad rang, it crackled (the user, 2026-10-09), since each
  step jumped a quiet tail and switching the curve in or out jumped by its
  bend and its half-sample delay.
- **CRUSH:** from 16 bits down to 4, and from every sample kept to every
  16th held, together: grit, then ring, then a broken toy.
- **LOW** and **HIGH:** gentle shelves, below about 200 Hz and above about
  4 kHz, ±18 dB. The corner moves with the gain, so a cut is a lift turned
  upside down (with it fixed, a cut of 18 dB reached only 12 at 60 Hz). A
  lift of 18 dB on a loud kick is past full scale; the output's limiter
  rounds it.
- **DICE:** see *How DICE works*.
- **CPU.** 0.3.0 ran each effect as its own loop over the block: on the
  Move, every effect on every pad took the worst case from 11.9 % to 18.7 %
  (2026-10-07). COLOR, LOW and HIGH are filters whose next sample waits on
  the last, so one after another the processor waited on each in turn.
  0.3.1 runs them all in one loop, a sample at a time, and works on them
  together: on a laptop, every effect from +48 to +16 µs a block. **On the
  Move, every effect on every pad: 15.3 %, against 12.6 % without** (0.3.1,
  2026-10-07): the effects at their dearest cost 2.7 %, from 6.8.

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
| 1 | SIZE | The room's size: rings under a second to about four, darker and later as it grows. How much each pad sends is its own Pad > SPACE. |
| 2 | GLUE | Kit compression, one knob: soft hits nearer loud ones, attacks kept. |
| 3 | WARM | Saturation of the whole kit: thicker, rounder, a softer top. |
| 4 | VOL | Kit volume, in dB. |
| 5 | CHOKE | The focused pad's choke group: Off, or A to D; a hit stops the others in its group (open/closed hats). |
| 6 | KIT | Own (the kit as it is), then the factory kits. The list follows the knob; the kit loads where it stops (header: Factory Kit). |
| 7 | DICE | Turn right to roll a new kit, every pad by its place; turn left to step back through the last eight (header: Kit Dice). |
| 8 | PAD | The pad the other pages edit, 1 to 16 (header: Selected Pad). Tapping a pad picks it too. |

(Rejected: SWING. Move's own sequencer swings.)

Kits are to be Schwung presets: the whole kit saved in `state` (built, 0.8.1: a flat object of the set_param keys that differ from their defaults, enums by name, so a sample comes back by name; DICE and the MOD views are left out, so loading never rolls), and the factory
kits are presets. SOUND (Pad, knob 1) is the per-pad library.

### How the Kit page works (built, 0.5.0)

The sixteen pads are mixed, then GLUE, then WARM, then the room is added;
VOL, the limiter and the dither follow (*Implementation notes*). Each is
skipped while its knob is at zero, and the room once its tail has died, so
with all three at zero the kit is exactly the pads (the tests check it).
Every knob glides across a block.

- **Layout from 0.9.0** (the user's ask, 2026-10-07; the tables above
  show it, with KIT from 0.10.0):
  - Pad: SOUND TUNE DECAY COLOR SKIN WAVE NOISE SPACE. SPACE is now this
    pad's own send to the room, taken after its Finish, so a dry kick can sit
    beside a roomy snare. The room no longer hears GLUE and WARM.
  - Finish: LEVEL PAN FLAM DRIVE CRUSH LOW HIGH DICE (LEVEL before PAN).
  - Kit: SIZE GLUE WARM VOL CHOKE KIT DICE PAD (KIT from 0.10.0). Kit > CHOKE sets the focused
    pad's choke group (`kit_choke`, served from that pad); Finish was full.
    Rejected: DICE off Finish to make room (the user chose to move CHOKE).
- **SPACE** (Pad > SPACE, each pad's own, from 0.9.0) sends that pad,
  after its Finish, to the room, high-passed at 150 Hz so a kick does not
  boom in it; full up on every pad, the room is about 6 dB under a beat's
  dry sound. The default is 0, a dry kit (the user's choice, 2026-10-07; it was 0.15, a touch of air about 22 dB under).
- **The room** is Dattorro's plate (JAES 1997), Quilt's: four diffusing
  all-passes into two cross-fed loops, each a slowly wandering all-pass, a
  delay, damping and another all-pass and delay, tapped at seven points a
  side for a wide stereo.
- **SIZE** sets how long it rings, falling 60 dB in under a second to about
  four, evenly by ear (measured from a click: 30 dB in 0.43, 0.89 and
  2.08 s at SIZE 0, ½ and 1); a little darker as it grows (damping 9 kHz to
  4.5); and its first reflection later, 2 to 32 ms.
- **GLUE** is a compressor for the whole kit. Further round, the threshold
  falls from −8 to −24 dB and the ratio rises from 1 to 4, over a soft
  knee. It is slow to grab, 10 ms, so each hit's attack passes. Its level
  is the larger of two followers: a fast one that lets go in 100 ms, after
  a single hit, and a slow one that rises over 400 ms and lets go over
  1.5 s, which holds the squeeze steady through a busy groove instead of
  pumping with every hit. Made up by half what it takes at −15 dB. On a
  beat of loud and soft hits, full up: the soft hits come 3.9 dB nearer
  the loud, the level moves −0.8 dB and the peaks +1.7.
- **WARM** pushes the kit up to 12 dB into DRIVE's soft curve, off centre,
  so it adds even harmonics (which thicken) as well as odd ones (which
  bite), and rolls the top from 20 kHz down to 8 kHz. Smoothed as DRIVE is
  (Parker et al., 2016); the offset's own level is taken back out, with a
  high-pass at 10 Hz for any drift. Brought back so a quiet sound is about
  2 dB up and a loud one a little down: on a 110 Hz sine at 0.3, full up,
  the second and third harmonics at −20 and −16 dB, the level −1.5 dB.
- **The dither** stays on while the room rings, not only while a pad
  sounds, so a tail fades with its dither as a pad's does. The room is let
  go once its loops fall under about a twentieth of a 16-bit step, and
  listens on for half a second after its last input, the time a sound takes
  to come round its loops the first time. (0.5.0's first try let go after a
  tenth of a second, and the room fell silent before it rang.)

Rejected for the Kit page:

- **GLUE after the room.** Squeezing the room with the kit ducks its tail on
  every hit and lifts it between them: a pumping effect, not glue. The room
  hears the glued, warmed kit instead.
- **Quilt's SIZE as it was** (decay 0.2 to 0.93): ten seconds at the top,
  too long for drums.
- **SPACE by its square.** A send squared put the default 39 dB under the
  kit, inaudible.
- **Make-up by what GLUE takes at −6 dB.** A beat's peaks sit near −6 dB
  but its body near −24; made up for the peaks, the beat came out 2.6 dB
  louder, its peaks 5 dB.

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

### How the modulators work (built, 0.4.0)

- **Each hit restarts them.** Envelope falls from 1 to nothing over RATE's
  time (exponentially, 60 dB); LFO is a sine starting from zero, a cycle in
  RATE's time; Random is a new value from −1 to 1 each hit, held; Velocity
  is the hit's own, 0 to 1.
- **RATE** is a time. Left of centre it is free: 5 ms at the centre to 4 s
  fully left. Right of centre it is a note value at the tempo the Move
  reports (`get_bpm`; 120 if it does not say), from a 64th through
  triplets to four bars fully right.
- **DEPTH** is bipolar. On a pitch it is up to four octaves either way,
  finer near the centre (48 × d × |d| semitones); on Level a factor from
  silent (−1, an envelope then fades the engine in) to twice (+1, a punch);
  on any other knob its whole range. Both AIMs may name the same knob, and
  add.
- **The modulators never write a knob.** Each stretch of 64 samples, the
  pad's knobs are copied and the copy is moved, so a knob shows what was
  set and automation records it. The hit itself is sized with the copy, so
  velocity on SNAP shapes the strike.
- **CURVE** is the shape of the engine's own fall, over its RING (Skin) or
  DECAY (Wave, Noise), as moved:
  - **Natural:** the fall as it is.
  - **Ping:** twice as fast.
  - **Soft:** rises over a tenth of it, 2 to 80 ms.
  - **Hold:** stays full for half of it, its own fall paused (a ring kept
    ringing), then lets go over a tenth (at least 5 ms).
  - **Swell** (Wave and Noise): rises from 60 dB down to full over it, then
    stops in 10 ms, a sound played backwards. Not on Skin: a resonance swells
    only by being struck again.
  - **Clap** (Wave and Noise, 0.11.1): three slaps 10.5 and 9 ms apart,
    each falling away in a few milliseconds (2.5 ms to a third), its own
    fall paused; at 31 ms the fourth starts the tail, the fall as it is.
    The hand clap's own envelope, so a clap is one hit and FLAM stays free.
    Rejected: claps made by FLAM, each of its three hits carrying the whole
    tail; heard on the device (2026-10-09), they blurred into one hiss.
- **A pad that moves nothing runs as before**, a whole block at once; only
  one with a depth or a CURVE other than Natural runs in stretches of 64
  samples (1.45 ms).
- **CPU.** 0.4.0 moved the knobs every 32 samples: on the Move, every
  engine's modulator on every pad took the worst case from 16.1 % to 20.0 %
  (2026-10-07), nearly all of it each engine setting itself up again. 0.4.1
  moves them every 64; works out CURVE's falls once a block; keeps Skin's
  TONE filter while TONE stands still; and works out Noise's level match
  for a moved COLOR only once COLOR has moved a hundredth of a turn (each
  stretch had cost 40 tangents). Counted, the modulators' extra work is
  less than half 0.4.0's. **On the Move, 17.7 %** (0.4.1, 2026-10-07): the
  modulators cost 1.0 %, from 3.9.
- **Start and Loop** are Noise's sample destinations, and move nothing on a
  noise table until samples come (step 9).

Rejected for the modulators:

- **Moving the knobs a block at a time** (128 samples, 2.9 ms). A pitch
  envelope stepped that coarsely zips.
- **Every 32 samples** (0.4.0). Smooth, but each engine setting itself up
  four times a block cost 3.9 % of the Move with every pad modulated; 64
  halves that, and a pitch envelope stepped every 1.45 ms still glides
  (tested: Skin's starts high and settles on PITCH).
- **CURVE as the envelope's shape.** It is the engine's own fall, so it is
  heard with no modulator set; the Envelope is plainly exponential, which is
  what a pitch drop wants.

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
- **Two DICE, each with its own undo** (the user's choice, 2026-10-07):
  Finish > DICE rolls the focused pad, and Kit > DICE (the Kit page's knob
  7) rolls all sixteen, each by its place in the kit. The user first asked
  for one knob, right for the kit and left for the pad; that leaves no way
  back, and one nudge right would replace sixteen pads, so DICE stays on
  both pages with undo on each.
  - **The gesture, as the host allows it.** The host's knob steps from its
    own copy of the value and stops at the range's ends (`onKnobTurn`,
    `knobStep` in `page_controller.mjs`), and a trigger (`access: "write"`)
    fires the same in either direction. So DICE is an int, 0 to 9999: the
    roll you are on, 0 the sound before any roll. A write past the newest
    roll makes one new roll (one a write, however fast the turn); a write
    lower goes back to that roll, at most eight back, the oldest kept
    served if asked for further. Moving away saves what is there first, so
    edits made after a roll survive a step back and forward.
  - Not saved with a kit or preset: the history is not kept, so DICE
    starts again after a load.
  - Roles by pad, to agree with the user: for example 1 kick, 2 snare,
    3 closed hat, 4 open hat (choked with 3), 5 second kick, 6 clap, 7 rim,
    8 cymbal, 9 to 11 toms, 12 and 13 percussion, 14 bell or tonal, 15 bass,
    16 effect.

  The int gesture above was the plan; reading the host's knob code for
  1.7.3 changed it (below). Kit > DICE is knob 6, not 7, so PAD stays last.

### SOUND and KIT (built, 0.10.0)

- **Pad > SOUND** is Own, then 40 sounds: Kick 1 to 7, Snare 1 to 4, Clap
  1 to 3, Rim 1 and 2, Hat 1 to 4, Open 1 and 2, Cymbal 1 and 2, Tom 1 to
  4, Perc 1 to 4, Bell 1 to 3, Bass 1 and 2, FX 1 to 3. Each is one roll of
  its role from a seed of its own (`dice_sound`), so it is the same sound
  every time, level-matched as DICE's rolls are; it keeps the pad's LEVEL,
  PAN, CHOKE and SPACE. The names say the role, not a promise of the
  timbre. Any edit or roll after a pick is the pad's Own sound again.
- **Kit > KIT** is Own, then the factory kits: a kit roll from the kit's
  seed, dressed with sends, DRIVE, CRUSH, HIGH and the Kit page
  (`dice_kit`). It writes only when the knob is let go
  (`"commit":"release"`), so a turn past ten kits loads one.
- **Both are one step for DICE Back.** A pick saves what was there into
  the DICE history (Finish > DICE for SOUND, Kit > DICE for KIT), and picks
  in a row, one knob turning, share the step: a long turn of SOUND costs
  one step back, not eight. Kit > DICE Back brings back the pads, not the
  Kit page's knobs.
- **Neither loads on a `state` load.** The saved value is only a name: the
  pads come back as saved, edits and all.
- **The factory kits are placeholders.** Twelve (Strut, Dry, Hall, Dust,
  Hard, Tape, Tin, Club, Soft, Cave, Grit, Glass), within 3 dB of each other
  through the Kit page (tests, `picks()`). The real set waits until the
  engines are done (the user's call, 2026-10-09): **30 kits**, that sound
  great and range across genres, from acoustic to electronic to
  experimental, each designed by hand, not rolled.
- **Claps and rims rebuilt (0.11.1),** after the user heard neither as its
  name on the device (2026-10-09). A clap is White, Pink or Hiss noise
  low-passed by Noise > COLOR to about 2.5 to 4 kHz (it makes up the level
  it takes) and high-passed by Pad > COLOR at about 600 Hz to 1.1 kHz, with
  Noise > CURVE on Clap. A rim is Skin at 370 to 520 Hz, RING near its
  shortest, METAL 0.6 to 1, MODE High, with 15 to 25 ms of bright noise
  and a little DRIVE: under 15 ms to fall 40 dB, energy from 200 Hz to
  4 kHz. Rejected: the clap as FLAM's three hits of full-band noise (mostly
  above 8 kHz, the hits blurred into one), and the rim as Skin alone rung
  40 to 100 ms (a pure tone at 440 to 600 Hz, a woodblock).
  Then (0.12.2), after the user's listen: a recorded clap is held whole
  for its slaps and cut at 55 to 70 ms by Noise > CURVE on Hold (they
  rang airy for about 100 ms; a plain fade from the start dulled the
  slaps), and a rim is high-passed by Pad > COLOR at 390 to 520 Hz (Rim 1
  was bassy).
- **Kicks by style (0.12.0).** Kick 1 to 7 are designed, not rolled:
  Soft (a felt beater on a deep Skin), Round (a played kick drum, beater,
  shell and a breath of Pink), Dance (a bent sine under a click, pushed),
  Boom (a long tuned sine), Tight (minimal: short and high, a tick and a
  blip), FM (Skin's ring bending the sine, a growl that settles) and Hard
  (a sine driven square, dived from four octaves up). A kick pad's DICE
  rolls one of the seven, varied a little. Each lands within about 2 dB
  of a recorded kick at the same faders, -11 dB over its loudest 400 ms;
  Hard a little over. The output's 6 dB make-up came with them: to
  0.11.1 Strut sat about 9 dB under a recorded drum at full fader.
  Rejected: kicks as rolls of one Skin recipe, 0.10.0 (seven soft thumps
  9 dB under a recorded kick).
- **Rejected: a seventh page for the kits** (the host's preset browser, a
  `list_param` level). The user wants as few pages as Quilt has
  (2026-10-09): six is already a lot, and the Kit page had a knob free.

### How DICE works (built, 0.8.0)

- **The knob is a two-word switch, Back and Roll.** Turn it right and let
  go: one roll. Turn it left and let go: one step back. However far it
  turns, one turn is one step. It declares `turn: "absolute"` (right always
  writes the second word, left the first, `knob_engine.mjs`) and `commit:
  "release"` (the host writes once, when the knob is let go,
  `page_controller.mjs`), with no option list over the page. The cell shows
  the way it last went. A jog click flips it, so a click after a roll steps
  back, and another goes forward again: an A/B of the last two.
- **Roll** goes forward to the next roll kept, if you had stepped back, or
  rolls a new one past the newest. **Back** steps to the roll before, down
  to the sound before any roll, at most eight back. Each move first keeps
  what is on the pad, so an edit after a roll survives a step back and
  forward. Rolls past the eighth back are forgotten.
- **Finish > DICE** rolls the focused pad and keeps its place in the mix:
  LEVEL, PAN and CHOKE. **Kit > DICE** rolls all sixteen and sets the mix
  too: the hats (3 and 4) choke each other in group A, the toms spread
  left to right, the percussion either side. A kit roll, or a step of it,
  starts each pad's own rolls again from there.
- **The roles** (`src/dsp/dice.c`), as proposed above. Each role says
  which engines it uses, how often, and over what ranges, with times in
  seconds and pitches in semitones: a kick is Skin low and deep with an
  envelope dropping its pitch, sometimes a tone or a click under it; a
  closed hat is Noise (Metal, Hiss, White or Wires) falling in 40 to
  100 ms; a clap is noise with FLAM; a bass is Wave alone; and so on. About
  a quarter of the rolls of each drum take a sample of its own kind from
  the library (a kick pad from Kick/, an effect from Glitch/, Foley/, Toy/
  or Voice/), cut short where the file rings long.
- **Level-matched.** Each role's engine faders are set by measurement.
  The library's files differ by about 20 dB as recorded, so each one's
  loudness and peak are measured once, as DICE plays it, into a table
  (`src/dsp/levels.c`, made by `tools/levels`; the tests measure again and
  fail if it is out of date). A roll aims a sample at its role's
  loudness from that, keeping its peak under −3 dB. Measured over 40 rolls
  of each pad: the middle half of each role's rolls within 1.3 to 4.6 dB
  of each other, every roll within 15 (the effects and percussion, whose
  rolls are most unalike, the widest).
- **Cheap.** A roll sets knobs and nothing more, on the audio thread where
  the host calls it; a sample it names loads as any other does, so a hit
  in the first moment after a roll may play without it.
- **Found on the way:** Skin with HIT Soft, MODE High and a high PITCH
  peaks over full scale (1.5 at PITCH 45). DICE does not roll it; the
  voicing pass should look at it.
- **Known edges.** Holding a step and turning DICE can lock it to that
  step, and recording automation of it records the word: either way a
  "Roll" played back rolls again every time it passes. A trigger is never
  locked, but a trigger fires the same either way, with no Back. And a
  kit saved before 0.8.0, or a host restoring every key one by one, writes
  DICE's word back: `state` (0.8.1) leaves DICE out.

Rejected for DICE:

- **An int roll counter, 0 to 9999** (the plan above). The host steps an
  int by a hundredth of its range a detent, halved (50 here), and keeps
  its own copy between turns, so its number and Strut's drift apart, and a
  turn left could land past the newest roll and roll.
- **A write per detent.** One flick of the encoder is a dozen detents: a
  dozen rolls, and the sound before them gone past the eighth.
- **One knob for both**, right for the kit and left for the pad (the
  user's first idea): no way back, and a nudge replaces sixteen pads.
- **Measuring each roll by playing it.** A roll runs on the audio thread;
  rendering a hit of every pad to measure it would cost tens of
  milliseconds there.

---

## Knob pictures (built, 0.11.0)

Every cell draws a picture of what its knob does in place of Schwung's dial,
from `src/canvas.js`, in the same hand as Quilt's. Each param in
`chain_params` names the one kind, `viz: {kind: "custom:strut"}`, and
`module.json` declares `canvas_script`, so Schwung (1.7.3 and later) loads
the file and hands each cell a 32×15 frame. An older host, or a picture that
throws, draws the built-in dial instead. A pad page's keys reach the picture
bare (`tune`, not `p05_tune`), so one picture serves all sixteen pads, and
moving to another pad eases each picture to that pad's value.

Quilt's three rules, unchanged:

- **Small and centred.** Drawn in a 32×12 design space, scaled to 80 % about
  the cell's centre, so lines stay one pixel.
- **One pen.** One-pixel lines. What rings (a head, a wave) is a plain line,
  what strikes it (stick, mallet, brush) an outline, and noise is dotted.
  Solid fills are kept to the chosen pad on PAD and nothing else.
- **Motion means time.** Only Pad > DECAY, Skin > RING, Wave > DECAY,
  Noise > DECAY, each engine's RATE and Pad > SPACE move, while the knob is
  touched or for 1.5 s after it turns. Every value eases in 140 ms.

What they show, page by page:

- **Pad.** SOUND the drum (kick, snare, hat, cymbal, cowbell, bolt for FX…)
  and its number in its kind, sliding in the way you turned. TUNE more
  cycles, higher. DECAY a strike and its tail. COLOR the filter's slope.
  SKIN, WAVE and NOISE each engine's own sound (a ring, a saw, a cloud), as
  big as its fader; a dotted line when off. SPACE echoes rolling out.
- **Skin.** PITCH a drum, smaller as it rises. RING a ring dying away. HIT
  what strikes the head: stick, mallet, brush, a wave, a cloud. SNAP the
  strike, a spike widening to a thud. METAL the two partials climbing. TONE
  the low-pass. MODE low, band or high-pass around the pitch.
- **Wave.** PITCH cycles. BEND where the pitch starts and how it comes home.
  TABLE each table's cycle, sliding; WAVE the cycle at that point of the
  table Wave > TABLE picks (worked out from the same recipes as `tables.c`,
  with spread phases). FM the cycles bunching, RING the beating.
- **Noise.** PITCH the burst shorter as it speeds up. MODE a sample, its
  partials, or its noise. TABLE each noise as a stretch of signal; a sample
  as its own outline, the same for the same name. START and LOOP a sample
  with the marker or the loop.
- **Mod views.** MOD a ring (Sound) and a sine (Mod), the one on drawn and
  the other dotted. KIND the modulator's shape. RATE that shape at its speed
  with the time or note value in figures (`141MS`, `1/4T`, `4BAR`). CURVE the
  fall's shape. AIM an arrow into a small picture of the knob it moves.
  DEPTH how far and which way.
- **Finish.** LEVEL a fader wedge. PAN a knob on a rail. FLAM three hits
  drawing apart. DRIVE flattening shoulders. CRUSH a stepped sine. LOW and
  HIGH the shelves. DICE a die and the way it turns.
- **Kit.** SIZE the room in perspective. GLUE soft hits pulled up to loud
  ones. WARM a rounder, thicker wave. VOL a wedge with 0 dB marked. CHOKE a
  ring cut off, and the group's letter. KIT a kit from the front and its
  number. DICE two dice. PAD the sixteen pads as the Move lays them out
  (pad 1 bottom left), the chosen one solid, and its number. The grid is
  drawn in the screen's own pixels: scaled, its rows ran together.

Rejected: a kit drawn as three drums side by side. At this size three shapes
in a row read as letters ("TOT", "ROT"); the kit is one cluster, toms on the
kick.

A still picture is drawn once into a bitmap and replayed as horizontal runs
(64 host calls at most). Measured in Node with the JIT off, closer to the
Move's QuickJS: a still page 0.03 ms; all eight cells moving at once under
0.9 ms, with each Wave cycle worked out once and kept.

`tests/widgets.test.mjs` loads the file the way the host does and draws
every cell of every page, in both views, through the host's own frame and
viz resolver. It checks that each draws inside its frame and near its
middle, that every option and two settings of every knob look different,
that its option lists match what Strut serves, that WAVE follows TABLE, that
an unanswered value draws nothing, and that RATE holds still until touched
or turned.

## Implementation notes

- **`pad_press` and the host's slow-param warning.** The Move logs
  `param-slow: set ... synth:pad_press took 2 ms` (0.9.1, 2026-10-09).
  Strut's own part is a time stamp and one comparison (`strut_press`); the
  rest is the host's handling of the key around the call (it looks the key
  up and makes a smoother for it, as "1" reads as a number). Nothing to fix
  in Strut, and no request to the host (the user's rule).

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
  early) glides across the block, with 6 dB of make-up (0.12.0); a soft
  limiter above 0.7 of full scale rounds off a stack of pads instead of
  clipping; then 16 bits, rounded with one step of
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
  - **Resynth and Noise:** built (0.7.0); see *How Resynth and Noise
    work*. The analysis runs on the loader's thread when a pad first asks
    for the mode; playback is 32 sines a voice plus two grains of the
    leftover noise, and cost no more than Sample mode on a laptop.
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
   DICE waits for step 10). On the Move, every effect on every pad: 15.3 %
   (0.3.1; 18.7 % before the effects ran in one loop). Still to do: hear it.
7. ~~**Modulation** (the MOD views)~~ (0.4.0). On the Move, every
   modulator on every pad: 20.0 % with every effect (0.4.0), 17.7 % once
   their setup was halved (0.4.1). Still to do: hear it.
8. ~~**Kit** page: room, glue, warmth~~ (0.5.0). On the Move, the room at
   its longest with GLUE and WARM full: 19.1 %, against 19.3 % without in
   the same run (2026-10-07); in a second run (0.6.0) 21.1 % against 18.9,
   its runs reaching 22.0 %. So the kit costs up to about 2 %, and the
   worst case sits near 22 % of the 25 % allowed. Still to do: hear it.
9. ~~Samples~~ (see *The sample library*, *How Resynth and Noise work*):
   - ~~ship `src/samples/` in the module and the tarball~~ (0.6.0; the
     tarball is 40 MB; the library installs in 2 s);
   - ~~TABLE past the noise tables: the library, then your own samples~~
     (0.6.0, one long list);
   - ~~load on demand per pad, off the audio thread~~ (0.6.0);
   - ~~Sample mode, START and LOOP; Cycle/ looped and pitched, in copies an
     octave apart~~ (0.6.0); ~~Resynth and Noise modes~~ (0.7.0);
   - ~~CPU measured with resynthesis on every pad~~ (0.7.0: 20.5 % on
     the Move, runs to 22.3 %, against 17.0 % for samples; still to do:
     hear it);
   - ~~tests: every file parses, every Cycle file is 2048 samples, the
     tarball holds the library~~ (0.6.0).
10. The SOUND library, factory kits and DICE, drawing on the sample library
    (DICE by role), `help.json` and README:
    - ~~DICE, Finish's and the Kit page's, by role, with eight steps
      back, level-matched~~ (0.8.0; still to do: hear it);
    - ~~`state`, so a kit saves and loads whole, without DICE~~ (0.8.1;
      a module that does not answer `state` makes the host retry every
      few seconds on the channel the knobs read through);
    - ~~the SOUND library, and KIT with placeholder kits~~ (0.10.0);
    - the 30 factory kits, after more engine work (the user's call,
      2026-10-09);
    - ~~`help.json` and the README~~ (0.10.0);
    - ~~knob pictures for every control, in Quilt's style~~ (0.11.0).
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
  - Modulation: an Envelope on Skin's pitch starts high and settles on
    PITCH, and the knob is never written; an LFO on Wave's level wobbles
    over 10 dB; Random gives two hits two pitches; Velocity on Noise's
    level widens the hard-soft gap; Hold stays full and Swell rises, Soft
    starts gently and Ping falls faster, and Skin's Hold rings on; every
    KIND on every destination of every engine, at full depth either way, is
    finite, heard and ends.
  - Finish: every Finish knob and Pad COLOR at its ends and middle, on a
    pad of all three engines, stays finite, sounds and dies away; PAN hard
    left or right is that side alone, at the centre's power to 0.1 dB;
    CHOKE silences a pad in its group, and leaves one in another ringing;
    FLAM gives three rising onsets its gap apart; DRIVE grows a sine's
    third harmonic and keeps a loud hit within 6 dB; CRUSH holds samples;
    LOW and HIGH lift and cut their ends of White noise by over 12 dB; Pad
    COLOR darkens and thins.
  - Kit: with SPACE, GLUE and WARM at zero the pads pass untouched; every
    kit knob at its ends and middle is finite, peaks under 1 and falls
    silent; SPACE grows to about 6 dB under a beat and rings on, wide,
    after it; SIZE rings longer as it grows; GLUE brings soft hits over
    2 dB nearer loud ones within 2 dB of the level and 3 of the peaks;
    WARM adds second and third harmonics within 2 dB, centred, and nothing
    at zero.
  - Samples: the library lists 208 sounds, drums first; every file reads
    (one-shots at 44.1 kHz under 4 s, cycles 2048 samples) and loads;
    TABLE takes a sample by name; a sample not loaded plays nothing; every
    sound at the knobs' defaults sounds, stays finite, peaks under 0.95
    and ends; PITCH +12 halves a sample's length and START half way plays
    its second half, to 5 %; LOOP repeats a slice past the end; a cycle
    plays 55 Hz at PITCH 0 and loops; the loader never lets go of a sample
    a voice plays, and holds under 20 MB after 120 samples; its own thread
    loads a pad's sample while blocks render. CI checks the tarball holds
    every sample and SOURCES.md.
  - Resynth and Noise (MODE): every one-shot in both sounds, stays finite,
    peaks under 0.95, ends, and is within 6 dB of the sample (the middle
    within 1.5); Resynth keeps a bass note's pitch to 2 %, its length at
    DECAY's centre to 10 %, an octave up at PITCH +12 as long, four times
    as long at DECAY fully right and a quarter at 0, both at its pitch; a
    short LOOP holds a moment steady for seconds; Noise takes a mallet's
    pitch out; a mode not yet made plays the sample, and a cycle plays as
    itself.
  - DICE: Roll makes a new sound and leaves the other pads alone; Back
    steps to the roll before and down to the sound before any roll, and no
    further; Roll after Back goes forward, not to a new roll; an edit
    after a roll survives a step back and forward; eight back and no
    further, and forward through the same eight; a pad's roll keeps LEVEL,
    PAN and CHOKE; Kit > DICE rolls all sixteen, chokes the hats together,
    steps back to every pad as it was and forward again, and starts each
    pad's rolls afresh. Forty rolls of every pad: each sounds, stays
    finite, peaks under 0.95 and ends in its role's time (a closed hat in
    0.4 s, a kick in 2); the middle half of each role within 5 dB and all
    within 15. The levels table matches the library as it plays.
  - The faders (SKIN, WAVE, NOISE, LEVEL): off at zero, 2 to 4 dB each tenth of a turn. A quiet tail
    through the real 16-bit output stays within 1.5 steps of the exact
    signal and ends in true silence.
- `tools/demo.c` renders a few hand-set sounds and a groove to a WAV, for
  listening away from the Move (`build/tests/demo out.wav`).
- The CI builds a device tarball on every push.
- **Listening.** The user plays it on the Move.

## Later

- Separate outputs.
- MPE / pad pressure as a modulation kind.
