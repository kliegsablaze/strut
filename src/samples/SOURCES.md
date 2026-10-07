# Strut's sample library

208 sounds for the Noise engine's sample mode, in 23 folders, one per
kind, each file named for its kind and a number (`Kick 001.wav`).

Every sound comes from a source that puts it in the public domain, or is
generated here, so the library can ship with Strut and be used commercially,
with no attribution required:

| Source | Licence | Where |
|---|---|---|
| VCSL, the Versilian Community Sample Library (Versilian Studios) | CC0 1.0 | https://github.com/sgossner/VCSL |
| The Open Source Drum Kit (Real Music Media) | Public domain, in the author's words: "completely in the public domain" | https://github.com/crabacus/the-open-source-drumkit, http://www.kvraudio.com/forum/viewtopic.php?t=277132 |
| Kenney: Impact Sounds, Sci-fi Sounds, Digital Audio, Voiceover Pack, RPG Audio, Casino Audio, UI Audio | CC0 1.0 (License.txt in each pack) | https://kenney.nl/assets/category:Audio |
| Generated here (the classic single cycles) | no third-party material | |

Attribution is not required by any of them. It is given here only so every
file's origin can be traced.

## How the one-shots were made

Everything except `Cycle/`:

- **Format:** 44.1 kHz, 16-bit WAV with triangular dither. A file whose two
  channels were the same is stored mono.
- **Length:** at most 4 s. The silence before a sound is cut, leaving 1 ms. The
  tail is cut once it has died below −66 dBFS. A sound cut at 4 s fades out
  over its last 0.6 s. A bowed or swelling sound fades in over 50–300 ms.
- **Loudness:** matched on the loudest 400 ms (EBU R128 momentary
  loudness) to −18 LUFS, with peaks no higher than −1 dBFS.
  - Short, sharp hits are allowed up to 4 dB of fast peak limiting to get
    there. A few stay up to 4.5 dB quieter, rather than be squashed further.
  - A mono file measures 3 dB lower than the same sound in stereo; it plays
    at the same level.
- **Chords and arpeggios** (Keys, and Pluck 010) are built here from single
  CC0 notes. Some notes are pitched by a semitone (by resampling), and some
  are strummed by a few milliseconds.
- **Our own processing:** two glitches are made here from CC0 hits:
  - Glitch 007, a clap cut into a falling stutter;
  - Glitch 008, a snare through a bit crusher.

## How the single cycles were made (`Cycle/`)

Each file is exactly **one period of a waveform, 2048 samples long**: mono,
16-bit, 44.1 kHz, peak −1 dBFS, no DC. A `smpl` chunk loops the whole file.
Played at 44.1 kHz unchanged, a cycle sounds at 21.53 Hz; a sampler plays
note *f* by stepping through it 2048·*f*/44100 samples a time.

- **Cycle 001–016 are generated here.**
  - The classic shapes are additive, with exact harmonics, Lanczos-smoothed
    so they do not ring: sine, triangle, saw, square, two pulses, soft saw,
    hollow, organ, a sung "ah".
  - The rest are drawn 16 times oversampled and averaged down: FM, wavefold,
    hard sync, overdrive, a stepped sine.
- **Cycle 017–036 are cut from CC0 recordings:**
  1. The note's period is found by autocorrelation near the pitch its file
     name gives.
  2. Eight consecutive periods are read with windowed-sinc interpolation and
     averaged.
  3. DC is removed.
  4. The cycle is started at a rising zero crossing.

  The notes column gives the measured pitch, and how periodic the note was
  (1 is perfectly periodic).

`LUFS` below is the measured max momentary loudness. `peak` is in dBFS.

## Kick

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Kick 001.wav | Open Source Drum Kit: kick/kick21.wav |  | 0.74 | -18.9 | -1.2 | 2 |
| Kick 002.wav | Open Source Drum Kit: kick/kick15.wav |  | 0.65 | -18.0 | -1.3 | 2 |
| Kick 003.wav | Open Source Drum Kit: kick/kick9.wav |  | 0.74 | -18.0 | -2.1 | 2 |
| Kick 004.wav | Open Source Drum Kit: kick/kick19.wav | pitched down 4 semitones | 0.96 | -18.5 | -1.2 | 2 |
| Kick 005.wav | VCSL: Membranophones/Struck Membranophones/Bass Drum 1/BDrumNew_hit_v7_rr1_Sum.wav |  | 3.68 | -18.0 | -4.0 | 2 |

## Snare

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Snare 001.wav | Open Source Drum Kit: snare/snare-top33.wav |  | 0.27 | -20.5 | -1.2 | 2 |
| Snare 002.wav | Open Source Drum Kit: snare/snare-top21.wav |  | 0.25 | -20.0 | -1.2 | 2 |
| Snare 003.wav | Open Source Drum Kit: snare/snare-top-off25.wav | snares off | 0.3 | -19.9 | -1.2 | 2 |
| Snare 004.wav | VCSL: Membranophones/Struck Membranophones/Snare Drum, Modern 1/Snare2_HitSN_v6_rr1_Mid.wav |  | 0.46 | -18.0 | -2.5 | 2 |
| Snare 005.wav | VCSL: Membranophones/Struck Membranophones/Snare Drum, Modern 2/Snare3M_HitSN_v5_rr1_Mid.wav |  | 0.61 | -18.0 | -2.3 | 2 |
| Snare 006.wav | VCSL: Membranophones/Struck Membranophones/Snare Drum, Modern 3/Snare4_HitSN_v5_rr1_Mid.wav |  | 1.0 | -18.7 | -1.2 | 2 |

