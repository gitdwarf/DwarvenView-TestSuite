#!/bin/bash
# resize_filter.sh PROBE
# Scaled exports use zscale spline64 when ffmpeg accepts it, spline36 otherwise (ffmpeg missing, or
# zscale rejects spline64), and ask ffmpeg only once. Uses fake ffmpegs on PATH, plus the real one.
PROBE=$1; T=$(mktemp -d); trap 'rm -rf "$T"' EXIT; fail=0
check() { if [ "$2" = ok ]; then echo "$1: PASS"; else echo "$1: FAIL ($3)"; fail=1; fi; }
fake() { mkdir -p "$T/$1"; printf '#!/bin/bash\necho "$*" >> %s/%s.log\nexit %s\n' "$T" "$1" "$2" > "$T/$1/ffmpeg"; chmod +x "$T/$1/ffmpeg"; }
fake new 0; fake old 1
mkdir "$T/none"
run() { PATH="$1:/usr/bin:/bin" "$PROBE" | head -1; }

out=$(run "$T/new"); [[ $out == *"filter=spline64:"* ]] && check "ffmpeg accepts spline64" ok || check "ffmpeg accepts spline64" bad "$out"
out=$(run "$T/old"); [[ $out == *"filter=spline36:"* ]] && check "ffmpeg rejects spline64: spline36" ok || check "ffmpeg rejects spline64: spline36" bad "$out"
# no ffmpeg first on PATH and none after it: use a PATH holding only the probe's own dir
out=$(env PATH="$T/none" "$PROBE" | head -1); [[ $out == *"filter=spline36:"* ]] && check "no ffmpeg: spline36" ok || check "no ffmpeg: spline36" bad "$out"
n=$(wc -l < "$T/new.log"); [ "$n" = 1 ] && check "asked ffmpeg once for two filters" ok || check "asked ffmpeg once for two filters" bad "$n calls"
grep -q "zscale=w=32:h=18:filter=spline64" "$T/new.log" && check "probe asks zscale for spline64" ok || check "probe asks zscale for spline64" bad "$(cat "$T/new.log")"
if command -v ffmpeg >/dev/null; then
    want=spline36; ffmpeg -nostdin -hide_banner -v error -f lavfi -i "color=c=gray:s=64x36,format=yuv420p10le" -frames:v 1 \
        -vf "zscale=w=32:h=18:filter=spline64" -f null - >/dev/null 2>&1 && want=spline64
    out=$("$PROBE" | head -1); [[ $out == *"filter=$want:"* ]] && check "real ffmpeg agrees with its own answer ($want)" ok || check "real ffmpeg agrees with its own answer ($want)" bad "$out"
else echo "real ffmpeg not found: skipped"; fi
exit $fail
