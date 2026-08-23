#!/bin/sh
#
# Build and run the test harnesses.
# tollama/tasync expect an Ollama server on
# :11434 with llama3.2 pulled.
#
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mktemp -d)"

trap 'rm -rf "$OUT"' EXIT

LIBS="-lcurl -lSDL2"

echo "== thist: history round-trip =="

gcc -Wall -Wextra -O2 -I "$ROOT" \
    "$ROOT/tests/thist.c" \
    "$ROOT/history.c" \
    -o "$OUT/thist"

(cd "$OUT" && ./thist)

echo "== tollama: blocking client =="

gcc -Wall -Wextra -O2 -I "$ROOT" \
    "$ROOT/tests/tollama.c" \
    "$ROOT/ollama.c" \
    -o "$OUT/tollama" $LIBS

"$OUT/tollama"

SDL_CFLAGS="$(sdl2-config --cflags)"

echo "== tasync: begin/poll loop =="

# shellcheck disable=SC2086
gcc -Wall -Wextra -O2 -I "$ROOT" \
    $SDL_CFLAGS \
    "$ROOT/tests/tasync.c" \
    "$ROOT/ollama.c" \
    -o "$OUT/tasync" $LIBS

"$OUT/tasync"

echo "== tpull: model installer =="

# shellcheck disable=SC2086
gcc -Wall -Wextra -O2 -I "$ROOT" \
    $SDL_CFLAGS \
    "$ROOT/tests/tpull.c" \
    "$ROOT/ollama.c" \
    -o "$OUT/tpull" $LIBS

"$OUT/tpull"

echo "== torb: orb edge quality =="

gcc -Wall -Wextra -O2 -I "$ROOT" \
    "$ROOT/tests/torb.c" \
    -o "$OUT/torb" $LIBS -lm

"$OUT/torb" | tail -2