## Rim

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Rim 001.wav | Open Source Drum Kit: rimshot/rimshot15.wav |  | 0.23 | -20.3 | -1.2 | 2 |
| Rim 002.wav | Open Source Drum Kit: rimshot/rimshot9.wav |  | 0.22 | -19.8 | -1.2 | 2 |
| Rim 003.wav | Open Source Drum Kit: sidestick/sidestick19.wav |  | 0.19 | -19.9 | -1.2 | 2 |
| Rim 004.wav | Open Source Drum Kit: sidestick/sidestick10.wav |  | 0.19 | -19.5 | -1.2 | 2 |
| Rim 005.wav | VCSL: Idiophones/Struck Idiophones/Claves/Claves2_Hit_v3_rr1_Mid.wav | claves pitched down 5 semitones | 0.47 | -18.6 | -1.2 | 2 |

## Clap

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Clap 001.wav | VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr1.wav |  | 0.35 | -20.6 | -1.2 | 2 |
| Clap 002.wav | VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr4.wav |  | 0.71 | -19.5 | -1.2 | 2 |
| Clap 003.wav | VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr6.wav |  | 0.36 | -19.2 | -1.2 | 2 |
| Clap 004.wav | VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr3.wav |  | 0.69 | -20.3 | -1.2 | 2 |
| Clap 005.wav | VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr2.wav<br>VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr5.wav<br>VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr6.wav | three takes layered | 0.41 | -18.1 | -1.2 | 2 |

## Hat

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Hat 001.wav | Open Source Drum Kit: hihat/closed-hihat/chh33.wav |  | 0.36 | -18.0 | -3.0 | 2 |
| Hat 002.wav | Open Source Drum Kit: hihat/closed-hihat/chh21.wav |  | 0.31 | -18.0 | -2.2 | 2 |
| Hat 003.wav | Open Source Drum Kit: hihat/foot-hihat/fhh16.wav |  | 0.22 | -19.1 | -1.2 | 2 |
| Hat 004.wav | Open Source Drum Kit: hihat/half-open-hihat/hohh29.wav |  | 1.06 | -18.0 | -4.5 | 2 |
| Hat 005.wav | VCSL: Idiophones/Struck Idiophones/Hi-Hat Cymbal/HiHat_HitC_v4_rr1_Mid.wav |  | 0.42 | -18.0 | -6.0 | 2 |
| Hat 006.wav | VCSL: Idiophones/Struck Idiophones/Hi-Hat Cymbal/HiHat_HitLoose_rr1_Mid.wav |  | 0.57 | -18.0 | -7.2 | 2 |

## Cymbal

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Cymbal 001.wav | Open Source Drum Kit: crash/crash25.wav |  | 4.0 | -18.0 | -4.5 | 2 |
| Cymbal 002.wav | Open Source Drum Kit: ride/ride-bell6.wav |  | 4.0 | -18.0 | -1.4 | 2 |
| Cymbal 003.wav | Open Source Drum Kit: ride/ride-mid-out3.wav |  | 3.72 | -18.7 | -1.2 | 2 |
| Cymbal 004.wav | VCSL: Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_hit_f1.wav |  | 4.0 | -18.0 | -10.9 | 2 |
| Cymbal 005.wav | VCSL: Idiophones/Struck Idiophones/Finger Cymbals/Fing_Cymb.wav |  | 1.47 | -18.0 | -5.0 | 2 |

## Tom

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Tom 001.wav | Open Source Drum Kit: toms/large-tom29.wav |  | 0.73 | -20.3 | -1.2 | 2 |
| Tom 002.wav | Open Source Drum Kit: toms/medium-tom25.wav |  | 0.62 | -21.9 | -1.2 | 1 |
| Tom 003.wav | Open Source Drum Kit: toms/small-tom25.wav |  | 0.63 | -21.9 | -1.2 | 1 |
| Tom 004.wav | VCSL: Membranophones/Struck Membranophones/Frame Drum/HDrumL_Hit_v3_rr1_Sum.wav |  | 1.29 | -18.0 | -5.2 | 2 |
| Tom 005.wav | VCSL: Membranophones/Struck Membranophones/Frame Drum/HDrumS_Hit_v3_rr1_Sum.wav |  | 1.08 | -18.2 | -1.2 | 2 |

## Percussion

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Percussion 001.wav | VCSL: Idiophones/Struck Idiophones/Cowbells/Cowbell1_Hit_v4_rr1_Mid.wav |  | 0.59 | -18.0 | -1.5 | 2 |
| Percussion 002.wav | VCSL: Idiophones/Struck Idiophones/Woodblock/wood_click_ff.wav |  | 0.36 | -20.1 | -1.2 | 2 |
| Percussion 003.wav | VCSL: Idiophones/Struck Idiophones/Claves/Claves1_Hit_v3_rr1_Mid.wav |  | 0.34 | -19.8 | -1.2 | 2 |
| Percussion 004.wav | VCSL: Idiophones/Struck Idiophones/Agogo Bells/Agogo_High_v3_rr1_Mid.wav |  | 0.31 | -18.8 | -1.2 | 2 |
| Percussion 005.wav | VCSL: Idiophones/Struck Idiophones/Tambourine 1/Tamb1_Hit_v2_rr1_Mid.wav |  | 0.41 | -18.0 | -5.1 | 2 |
| Percussion 006.wav | VCSL: Idiophones/Struck Idiophones/Shaker, Small/Mid_ShakerDouble_Down_rr1.wav |  | 0.26 | -18.0 | -2.0 | 2 |
| Percussion 007.wav | VCSL: Idiophones/Struck Idiophones/Triangles/Triangle1_Hit_v2_rr1_Mid.wav |  | 1.82 | -18.0 | -7.3 | 2 |
| Percussion 008.wav | VCSL: Idiophones/Struck Idiophones/Cajon/Cajon_hit1_fff_rr1.wav |  | 0.62 | -18.9 | -1.2 | 2 |
| Percussion 009.wav | VCSL: Idiophones/Struck Idiophones/Slit Drum/LogDrumLo_MedM_v3_rr1_Sum.wav |  | 0.53 | -18.0 | -2.4 | 2 |
| Percussion 010.wav | VCSL: Idiophones/Struck Idiophones/Cabasa/Cabasa1_Hit_rr1_Mid.wav |  | 0.16 | -20.2 | -1.2 | 2 |
| Percussion 011.wav | VCSL: Membranophones/Struck Membranophones/Conga/Conga_HitN_v3_rr1_Sum.wav |  | 0.78 | -18.0 | -3.9 | 2 |
| Percussion 012.wav | VCSL: Membranophones/Struck Membranophones/Bongos/BongoH_Hit1_v3_rr1_Mid.wav |  | 0.58 | -18.3 | -1.2 | 2 |
| Percussion 013.wav | VCSL: Membranophones/Struck Membranophones/Darbuka/Darbuka_1_hit_vl2_rr1.wav |  | 1.25 | -18.0 | -5.5 | 2 |

