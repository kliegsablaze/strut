# Strut

A drum instrument for Ableton Move, running in Schwung. It has sixteen pads,
each its own drum, built from three engines:

- **Skin**, a body that rings, from a tight kick to a clanging bar;
- **Wave**, an oscillator with bends, tables, FM and ring modulation;
- **Noise**, noise or a sample, played straight, resynthesised, or as its
  colour alone. Over 200 samples come with it, and you can add your own.

Each engine has one modulator: an envelope, LFO, random value or velocity,
sent to two of its knobs. Each pad has its own finish: level, pan, flam,
drive, crush, two shelves and a send to the room. The whole kit shares a
plate room, glue compression and warmth.

Six pages, the same for every pad: Pad, Skin, Wave, Noise, Finish and Kit.
Hit a pad to edit it.

- **Pad > SOUND** picks one of 40 starting sounds.
- **Finish > DICE** rolls a new sound for the pad. Turn it right for a new
  one, left to go back, up to eight steps.
- **Kit > DICE** rolls all sixteen pads, each by its place in the kit.
- **Kit > KIT** picks a factory kit. The factory kits are placeholders for
  now.

Rolls are level-matched and never silent. Kits save with the set. Module
Help on the device explains every page.

Needs Schwung 1.7.3 or later. See [DESIGN.md](DESIGN.md) for how it works and
why.

## Build and install

```bash
git clone --depth 1 https://github.com/charlesvestal/schwung .schwung
```

```bash
bash tests/run.sh
```

```bash
./scripts/build.sh
```

```bash
./scripts/install.sh
```

`install.sh` copies the build to `ableton@move.local` and restarts the Move.

## License

Strut is under the [PolyForm Strict License 1.0.0](LICENSE): you may use it
for any noncommercial purpose, but not distribute it or make changes or new
works based on it.

**The music is yours.** Music, recordings and performances you make by playing
Strut belong to you, and you may use, release and sell them however you like,
including commercially. The license covers the software, not the sounds you
make with it.

The Schwung plugin API header is MIT-licensed; see
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
