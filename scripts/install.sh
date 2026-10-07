#!/usr/bin/env bash
# Copies Strut to a connected Move. No restart: the synth picker rescans the
# modules folder each time it opens. Files are uploaded beside the target and
# renamed over it, never written in place (see Ragtag's install.sh for the
# SIGSEGV that writing over a dlopen()'d .so causes).
#
#   scripts/install.sh            install
#   scripts/install.sh --remove   delete Strut from the Move
#   scripts/install.sh strut-module.tar.gz
#                                 install a tarball built elsewhere (a CI
#                                 artifact or a release) instead of building
set -euo pipefail

cd "$(dirname "$0")/.."

HOST="${MOVE_HOST:-ableton@move.local}"
REMOTE_DIR="/data/UserData/schwung/modules/sound_generators/strut"

if [ "${1:-}" = "--remove" ]; then
    ssh "$HOST" "rm -rf '$REMOTE_DIR'"
    echo "Removed $REMOTE_DIR"
    exit 0
fi

if [ -n "${1:-}" ] && [ -f "$1" ]; then
    rm -rf dist && mkdir -p dist && tar -xzf "$1" -C dist
fi
[ -f dist/strut/dsp.so ] || ./scripts/build.sh
if ! head -c 20 dist/strut/dsp.so | od -An -tx1 |
     tr -d ' \n' | grep -Eq '^7f454c46.{28}b700'; then
    echo "dist/strut/dsp.so is not an aarch64 ELF" >&2
    exit 1
fi

ssh "$HOST" "mkdir -p '$REMOTE_DIR'"
scp -q dist/strut/dsp.so "$HOST:$REMOTE_DIR/.dsp.so.incoming"
scp -q dist/strut/module.json "$HOST:$REMOTE_DIR/.module.json.incoming"
scp -q dist/strut/help.json "$HOST:$REMOTE_DIR/.help.json.incoming"
ssh "$HOST" "cd '$REMOTE_DIR' && chmod 755 .dsp.so.incoming && \
    mv -f .dsp.so.incoming dsp.so && \
    mv -f .module.json.incoming module.json && \
    mv -f .help.json.incoming help.json && ls -l"
echo "Installed to $HOST:$REMOTE_DIR"