## Bell

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Bell 001.wav | VCSL: Idiophones/Struck Idiophones/Glockenspiel/glock_loud_C6_01.wav |  | 1.87 | -18.0 | -7.4 | 2 |
| Bell 002.wav | VCSL: Idiophones/Struck Idiophones/Hand Chimes/sus_C5_r01_main.wav |  | 4.0 | -18.0 | -11.0 | 2 |
| Bell 003.wav | VCSL: Idiophones/Struck Idiophones/Tubular Bells 1/chimes_C4_ff_rr2.wav |  | 4.0 | -18.0 | -10.3 | 2 |
| Bell 004.wav | VCSL: Idiophones/Plucked Idiophones/Kalimba, Kenya/Mbira6_Normal_MainSpirit_A4_k1_vl3_rr2.wav |  | 0.66 | -18.6 | -1.2 | 2 |
| Bell 005.wav | VCSL: Idiophones/Struck Idiophones/Vibraphone/Hard Mallets/Vibes_hard_F4_v3_rr1_Main.wav |  | 4.0 | -18.0 | -9.3 | 2 |
| Bell 006.wav | VCSL: Idiophones/Struck Idiophones/Marimba/Marimba_hit_Outrigger_C4_loud_01.wav |  | 1.47 | -18.0 | -8.5 | 2 |
| Bell 007.wav | VCSL: Idiophones/Struck Idiophones/Brake Drum/BrakeDrum1_Hammer_v3_rr1_Mid.wav |  | 0.72 | -18.0 | -4.2 | 2 |

## Mallet

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Mallet 001.wav | VCSL: Idiophones/Struck Idiophones/Balafon/Hard Mallet/EthnicXylo_hardM_C4_vl3_rr1_Mid.wav | balafon, hard mallet | 0.48 | -18.4 | -1.2 | 2 |
| Mallet 002.wav | VCSL: Idiophones/Struck Idiophones/Balafon/Soft Mallet/EthnicXylo_softM_C4_vl3_rr2_Mid.wav | balafon, soft mallet | 0.45 | -18.0 | -3.3 | 2 |
| Mallet 003.wav | VCSL: Idiophones/Struck Idiophones/Xylophone/Hard Mallets/Xylo_Hard_C5_ff_01_far.wav | xylophone, hard mallets | 1.0 | -18.0 | -5.9 | 2 |
| Mallet 004.wav | VCSL: Idiophones/Struck Idiophones/Xylophone/Soft Mallets/Xylo_Soft_C5_ff_01_far.wav | xylophone, soft mallets | 0.95 | -18.0 | -7.6 | 2 |
| Mallet 005.wav | VCSL: Idiophones/Struck Idiophones/Vibraphone/Soft Mallets/Vibes_soft_C3_v2_rr1_Main.wav | vibraphone, soft mallets | 4.0 | -18.0 | -12.5 | 2 |
| Mallet 006.wav | VCSL: Idiophones/Struck Idiophones/Tubular Glockenspiel/C1_loud1.wav | tubular glockenspiel | 4.0 | -18.0 | -8.2 | 2 |
| Mallet 007.wav | VCSL: Idiophones/Struck Idiophones/Slit Drum/LogDrumHi_MedM_v3_rr1_Sum.wav | log drum, high | 0.79 | -18.0 | -3.0 | 2 |

## Metal

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Metal 001.wav | VCSL: Idiophones/Struck Idiophones/Anvil/Anvil_Hit3_v3_rr1_Mid.wav | anvil | 0.59 | -18.0 | -8.0 | 2 |
| Metal 002.wav | VCSL: Idiophones/Struck Idiophones/Bell Tree/Stroke/BellTree_Stroke_1_Mid.wav | bell tree, stroked | 1.4 | -18.0 | -11.3 | 2 |
| Metal 003.wav | VCSL: Idiophones/Struck Idiophones/Hand Bells, Nepalese/HB_1.wav | Nepalese hand bell | 1.25 | -18.0 | -5.8 | 2 |
| Metal 004.wav | VCSL: Idiophones/Struck Idiophones/Brake Drum/BrakeDrum2_Hammer3_v3_rr1_Mid.wav | brake drum, hammer | 0.49 | -18.5 | -1.2 | 2 |
| Metal 005.wav | VCSL: Idiophones/Struck Idiophones/Gong 1/gong_scrape_mf.wav | gong, scraped | 4.0 | -18.0 | -8.4 | 2 |
| Metal 006.wav | Kenney: kenney_impact-sounds/Audio/impactBell_heavy_000.ogg |  | 1.48 | -21.3 | -1.2 | 1 |
| Metal 007.wav | Kenney: kenney_rpg-audio/Audio/metalPot1.ogg |  | 1.39 | -18.0 | -9.0 | 2 |

