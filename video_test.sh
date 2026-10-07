#!/bin/bash
# video_test.sh ENGINE_PROBE ENGINE_RACE [FIXTURE_DIR]
# A video or a cover-art file must show one picture without being read through.
# needs: ffmpeg; strace (optional, enables the "bytes read" check). make_fixtures.sh builds FIXTURE_DIR.
P=$1; R=$2; F=${3:-/tmp/props}
fail=0
check() { if [ "$2" = ok ]; then echo "$1: PASS"; else echo "$1: FAIL ($3)"; fail=1; fi; }

for f in video/clip.mkv video/clip.mp4 audio/with_cover.mp3 audio/with_cover.flac; do
    out=$(timeout 20 "$P" "$F/$f" 2>/dev/null | tail -1)
    [[ $out == *"final=1 failed=0"* ]] && check "loads $f" ok || check "loads $f" bad "$out"
done
for f in lookalike/stored_mpg.zip lookalike/stored_mpg.tar; do
    out=$(timeout 20 "$P" "$F/$f" 2>/dev/null | tail -1)
    [[ $out == *"failed=1"* ]] && check "archive holding a film is not decoded: $f" ok || check "archive holding a film is not decoded: $f" bad "$out"
done
out=$(timeout 20 "$P" "$F/media/tone.mpg" 2>/dev/null | tail -1)
[[ $out == *"final=1 failed=0"* ]] && check "control: the film itself still loads" ok || check "control: the film itself still loads" bad "$out"
out=$(timeout 20 "$R" play "$F/video/clip.mkv" 2>/dev/null | tail -1)
[[ $out == *"animated=0"* ]] && check "video is one frame, not an animation" ok || check "video is one frame, not an animation" bad "$out"

if command -v strace >/dev/null; then
    T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
    ffmpeg -loglevel error -y -f lavfi -i testsrc=size=1280x720:rate=25 -t 100 -c:v mpeg4 -b:v 6M "$T/big.mkv"
    ffmpeg -loglevel error -y -f lavfi -i sine=duration=1500 -i "$F/photo.jpg" -map 0:a -map 1:v -c:a libmp3lame \
        -b:a 128k -c:v copy -id3v2_version 3 -disposition:v attached_pic "$T/long.mp3"
    for f in big.mkv long.mp3; do
        size=$(stat -c %s "$T/$f")
        strace -f -e trace=read,pread64 -o "$T/st" timeout 20 "$P" "$T/$f" >/dev/null 2>&1
        got=$(awk -F'= ' '/read/ && $NF+0 >= 4096 {s += $NF} END {print s+0}' "$T/st")
        [ "$got" -lt $((size / 10)) ] && check "$f read $got of $size bytes" ok || check "$f read $got of $size bytes" bad "more than 10%"
    done
else
    echo "strace not found: bytes-read check skipped"
fi
exit $fail
