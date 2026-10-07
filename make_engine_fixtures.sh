#!/bin/bash
# Fixtures for engine_probe.c and vg_repro.sh. Usage: make_engine_fixtures.sh OUTDIR
# hdr.png is PQ tagged (ffprobe must say color_transfer=smpte2084): the only way to test the HDR flag without HDR hardware.
D=${1:-/tmp/fx}; mkdir -p "$D"; cd "$D" || exit 1
cat > good.svg <<'X'
<svg xmlns="http://www.w3.org/2000/svg" width="200" height="120" viewBox="0 0 200 120"><rect width="200" height="120" fill="#2b5f8e"/><circle cx="100" cy="60" r="40" fill="#f0b75c"/></svg>
X
printf '<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><rect width="10" height=</svg' > bad.svg
echo "this is not an image" > not_an_image.png
ffmpeg -v error -y -f lavfi -i "color=c=gray:s=64x64" -frames:v 1 sdr.png
ffmpeg -v error -y -f lavfi -i "color=c=gray:s=64x64" -frames:v 1 -vf "format=rgb48be,setparams=color_trc=smpte2084:color_primaries=bt2020:colorspace=bt2020nc" hdr.png
ffprobe -v error -select_streams v -show_entries stream=color_transfer -of csv=p=0 hdr.png