## Pluck

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Pluck 001.wav | VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_C3_f2.wav | concert harp | 4.0 | -18.0 | -12.4 | 2 |
| Pluck 002.wav | VCSL: Chordophones/Composite Chordophones/Folk Harp/EWHarp_Normal_C3_v3_RR1.wav | folk harp | 4.0 | -18.0 | -7.8 | 2 |
| Pluck 003.wav | VCSL: Chordophones/Composite Chordophones/Strumstick/Finger/Strumstick_Finger_Str1_Main_D2_vl3_rr1.wav | strumstick | 4.0 | -18.0 | -4.8 | 2 |
| Pluck 004.wav | VCSL: Chordophones/Zithers/Dan Tranh/Normal/D#3_ff_1.wav | dan tranh (Vietnamese zither) | 2.82 | -21.0 | -4.5 | 1 |
| Pluck 005.wav | VCSL: Chordophones/Zithers/Dan Tranh/Tremolo/B2_Trem_1.wav | dan tranh, tremolo | 4.0 | -21.0 | -8.6 | 1 |
| Pluck 006.wav | VCSL: Chordophones/Zithers/Psaltery, Bowed and Plucked/Pluck/BowedPsaltery_C4_Main_Pluck_rr1.wav | psaltery, plucked | 4.0 | -18.0 | -6.8 | 2 |
| Pluck 007.wav | VCSL: Idiophones/Plucked Idiophones/Mbira dzaVadzimu Nyamaropa, Zimbabwe, Low B/MBira4_pluck_Main_A2_4_50_100_rr2.wav | mbira dzaVadzimu | 1.03 | -18.0 | -8.8 | 2 |
| Pluck 008.wav | VCSL: Idiophones/Plucked Idiophones/Nyunga Nyunga, Mozambique, Low F/Mbira2_Normal_MainSpirit_C3_k15_vl3_rr3.wav | nyunga nyunga mbira | 4.0 | -18.0 | -4.8 | 2 |
| Pluck 009.wav | VCSL: Idiophones/Plucked Idiophones/Kalimba, Tanzania/MBira3_pluck_Main_A#3_k18_50_100_rr2.wav | kalimba, Tanzania | 1.35 | -18.6 | -1.2 | 2 |
| Pluck 010.wav | VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_A2_f1.wav<br>VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_C3_f2.wav<br>VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_D4_f1.wav<br>VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_F4_f1.wav<br>VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_A4_f1.wav<br>VCSL: Chordophones/Composite Chordophones/Concert Harp/KSHarp_C5_f1.wav | concert harp, an A minor arpeggio built from single notes | 4.0 | -18.0 | -6.4 | 2 |

## Wind

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Wind 001.wav | VCSL: Aerophones/Edge-blown Aerophones/Baroque Alto Recorder/Staccato/AltRecorder_Stac_C4_rr1_Main.wav | alto recorder, staccato | 0.41 | -18.0 | -10.5 | 2 |
| Wind 002.wav | VCSL: Aerophones/Edge-blown Aerophones/Baroque Alto Recorder/Sustain/AltRecorder_Sus_C5_rr1_Main.wav | alto recorder | 4.0 | -18.0 | -14.1 | 2 |
| Wind 003.wav | VCSL: Aerophones/Edge-blown Aerophones/Ocarina, Typical/Sustains/Sus/StdOcarina_Sus_D4.wav | ocarina | 4.0 | -18.0 | -14.2 | 2 |
| Wind 004.wav | VCSL: Aerophones/Free Aerophones/Harmonica-Hohner-Special20-C/Sustains/Normal/Hohner-Special20_Normal_C4.wav | harmonica | 4.0 | -21.1 | -13.7 | 1 |
| Wind 005.wav | VCSL: Aerophones/Reed Aerophones/Tenor Saxophone/Staccato/BrettTenor_Staccato_Main_A2_vl2_rr4.wav | tenor saxophone, staccato | 0.42 | -18.0 | -4.6 | 2 |
| Wind 006.wav | VCSL: Aerophones/Reed Aerophones/Tenor Saxophone/Non-Vibrato/BrettTenor_NV_Main_A#2_vl3_rr1.wav | tenor saxophone | 4.0 | -18.0 | -5.2 | 2 |
| Wind 007.wav | VCSL: Aerophones/Reed Aerophones/Saxello/Staccato/BrettSaxello_Stcts_MainSpirit_A#3_vl2_rr2.wav | saxello, staccato | 0.38 | -18.0 | -3.6 | 2 |
| Wind 008.wav | VCSL: Aerophones/Edge-blown Aerophones/Pipe Organ/Quiet/NT5_Man3Quiet_C3_rr1.wav | pipe organ, quiet | 4.0 | -18.0 | -13.8 | 2 |
| Wind 009.wav | VCSL: Aerophones/Edge-blown Aerophones/Pipe Organ/Loud/Rode_Man3Open_C3.wav | pipe organ, open | 4.0 | -18.0 | -11.5 | 2 |
| Wind 010.wav | VCSL: Aerophones/Edge-blown Aerophones/Renaissance Organ/Full/RenOrgan_Full_Room_C3_rr1.wav | renaissance organ, full | 4.0 | -18.0 | -11.7 | 2 |

