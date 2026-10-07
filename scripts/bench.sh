#!/usr/bin/env bash
# Builds tools/bench for aarch64 and runs it on a connected Move, alongside
# whatever the Move is doing. No files are left behind on the device.
set -euo pipefail

cd "$(dirname "$0")/.."

HOST="${MOVE_HOST:-ableton@move.local}"
IMAGE="${STRUT_BUILD_IMAGE:-forgetful-builder}"
CC_CMD="aarch64-linux-gnu-gcc -std=c11 -O2 -Wall -Wextra -Werror -Isrc/dsp \
  tools/bench.c src/dsp/*.c -o build/bench-aarch64 -lm"

mkdir -p build
if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
    sh -c "$CC_CMD"
else
    docker run --rm -v "$PWD:/build" -w /build "$IMAGE" sh -c "$CC_CMD"
fi

scp -q build/bench-aarch64 "$HOST:/data/UserData/strut-bench"
ssh "$HOST" "/data/UserData/strut-bench; rm -f /data/UserData/strut-bench"
