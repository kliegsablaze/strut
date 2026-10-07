# Strut

A drum instrument for Ableton Move, running in Schwung: sixteen pads, each its
own drum, built from a resonator, an oscillator and a noise source.

**Work in progress.** This is a scaffold; see [DESIGN.md](DESIGN.md) for the
plan.

## Build and test

```bash
git clone --depth 1 https://github.com/charlesvestal/schwung .schwung
bash tests/run.sh
./scripts/build.sh
./scripts/install.sh     # to a Move at move.local
```

## License

[PolyForm Strict 1.0.0](LICENSE). The Schwung plugin API header is MIT; see
[THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).