## Bowed

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Bowed 001.wav | VCSL: Chordophones/Zithers/Psaltery, Bowed and Plucked/LongBow/BowedPsaltery_C4_Main_LongBow_rr1.wav | bowed psaltery | 4.0 | -18.0 | -12.7 | 2 |
| Bowed 002.wav | VCSL: Chordophones/Zithers/Psaltery, Bowed and Plucked/Spiccato/BowedPsaltery_E4_Main_Spic_rr3.wav | bowed psaltery, spiccato | 3.37 | -18.0 | -8.4 | 2 |
| Bowed 003.wav | VCSL: Idiophones/Struck Idiophones/Brake Drum/BrakeDrum1_Bowed_rr2_Mid.wav | bowed brake drum | 4.0 | -18.0 | -14.8 | 2 |
| Bowed 004.wav | VCSL: Idiophones/Struck Idiophones/Brake Drum/BrakeDrum2_Bowed_rr1_Mid.wav | bowed brake drum | 4.0 | -18.0 | -14.5 | 2 |
| Bowed 005.wav | VCSL: Idiophones/Struck Idiophones/Vibraphone/Bowed/Vibes_bowed_D4_rr1_Main.wav | bowed vibraphone | 4.0 | -18.0 | -14.6 | 2 |
| Bowed 006.wav | VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Fast/glass4_D5_Fast_1_Main.wav | wine glass, rubbed fast | 4.0 | -18.0 | -14.3 | 2 |

## Keys

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Keys 001.wav | VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_C3_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_E3_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_F#3_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_A#3_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_D4_vl3_rr1.wav | grand piano, C major 9, built from single notes | 4.0 | -18.0 | -7.9 | 2 |
| Keys 002.wav | VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_A#2_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_C3_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_E3_vl3_rr1.wav<br>VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_F#3_vl3_rr1.wav | grand piano, A minor 7, built from single notes | 4.0 | -18.0 | -7.0 | 2 |
| Keys 003.wav | VCSL: Electrophones/TX81Z/FM Piano/FMPiano_C3_vl3.wav<br>VCSL: Electrophones/TX81Z/FM Piano/FMPiano_E3_vl3.wav<br>VCSL: Electrophones/TX81Z/FM Piano/FMPiano_G#3_vl3.wav<br>VCSL: Electrophones/TX81Z/FM Piano/FMPiano_C4_vl3.wav | TX81Z FM piano, C major 7, built from single notes | 4.0 | -21.1 | -9.7 | 1 |
| Keys 004.wav | VCSL: Chordophones/Zithers/Harpsichord, English/Sustains/Normal/ZuckermannKitHarpsi_Normal_Sus_D3_rr1.wav<br>VCSL: Chordophones/Zithers/Harpsichord, English/Sustains/Normal/ZuckermannKitHarpsi_Normal_Sus_E3_rr1.wav<br>VCSL: Chordophones/Zithers/Harpsichord, English/Sustains/Normal/ZuckermannKitHarpsi_Normal_Sus_A#3_rr1.wav<br>VCSL: Chordophones/Zithers/Harpsichord, English/Sustains/Normal/ZuckermannKitHarpsi_Normal_Sus_D4_rr1.wav | harpsichord, D minor, built from single notes | 4.0 | -18.0 | -5.8 | 2 |
| Keys 005.wav | VCSL: Chordophones/Zithers/Upright Piano, Yamaha/Sustains/Upright1_Sus_C2_vl2_rr1.wav<br>VCSL: Chordophones/Zithers/Upright Piano, Yamaha/Sustains/Upright1_Sus_G3_vl2_rr1.wav<br>VCSL: Chordophones/Zithers/Upright Piano, Yamaha/Sustains/Upright1_Sus_C4_vl2_rr1.wav<br>VCSL: Chordophones/Zithers/Upright Piano, Yamaha/Sustains/Upright1_Sus_G4_vl2_rr1.wav | upright piano, open fifths on C, built from single notes | 4.0 | -18.0 | -7.7 | 2 |
| Keys 006.wav | VCSL: Electrophones/TX81Z/Clavisynth/Clavisynth_E3_vl3.wav<br>VCSL: Electrophones/TX81Z/Clavisynth/Clavisynth_C4_vl3.wav<br>VCSL: Electrophones/TX81Z/Clavisynth/Clavisynth_E4_vl3.wav | TX81Z Clavisynth, E fifths, built from single notes | 1.75 | -21.0 | -9.3 | 1 |

## Bass

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Bass 001.wav | VCSL: Electrophones/TX81Z/FM Piano/FMPiano_C1_vl3.wav | TX81Z FM piano, low C | 4.0 | -21.0 | -8.1 | 1 |
| Bass 002.wav | VCSL: Electrophones/TX81Z/Clavisynth/Clavisynth_C1_vl3.wav | TX81Z Clavisynth, low C | 4.0 | -21.0 | -11.5 | 1 |
| Bass 003.wav | VCSL: Electrophones/TX81Z/Piano 1/Piano 1_E1_vl3.wav | TX81Z Piano 1, low E | 3.77 | -21.1 | -10.3 | 1 |
| Bass 004.wav | VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_C2_vl3_rr1.wav | grand piano, low C | 4.0 | -18.0 | -7.3 | 2 |
| Bass 005.wav | VCSL: Idiophones/Struck Idiophones/Marimba/Marimba_hit_Outrigger_F1_loud_01.wav |  | 2.67 | -18.0 | -5.6 | 2 |
| Bass 006.wav | VCSL: Membranophones/Struck Membranophones/Timpani 1/Hit/Timpani1_Hit_v4_rr1_Sum.wav |  | 2.98 | -18.0 | -6.7 | 2 |
| Bass 007.wav | VCSL: Aerophones/Lip Aerophones/Didgeridoo/Didgeridoo1_Short1_Main_rr2.wav |  | 0.35 | -18.0 | -4.6 | 2 |

## Cycle

