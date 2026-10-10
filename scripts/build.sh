#!/usr/bin/env bash
# Builds dsp.so for aarch64 (Ableton Move) into dist/strut/.
# Runs the cross compiler from the fleet's builder image unless it is on PATH.
set -euo pipefail

cd "$(dirname "$0")/.."

IMAGE="${STRUT_BUILD_IMAGE:-forgetful-builder}"
CC_CMD="aarch64-linux-gnu-gcc -std=c11 -g -O3 -ffp-contract=fast -shared -fPIC -Wall -Wextra -Werror \
  -Isrc/dsp src/dsp/*.c -o dist/strut/dsp.so -lm -lpthread"

rm -rf dist
mkdir -p dist/strut

if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    sh -c "$CC_CMD"
else
    docker run --rm -v "$PWD:/build" -w /build "$IMAGE" sh -c "$CC_CMD"
fi

# help.json is what puts "Module Help" one jog from the controls, and
# canvas.js draws the knob pictures.
cp src/module.json src/help.json src/canvas.js dist/strut/
cp LICENSE THIRD_PARTY_LICENSES.md dist/strut/
# the sample library, with SOURCES.md, which travels with it
cp -r src/samples dist/strut/
echo "Built dist/strut/"

cd dist
tar -czf strut-module.tar.gz strut/
cd ..
echo "Tarball: dist/strut-module.tar.gz"
