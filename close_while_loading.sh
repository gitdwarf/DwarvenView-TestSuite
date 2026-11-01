#!/bin/bash
# close_while_loading.sh ENGINE_RACE
# Closing the viewer while a big image is still decoding must not wait for the decode.
# Builds a large incompressible PNG, frees the engine mid-load, and checks that the free returned
# at once while the loader really was still running afterwards (the positive control).
R=$1; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
python3 -I -c "
import os,sys
from PIL import Image
Image.frombytes('RGB',(10000,10000),os.urandom(10000*10000*3)).save(sys.argv[1],compress_level=0)" "$T/big.png" || { echo "cannot build fixture"; exit 1; }
out=$(ASAN_OPTIONS=detect_leaks=0 timeout 120 "$R" closeslow "$T/big.png" 2>&1 | tail -1)
echo "$out"
free_ms=$(sed -n 's/.*free_ms=\([0-9]*\).*/\1/p' <<<"$out"); ran=$(sed -n 's/.*after=\([0-9]*\).*/\1/p' <<<"$out")
[ -n "$free_ms" ] && [ "$free_ms" -lt 200 ] && [ "$ran" -gt 200 ] && { echo "close while loading: PASS"; exit 0; }
echo "close while loading: FAIL"; exit 1