| File | From | Notes |
|---|---|---|
| Cycle 001.wav | generated here | sine |
| Cycle 002.wav | generated here | triangle |
| Cycle 003.wav | generated here | saw |
| Cycle 004.wav | generated here | square |
| Cycle 005.wav | generated here | pulse, 25% |
| Cycle 006.wav | generated here | pulse, 10% |
| Cycle 007.wav | generated here | soft saw (1/n² harmonics) |
| Cycle 008.wav | generated here | hollow (odd harmonics forward) |
| Cycle 009.wav | generated here | organ (additive drawbars) |
| Cycle 010.wav | generated here | vowel 'ah' (formants at 730, 1090, 2440 Hz) |
| Cycle 011.wav | generated here | FM, 1:2, index 2 |
| Cycle 012.wav | generated here | FM, 1:3, index 3.5 |
| Cycle 013.wav | generated here | sine folded (sin of 3·sin) |
| Cycle 014.wav | generated here | hard-synced saw, ratio 2.6 |
| Cycle 015.wav | generated here | overdriven sine (tanh) |
| Cycle 016.wav | generated here | stepped sine (4 levels, bit-crushed) |
| Cycle 017.wav | VCSL: Chordophones/Zithers/Grand Piano, Steinway B/Sus/JHPiano_Sus_Close_C4_vl2_rr1.wav | one cycle of grand piano, C4 (262.5 Hz measured, periodicity 0.995) |
| Cycle 018.wav | VCSL: Chordophones/Zithers/Upright Piano, Yamaha/Sustains/Upright1_Sus_C3_vl3_rr1.wav | one cycle of upright piano, C3 (130.8 Hz measured, periodicity 0.998) |
| Cycle 019.wav | VCSL: Chordophones/Zithers/Harpsichord, English/Sustains/Normal/ZuckermannKitHarpsi_Normal_Sus_C3_rr1.wav | one cycle of harpsichord, C3 (131.1 Hz measured, periodicity 0.981) |
| Cycle 020.wav | VCSL: Electrophones/TX81Z/Clavisynth/Clavisynth_C3_vl2.wav | one cycle of TX81Z Clavisynth, C3 (130.9 Hz measured, periodicity 0.999) |
| Cycle 021.wav | VCSL: Electrophones/TX81Z/FM Piano/FMPiano_C3_vl2.wav | one cycle of TX81Z FM piano, C3 (130.9 Hz measured, periodicity 1.000) |
| Cycle 022.wav | VCSL: Electrophones/TX81Z/FM Piano/FMPiano_C2_vl2.wav | one cycle of TX81Z FM piano, low, C2 (65.5 Hz measured, periodicity 0.999) |
| Cycle 023.wav | VCSL: Electrophones/TX81Z/Piano 1/Piano 1_C3_vl2.wav | one cycle of TX81Z Piano 1, C3 (130.8 Hz measured, periodicity 1.000) |
| Cycle 024.wav | VCSL: Aerophones/Reed Aerophones/Tenor Saxophone/Non-Vibrato/BrettTenor_NV_Main_D3_vl2_rr1.wav | one cycle of tenor saxophone, D3 (146.8 Hz measured, periodicity 1.000) |
| Cycle 025.wav | VCSL: Aerophones/Edge-blown Aerophones/Baroque Tenor Recorder/Sustain/TenRecorder_Sus_C4_rr1_Main.wav | one cycle of tenor recorder, C4 (261.4 Hz measured, periodicity 0.950) |
| Cycle 026.wav | VCSL: Aerophones/Edge-blown Aerophones/Baroque Bass Recorder/Sustain/BassRecorder_Sus_C3_rr1_Main.wav | one cycle of bass recorder, C3 (131.1 Hz measured, periodicity 0.998) |
| Cycle 027.wav | VCSL: Aerophones/Edge-blown Aerophones/Pipe Organ/Loud/Rode_Man3Open_A2.wav | one cycle of pipe organ, loud, A2 (110.1 Hz measured, periodicity 0.987) |
| Cycle 028.wav | VCSL: Aerophones/Edge-blown Aerophones/Pipe Organ/Loud/Rode_Man3Open_C4.wav | one cycle of pipe organ, loud, C4 (261.4 Hz measured, periodicity 0.998) |
| Cycle 029.wav | VCSL: Aerophones/Edge-blown Aerophones/Pipe Organ/Quiet Pedal/NT5_PedalQuiet_C2_rr1.wav | one cycle of pipe organ, pedal, C2 (65.3 Hz measured, periodicity 0.999) |
| Cycle 030.wav | VCSL: Aerophones/Edge-blown Aerophones/Renaissance Organ/4'/RenOrgan_4foot_Room_C3_rr1.wav | one cycle of renaissance organ, 4', C3 (130.8 Hz measured, periodicity 1.000) |
| Cycle 031.wav | VCSL: Aerophones/Free Aerophones/Harmonica-Hohner-Super64/Sustains/Normal/Hohner-Super64_Normal _C4.wav | one cycle of harmonica, C4 (261.8 Hz measured, periodicity 0.951) |
| Cycle 032.wav | VCSL: Idiophones/Struck Idiophones/Vibraphone/Bowed/Vibes_bowed_A2_rr1_Main.wav | one cycle of bowed vibraphone, A2 (109.9 Hz measured, periodicity 0.978) |
| Cycle 033.wav | VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass3_A#4_Slow_1_Main.wav | one cycle of wine glass, A#4 (481.7 Hz measured, periodicity 0.998) |
| Cycle 034.wav | VCSL: Aerophones/Edge-blown Aerophones/Ocarina, Typical/Sustains/Sus/StdOcarina_Sus_A4.wav | one cycle of ocarina, A4 (442.2 Hz measured, periodicity 0.997) |
| Cycle 035.wav | VCSL: Chordophones/Zithers/Psaltery, Bowed and Plucked/LongBow/BowedPsaltery_C5_Main_LongBow_rr1.wav | one cycle of bowed psaltery, C5 (524.0 Hz measured, periodicity 0.985) |
| Cycle 036.wav | VCSL: Idiophones/Struck Idiophones/Hand Chimes/sus_C3_r01_main.wav | one cycle of hand chime, C3 (131.8 Hz measured, periodicity 1.000) |

