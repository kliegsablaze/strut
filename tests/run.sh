#!/usr/bin/env bash
# Builds Strut natively, drives it black-box through the v2 API, and plans its
# pages with the host's own planner.
#   SCHWUNG=<checkout of charlesvestal/schwung> bash tests/run.sh
set -euo pipefail

cd "$(dirname "$0")/.."

SCHWUNG="${SCHWUNG:-../schwung}"
[ -d "$SCHWUNG/src/shared/param_pages" ] || SCHWUNG=.schwung
out="build/tests"
mkdir -p "$out"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Werror -Isrc/dsp"

cc $CFLAGS tests/test_strut.c src/dsp/*.c -lm -o "$out/test_strut"

rc=0
"$out/test_strut" "$out" || rc=$?

# The bench and the demo must keep building.
for t in bench demo; do
  cc $CFLAGS tools/$t.c src/dsp/*.c -lm -o "$out/$t" || rc=1
done

if command -v node >/dev/null 2>&1 && [ -d "$SCHWUNG/src/shared/param_pages" ]; then
  node tests/plan.test.mjs "$out" "$SCHWUNG" || rc=$?
else
  echo "FAIL: needs node and a Schwung checkout (git clone https://github.com/charlesvestal/schwung .schwung)"
  rc=1
fi

# A synth slot always dlopens <module>/dsp.so, whatever module.json says.
if ! grep -q '"dsp": "dsp.so"' src/module.json; then
  echo "FAIL: module.json dsp must be dsp.so (the chain host loads that name)"; rc=1
fi

exit $rc
