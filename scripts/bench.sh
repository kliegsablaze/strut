#!/usr/bin/env bash
# Runs tools/bench on a connected Move, alongside whatever the Move is doing:
# what Strut costs there. No files are left behind on the device.
#
#   scripts/bench.sh                 build the bench here (needs
#                                    aarch64-linux-gnu-gcc or Docker), then run it
#   scripts/bench.sh bench-aarch64   run one built elsewhere: the CI's
#                                    strut-bench artifact
set -euo pipefail

cd "$(dirname "$0")/.."

HOST="${MOVE_HOST:-ableton@move.local}"
IMAGE="${STRUT_BUILD_IMAGE:-forgetful-builder}"
BIN="${1:-}"

if [ -z "$BIN" ]; then
    BIN=build/bench-aarch64
    CC_CMD="aarch64-linux-gnu-gcc -std=c11 -O3 -ffp-contract=fast -static -Wall -Wextra -Werror -Isrc/dsp \
      tools/bench.c src/dsp/*.c -o $BIN -lm"
    mkdir -p build
    if command -v aarch64-linux-gnu-gcc >/dev/null 2>&1; then
        sh -c "$CC_CMD"
    else
        docker run --rm -v "$PWD:/build" -w /build "$IMAGE" sh -c "$CC_CMD"
    fi
fi

scp -q -o LogLevel=ERROR "$BIN" "$HOST:/data/UserData/strut-bench"
ssh -o LogLevel=ERROR "$HOST" "chmod 755 /data/UserData/strut-bench && /data/UserData/strut-bench; rm -f /data/UserData/strut-bench"
