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
CFLAGS="-std=c11 -O3 -ffp-contract=fast -Wall -Wextra -Werror -Isrc/dsp"

cc $CFLAGS tests/test_strut.c src/dsp/*.c -lm -lpthread -o "$out/test_strut"

rc=0
"$out/test_strut" "$out" || rc=$?

# The bench, the demo and the levels tool must keep building.
for t in bench demo levels; do
  cc $CFLAGS tools/$t.c src/dsp/*.c -lm -lpthread -o "$out/$t" || rc=1
done

if command -v node >/dev/null 2>&1 && [ -d "$SCHWUNG/src/shared/param_pages" ]; then
  node tests/plan.test.mjs "$out" "$SCHWUNG" || rc=$?
  node tests/widgets.test.mjs "$out" "$SCHWUNG" || rc=$?
else
  echo "FAIL: needs node and a Schwung checkout (git clone https://github.com/charlesvestal/schwung .schwung)"
  rc=1
fi

# Module Help draws 20 characters a line.
if command -v node >/dev/null 2>&1; then
  node -e 'const h=require("./src/help.json");const bad=h.children.flatMap(c=>c.lines.filter(l=>l.length>20));if(bad.length){console.log("FAIL: help.json lines over 20:",bad);process.exit(1)}' || rc=1
fi

# A synth slot always dlopens <module>/dsp.so, whatever module.json says.
if ! grep -q '"dsp": "dsp.so"' src/module.json; then
  echo "FAIL: module.json dsp must be dsp.so (the chain host loads that name)"; rc=1
fi

# The version Strut logs on load is the one the Move's module list shows.
if ! grep -q "\"version\": \"$(sed -n 's/.*STRUT_VERSION "\([^"]*\)".*/\1/p' src/dsp/strut.h)\"" src/module.json; then
  echo "FAIL: STRUT_VERSION in strut.h must match module.json's version"; rc=1
fi

exit $rc