## Voice

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Voice 001.wav | Kenney: kenney_voiceover-pack/Male/go.ogg | "go", male voice | 0.63 | -21.0 | -9.4 | 1 |
| Voice 002.wav | Kenney: kenney_voiceover-pack/Female/ready.ogg | "ready", female voice | 0.49 | -18.0 | -5.4 | 2 |
| Voice 003.wav | Kenney: kenney_voiceover-pack/Male/hold.ogg | "hold", male voice | 0.54 | -21.0 | -10.4 | 1 |
| Voice 004.wav | Kenney: kenney_voiceover-pack/Female/set.ogg | "set", female voice | 0.47 | -21.0 | -6.6 | 1 |
| Voice 005.wav | Kenney: kenney_voiceover-pack/Male/1.ogg | "one", male voice | 0.62 | -21.0 | -9.6 | 1 |
| Voice 006.wav | Kenney: kenney_voiceover-pack/Female/2.ogg | "two", female voice | 0.55 | -21.0 | -6.6 | 1 |
| Voice 007.wav | Kenney: kenney_voiceover-pack/Male/3.ogg | "three", male voice | 0.75 | -21.0 | -9.9 | 1 |
| Voice 008.wav | Kenney: kenney_voiceover-pack/Female/4.ogg | "four", female voice | 0.71 | -21.0 | -8.6 | 1 |

## Foley

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Foley 001.wav | Kenney: kenney_sci-fi-sounds/Audio/doorClose_001.ogg |  | 0.52 | -21.0 | -12.6 | 1 |
| Foley 002.wav | Kenney: kenney_impact-sounds/Audio/footstep_wood_002.ogg |  | 0.25 | -23.5 | -1.2 | 1 |
| Foley 003.wav | Kenney: kenney_impact-sounds/Audio/impactPunch_heavy_002.ogg |  | 0.45 | -21.0 | -3.9 | 1 |
| Foley 004.wav | Kenney: kenney_impact-sounds/Audio/impactPlank_medium_001.ogg |  | 0.5 | -21.1 | -2.7 | 1 |
| Foley 005.wav | Kenney: kenney_impact-sounds/Audio/impactGlass_light_002.ogg |  | 0.21 | -21.0 | -2.0 | 1 |
| Foley 006.wav | Kenney: kenney_impact-sounds/Audio/impactMining_004.ogg |  | 0.83 | -21.0 | -1.7 | 1 |
| Foley 007.wav | VCSL: Idiophones/Struck Idiophones/Slapstick/slapstick_rr1.wav |  | 0.34 | -22.4 | -1.2 | 2 |
| Foley 008.wav | VCSL: Idiophones/Struck Idiophones/Ratchet/Ratchet1_Fast_rr1_Mid.wav |  | 4.0 | -18.0 | -9.0 | 2 |
| Foley 009.wav | Kenney: kenney_casino-audio/Audio/dice-shake-1.ogg | dice, shaken | 1.48 | -21.8 | -1.2 | 2 |
| Foley 010.wav | Kenney: kenney_casino-audio/Audio/chips-stack-1.ogg | poker chips | 0.21 | -22.5 | -1.2 | 2 |
| Foley 011.wav | Kenney: kenney_casino-audio/Audio/card-place-1.ogg | playing card | 0.69 | -21.0 | -1.2 | 2 |
| Foley 012.wav | Kenney: kenney_rpg-audio/Audio/bookClose.ogg | book closing | 0.23 | -20.0 | -1.2 | 2 |
| Foley 013.wav | Kenney: kenney_rpg-audio/Audio/chop.ogg | chop | 0.24 | -18.0 | -1.5 | 2 |
| Foley 014.wav | Kenney: kenney_rpg-audio/Audio/cloth1.ogg | cloth | 0.64 | -18.4 | -1.2 | 2 |
| Foley 015.wav | Kenney: kenney_rpg-audio/Audio/handleCoins.ogg | coins | 0.72 | -20.1 | -1.2 | 2 |
| Foley 016.wav | Kenney: kenney_rpg-audio/Audio/drawKnife1.ogg | knife drawn | 0.24 | -18.0 | -4.3 | 2 |
| Foley 017.wav | Kenney: kenney_rpg-audio/Audio/metalLatch.ogg | latch | 0.25 | -18.9 | -1.2 | 2 |
| Foley 018.wav | Kenney: kenney_rpg-audio/Audio/creak1.ogg | creak | 0.6 | -18.0 | -6.1 | 2 |

## Toy

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Toy 001.wav | VCSL: Aerophones/Edge-blown Aerophones/Train Whistle, Toy/Main_TrainLow_Double-001.wav | toy train whistle | 2.44 | -18.0 | -8.2 | 2 |
| Toy 002.wav | VCSL: Aerophones/Edge-blown Aerophones/Train Whistle, Toy/Main_TrainMed_Short-001.wav | toy train whistle, short | 1.32 | -18.0 | -7.6 | 2 |
| Toy 003.wav | VCSL: Aerophones/Edge-blown Aerophones/Ball Whistle/Main_BallWhistle_Short-001.wav | ball whistle | 1.09 | -18.0 | -8.7 | 2 |
| Toy 004.wav | VCSL: Aerophones/Free Aerophones/Siren/Main_SirenWhistle-002.wav | siren whistle | 3.86 | -18.0 | -9.5 | 2 |
| Toy 005.wav | VCSL: Idiophones/Struck Idiophones/Flexatone/flexatone_slap1.wav | flexatone | 0.84 | -18.0 | -4.1 | 2 |
| Toy 006.wav | VCSL: Idiophones/Struck Idiophones/Flexatone/flexatone_fast.wav | flexatone, fast | 3.83 | -18.0 | -6.7 | 2 |
| Toy 007.wav | VCSL: Idiophones/Struck Idiophones/Vibraslap/Legacy/vibraslap_rr1.wav | vibraslap | 2.38 | -18.1 | -2.8 | 2 |

