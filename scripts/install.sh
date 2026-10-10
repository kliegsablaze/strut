#!/usr/bin/env bash
# Copies Strut to a connected Move, then restarts the Move. A slot that
# already holds Strut keeps the old code until then: the host opens the new
# dsp.so before closing the old, and dlopen() hands back the copy it already
# has. The restart is the host installer's own way: `reboot` as root. Files
# are uploaded beside the target and renamed over it, never written in place
# (see Ragtag's install.sh for the SIGSEGV that writing over a dlopen()'d .so
# causes).
#
#   scripts/install.sh            install
#   scripts/install.sh --remove   delete Strut from the Move
#   scripts/install.sh strut-module.tar.gz
#                                 install a tarball built elsewhere (a CI
#                                 artifact or a release) instead of building
#   STRUT_NO_RESTART=1 scripts/install.sh ...
#                                 install, and leave the restart to you
set -euo pipefail

cd "$(dirname "$0")/.."

HOST="${MOVE_HOST:-ableton@move.local}"
REMOTE_DIR="/data/UserData/schwung/modules/sound_generators/strut"

if [ "${1:-}" = "--remove" ]; then
    ssh -o LogLevel=ERROR "$HOST" "rm -rf '$REMOTE_DIR'"
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

ssh -o LogLevel=ERROR "$HOST" "mkdir -p '$REMOTE_DIR'"
scp -q -o LogLevel=ERROR dist/strut/dsp.so "$HOST:$REMOTE_DIR/.dsp.so.incoming"
scp -q -o LogLevel=ERROR dist/strut/module.json "$HOST:$REMOTE_DIR/.module.json.incoming"
scp -q -o LogLevel=ERROR dist/strut/help.json "$HOST:$REMOTE_DIR/.help.json.incoming"
scp -q -o LogLevel=ERROR dist/strut/canvas.js "$HOST:$REMOTE_DIR/.canvas.js.incoming"
ssh -o LogLevel=ERROR "$HOST" "cd '$REMOTE_DIR' && chmod 755 .dsp.so.incoming && \
    mv -f .dsp.so.incoming dsp.so && \
    mv -f .module.json.incoming module.json && \
    mv -f .help.json.incoming help.json && \
    mv -f .canvas.js.incoming canvas.js && ls -l"
version=$(sed -n 's/.*"version": *"\([^"]*\)".*/\1/p' dist/strut/module.json)
echo "Installed Strut $version to $HOST:$REMOTE_DIR"

# The sample library, 50 MB: sent only when it differs from the Move's
# (a stamp of its files' contents), into a folder beside it, then swapped in.
if [ -d dist/strut/samples ]; then
    stamp=$(cd dist/strut && find samples -type f ! -name .stamp -print0 | LC_ALL=C sort -z | xargs -0 cksum | cksum | cut -d' ' -f1)
    there=$(ssh -o LogLevel=ERROR "$HOST" "cat '$REMOTE_DIR/samples/.stamp' 2>/dev/null || true")
    if [ "$stamp" != "$there" ]; then
        echo "Copying the sample library ($(du -sh dist/strut/samples | cut -f1))..."
        t0=$(date +%s)
        tar -C dist/strut -cf - samples | ssh -o LogLevel=ERROR "$HOST" "cd '$REMOTE_DIR' && \
            rm -rf .samples.incoming && mkdir .samples.incoming && tar -xf - -C .samples.incoming && \
            echo $stamp > .samples.incoming/samples/.stamp && \
            rm -rf samples && mv .samples.incoming/samples samples && rmdir .samples.incoming"
        echo "Copied the sample library in $(( $(date +%s) - t0 )) s"
    else
        echo "The sample library on the Move is current"
    fi
fi

if [ -n "${STRUT_NO_RESTART:-}" ]; then
    echo "Restart the Move now: a slot already playing Strut keeps the old version until then."
    exit 0
fi

# Root login is what the host's own installer sets up. BatchMode: never stop
# to ask for a password; if the key is not there, say so and leave it.
ROOT_HOST="${MOVE_ROOT_HOST:-root@${HOST#*@}}"
LOG=/data/UserData/schwung/debug.log
loads=$(ssh -o LogLevel=ERROR "$HOST" "grep -ac 'strut $version loaded' $LOG 2>/dev/null || true")
echo "Restarting the Move..."
if ! ssh -o LogLevel=ERROR -o BatchMode=yes -o ConnectTimeout=5 "$ROOT_HOST" true 2>/dev/null; then
    echo "Could not log in as $ROOT_HOST to restart it."
    echo "Restart the Move by hand: a slot already playing Strut keeps the old version until then."
    exit 0
fi
# The connection may drop as it goes down, so its answer means nothing.
ssh -o LogLevel=ERROR -o BatchMode=yes -o ConnectTimeout=5 "$ROOT_HOST" reboot 2>/dev/null || true

# Wait for it to go down, then to come back (about 30 to 45 seconds).
sleep 10
for _ in $(seq 1 40); do
    ssh -o LogLevel=ERROR -o BatchMode=yes -o ConnectTimeout=3 "$HOST" true 2>/dev/null && break
    sleep 3
done
if ! ssh -o LogLevel=ERROR -o BatchMode=yes -o ConnectTimeout=3 "$HOST" true 2>/dev/null; then
    echo "The Move has not come back yet. Give it a minute, then play a pad."
    exit 0
fi

# Strut logs its version when a slot loads it; a saved set does that at boot.
for _ in $(seq 1 10); do
    now=$(ssh -o LogLevel=ERROR "$HOST" "grep -ac 'strut $version loaded' $LOG 2>/dev/null || true")
    if [ "${now:-0}" -gt "${loads:-0}" ]; then
        echo "The Move is back, playing Strut $version."
        exit 0
    fi
    sleep 3
done
echo "The Move is back. Strut $version starts when you load it into a slot."
