# Strut's sample library

98 short sounds for the Noise engine's sample mode, one folder per kind,
each file named for its kind and a number (`Kick 001.wav`).

Every sound comes from a source that puts it in the public domain, so the
library can ship with Strut and be used commercially, with no attribution
required:

| Source | Licence | Where |
|---|---|---|
| VCSL, the Versilian Community Sample Library (Versilian Studios) | CC0 1.0 | https://github.com/sgossner/VCSL |
| The Open Source Drum Kit (Real Music Media) | Public domain, in the author's words: "completely in the public domain" | https://github.com/crabacus/the-open-source-drumkit, http://www.kvraudio.com/forum/viewtopic.php?t=277132 |
| Kenney: Impact Sounds, Sci-fi Sounds, Digital Audio | CC0 1.0 (License.txt in each pack) | https://kenney.nl/assets/category:Audio |

Attribution is not required by any of them. It is given here only so every
file's origin can be traced.

## How they were made

- **Format:** 44.1 kHz, 16-bit WAV with triangular dither. A file whose two
  channels were the same is stored mono.
- **Length:** at most 4 s. The silence before a sound is cut, leaving 1 ms. The
  tail is cut once it has died below −66 dBFS. A sound cut at 4 s fades out
  over its last 0.6 s.
- **Loudness:** matched on the loudest 400 ms (EBU R128 momentary
  loudness) to −18 LUFS, with peaks no higher than −1 dBFS.
  - Short, sharp hits are allowed up to 4 dB of fast peak limiting to get
    there. A few stay up to 4 dB quieter, rather than be squashed further.
  - A mono file measures 3 dB lower than the same sound in stereo; it plays
    at the same level.
- **Chords:** the Keys chords are built here from single CC0 notes. Some
  notes are pitched by a semitone (by resampling), and some are strummed by a
  few milliseconds.
- **Our own processing:** two glitches are made here from CC0 hits:
  - Glitch 007, a clap cut into a falling stutter;
  - Glitch 008, a snare through a bit crusher.

  Their only source material is the listed CC0 files.

`loudness` below is the measured max momentary loudness in LUFS. `peak` is in
dBFS.

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

## Noise

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Noise 001.wav | VCSL: Membranophones/Other Membranophones/Ocean Drum/OceanDrum_Sus_1_Mid.wav |  | 4.0 | -18.0 | -10.4 | 2 |
| Noise 002.wav | VCSL: Idiophones/Struck Idiophones/Cabasa/Cabasa1_Rub_v2_rr1_Mid.wav |  | 0.25 | -18.0 | -5.1 | 2 |
| Noise 003.wav | VCSL: Idiophones/Struck Idiophones/Tambourine 1/Tamb1_Roll_v2_rr1_Mid.wav |  | 4.0 | -18.0 | -9.7 | 2 |
| Noise 004.wav | VCSL: Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_cresc_2s.wav |  | 4.0 | -18.0 | -10.4 | 2 |
| Noise 005.wav | Kenney: kenney_sci-fi-sounds/Audio/computerNoise_000.ogg |  | 4.0 | -21.0 | -15.9 | 1 |
| Noise 006.wav | Kenney: kenney_sci-fi-sounds/Audio/thrusterFire_000.ogg |  | 4.0 | -21.0 | -3.0 | 1 |

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

## Ambient

| File | From | Notes | s | LUFS | peak | ch |
|---|---|---|---|---|---|---|
| Ambient 001.wav | VCSL: Idiophones/Struck Idiophones/Vibraphone/Bowed/Vibes_bowed_E3_rr1_Main.wav |  | 4.0 | -18.0 | -14.1 | 2 |
| Ambient 002.wav | VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass2_F#4_Slow_1_Main.wav |  | 4.0 | -18.0 | -14.2 | 2 |
| Ambient 003.wav | VCSL: Idiophones/Struck Idiophones/Gong 1/gong_f.wav |  | 4.0 | -18.0 | -9.6 | 2 |
| Ambient 004.wav | VCSL: Idiophones/Struck Idiophones/Suspended Cymbal 1/susCymb1_bow_13.wav |  | 4.0 | -18.0 | -10.2 | 2 |
| Ambient 005.wav | VCSL: Idiophones/Struck Idiophones/Hand Chimes/sus_C4_r01_main.wav |  | 4.0 | -18.0 | -15.5 | 2 |
| Ambient 006.wav | VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass1_D#4_Slow_1_Main.wav<br>VCSL: Idiophones/Friction Idiophones/Wine Glasses/Sustains/Slow/glass3_A#4_Slow_1_Main.wav<br>VCSL: Idiophones/Struck Idiophones/Vibraphone/Bowed/Vibes_bowed_G3_rr1_Main.wav | two wine glasses and a bowed vibraphone, layered | 4.0 | -18.0 | -9.4 | 2 |