## Noise

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Noise 001.wav | VCSL: Membranophones/Other Membranophones/Ocean Drum/OceanDrum_Sus_1_Mid.wav |  | 4.0 | -18.0 | -10.4 | 2 |
| Noise 002.wav | VCSL: Idiophones/Struck Idiophones/Cabasa/Cabasa1_Rub_v2_rr1_Mid.wav |  | 0.25 | -18.0 | -5.1 | 2 |
| Noise 003.wav | VCSL: Idiophones/Struck Idiophones/Tambourine 1/Tamb1_Roll_v2_rr1_Mid.wav |  | 4.0 | -18.0 | -9.7 | 2 |
| Noise 004.wav | VCSL: Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_cresc_2s.wav |  | 4.0 | -18.0 | -10.4 | 2 |
| Noise 005.wav | Kenney: kenney_sci-fi-sounds/Audio/computerNoise_000.ogg |  | 4.0 | -21.0 | -15.9 | 1 |
| Noise 006.wav | Kenney: kenney_sci-fi-sounds/Audio/thrusterFire_000.ogg |  | 4.0 | -21.0 | -3.0 | 1 |
| Noise 007.wav | VCSL: Membranophones/Struck Membranophones/Timpani 1/Roll/Timpani1_Roll_v5_rr1_Sum.wav | timpani roll | 4.0 | -18.0 | -7.6 | 2 |
| Noise 008.wav | VCSL: Idiophones/Struck Idiophones/Sleigh Bells/sleighbell2_shake1.wav | sleigh bells, shaken | 4.0 | -18.0 | -7.9 | 2 |
| Noise 009.wav | VCSL: Idiophones/Struck Idiophones/Ratchet/Ratchet2_Slow_rr1_Mid.wav | ratchet, slow | 4.0 | -18.0 | -5.9 | 2 |

## Glitch

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Glitch 001.wav | Kenney: kenney_digital-audio/Audio/laser1.ogg |  | 0.96 | -21.0 | -9.4 | 1 |
| Glitch 002.wav | Kenney: kenney_digital-audio/Audio/phaserDown1.ogg |  | 0.4 | -18.0 | -7.6 | 2 |
| Glitch 003.wav | Kenney: kenney_digital-audio/Audio/zapThreeToneDown.ogg |  | 1.24 | -21.0 | -15.5 | 1 |
| Glitch 004.wav | Kenney: kenney_sci-fi-sounds/Audio/forceField_000.ogg |  | 0.88 | -21.0 | -13.1 | 1 |
| Glitch 005.wav | Kenney: kenney_digital-audio/Audio/spaceTrash1.ogg |  | 1.37 | -21.0 | -5.9 | 1 |
| Glitch 006.wav | Kenney: kenney_sci-fi-sounds/Audio/laserRetro_000.ogg |  | 0.24 | -21.0 | -12.5 | 1 |
| Glitch 007.wav | VCSL: Idiophones/Struck Idiophones/Claps/Clap_rr3.wav | a clap cut into a falling stutter | 0.45 | -18.0 | -1.5 | 2 |
| Glitch 008.wav | VCSL: Membranophones/Struck Membranophones/Snare Drum, Modern 1/Snare2_HitSN_v6_rr1_Mid.wav | a snare through a bit crusher | 0.42 | -18.0 | -1.2 | 2 |
| Glitch 009.wav | Kenney: kenney_ui-audio/Audio/switch22.ogg | switch | 0.36 | -22.3 | -1.2 | 2 |
| Glitch 010.wav | Kenney: kenney_digital-audio/Audio/pepSound4.ogg | blip | 0.49 | -21.1 | -11.9 | 1 |
| Glitch 011.wav | Kenney: kenney_sci-fi-sounds/Audio/laserSmall_004.ogg | small laser | 0.41 | -21.5 | -1.2 | 1 |

## Ambient

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Ambient 001.wav | VCSL: Idiophones/Struck Idiophones/Vibraphone/Bowed/Vibes_bowed_E3_rr1_Main.wav |  | 4.0 | -18.0 | -14.1 | 2 |
| Ambient 002.wav | VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass2_F#4_Slow_1_Main.wav |  | 4.0 | -18.0 | -14.2 | 2 |
| Ambient 003.wav | VCSL: Idiophones/Struck Idiophones/Gong 1/gong_f.wav |  | 4.0 | -18.0 | -9.6 | 2 |
| Ambient 004.wav | VCSL: Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_bow_13.wav |  | 4.0 | -18.0 | -10.2 | 2 |
| Ambient 005.wav | VCSL: Idiophones/Struck Idiophones/Hand Chimes/sus_C4_r01_main.wav |  | 4.0 | -18.0 | -15.5 | 2 |
| Ambient 006.wav | VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass1_D#4_Slow_1_Main.wav<br>VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass3_A#4_Slow_1_Main.wav<br>VCSL: Idiophones/Struck Idiophones/Vibraphone/Bowed/Vibes_bowed_G3_rr1_Main.wav | two wine glasses and a bowed vibraphone, layered | 4.0 | -18.0 | -9.4 | 2 |
| Ambient 007.wav | VCSL: Idiophones/Struck Idiophones/Mark Trees/Legacy/windchimes_desc1.wav | mark tree, falling | 4.0 | -18.0 | -9.7 | 2 |
| Ambient 008.wav | VCSL: Aerophones/Lip Aerophones/Didgeridoo/Didgeridoo1_Sus2_Main.wav | didgeridoo drone | 4.0 | -18.0 | -8.9 | 2 |
| Ambient 009.wav | VCSL: Chordophones/Zithers/Dan Tranh/FX/FX_01.wav | dan tranh, effect | 4.0 | -21.1 | -10.3 | 1 |

