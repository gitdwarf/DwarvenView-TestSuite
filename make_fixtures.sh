#!/bin/bash
# Recreates the fixture set used by props_test.c / dialog_test.c / propsui.sh in /tmp/props.
# needs: ffmpeg (with libmp3lame, flac, aac, wmav2, libvorbis, libopus, libtheora, mpeg4, mpeg2video, flv1, h263), exiftool, exiv2, mediainfo, python3-mutagen, python3-pillow (with WebP support)
set -e
D=/tmp/props; rm -rf $D; mkdir -p $D/fake/{fail,error,badutf8,slow}; cd $D
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=1 -frames:v 1 photo.jpg
exiftool -q -overwrite_original -Make="Canon" -Model="EOS R5" -LensModel="RF24-70mm F2.8" \
  -DateTimeOriginal="2026:03:14 09:26:53" -ExposureTime=0.004 -FNumber=5.6 -ISO=400 -FocalLength=50 \
  -Orientation#=6 -Artist="D. Warf" -Copyright="(c) 2026 gitdwarf" \
  -GPSLatitude=31.9505 -GPSLatitudeRef=S -GPSLongitude=115.8605 -GPSLongitudeRef=E \
  -XMP:Title="Test title: with colon" -XMP:Description="Multi word : description" photo.jpg
cp photo.jpg "ünï cödé 'q' \$(x).jpg"; cp photo.jpg ./-h.jpg; head -c 300 photo.jpg > corrupt.jpg
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=64x64:rate=5 -t 1 anim.gif
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=100x50:rate=1 -frames:v 1 plain.png
exiftool -q -overwrite_original -Artist="Zoë 山田" -XMP:Title="日本語のタイトル" plain.png
printf '<svg xmlns="http://www.w3.org/2000/svg" width="64" height="32"><rect width="64" height="32"/></svg>' > t.svg
printf 'not an image' > junk.bin
# fake tools (absolute paths inside: the tests run with a restricted PATH)
printf '#!/bin/sh\nexit 1\n' > fake/fail/exiftool
printf '#!/bin/sh\necho "[ExifTool]      Error                           : Simulated failure"\nexit 0\n' > fake/error/exiftool
printf '#!/bin/sh\nprintf "[File]          Comment                         : bad \\377\\376 bytes\\n"\nexit 0\n' > fake/badutf8/exiftool
printf '#!/bin/sh\nexec /usr/bin/sleep 31415\n' > fake/slow/exiftool
chmod +x fake/*/exiftool
# audio with and without cover art, and hostile look-alikes (viewable_test.c)
mkdir -p $D/audio; cd $D/audio
# Cover art must be the real thing for each format, because ffmpeg's own "-disposition attached_pic"
# only makes a real cover for mp3, flac and m4a. mka uses -attach; wma, ogg and opus are tagged with mutagen.
mkc() { out=$1; shift; ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -i ../photo.jpg "$@" \
  -map 0:a -map 1:v -c:v copy -disposition:v attached_pic "$out"; }
mkc with_cover.mp3     -c:a libmp3lame -id3v2_version 3
mkc with_cover_v24.mp3 -c:a libmp3lame -id3v2_version 4
mkc with_cover.flac    -c:a flac
mkc with_cover.m4a     -c:a aac
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a flac -attach ../photo.jpg \
  -metadata:s:t mimetype=image/jpeg -metadata:s:t filename=cover.jpg with_cover.mka
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a wmav2 with_cover.wma
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a libvorbis with_cover.ogg
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a libopus with_cover.opus
python3 - <<'PY'
import base64, struct
from mutagen.asf import ASF, ASFByteArrayAttribute
from mutagen.oggvorbis import OggVorbis
from mutagen.oggopus import OggOpus
from mutagen.flac import Picture
jpg = open("../photo.jpg", "rb").read()
w = ASF("with_cover.wma")
w["WM/Picture"] = [ASFByteArrayAttribute(struct.pack("<bi", 3, len(jpg)) + "image/jpeg".encode("utf-16-le") + b"\0\0" + b"\0\0" + jpg)]
w.save()
pic = Picture(); pic.data = jpg; pic.type = 3; pic.mime = "image/jpeg"; pic.width = 320; pic.height = 240
b64 = base64.b64encode(pic.write()).decode()
for cls, name in ((OggVorbis, "with_cover.ogg"), (OggOpus, "with_cover.opus")):
    f = cls(name); f["metadata_block_picture"] = [b64]; f.save()
PY
# video: shown as one frame, never read through (viewable_test.c, engine_probe.c)
mkdir -p $D/video
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=25:duration=4 -f lavfi -i sine=duration=4 -c:v mpeg4 -c:a aac $D/video/clip.mkv
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=25:duration=4 -c:v mpeg4 $D/video/clip.mp4
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a libmp3lame no_cover.mp3
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a flac no_cover.flac
ffmpeg -loglevel error -y -f lavfi -i sine=frequency=440:duration=1 -c:a libmp3lame -metadata title=Hi tags_only.mp3
printf 'ID3\003\000\000\000\000\000\020' > fake_id3.mp3     # a header that promises a tag and then ends
: > empty.bin; echo "plain text" > text.txt
cp with_cover.mp3 "ünï & cövér.mp3"
cp with_cover.mp3 "concat:evil.mp3"                          # libav would read this as a protocol if given as a bare relative name
# animations with known frame delays (anim_test.c): red, blue, red, blue; 50, 200, 50, 200 ms
cd $D; mkdir -p anim
python3 - <<'PY'
from PIL import Image
RED, BLUE = (255, 0, 0, 255), (0, 0, 255, 255)
fr = [Image.new("RGBA", (64, 48), c) for c in (RED, BLUE, RED, BLUE)]
fr[0].save("anim/varied.webp", save_all=True, append_images=fr[1:], duration=[50, 200, 50, 200], loop=0, lossless=True)
g = [i.convert("P", palette=Image.ADAPTIVE) for i in fr]
g[0].save("anim/varied.gif", save_all=True, append_images=g[1:], duration=[50, 200, 50, 200], loop=0)
g[0].save("anim/fast.gif", save_all=True, append_images=g[1:], duration=[0, 0, 0, 0], loop=0)      # 0 means "unspecified"
fr[0].save("anim/static.webp", lossless=True)
PY
head -c 120 anim/varied.webp > anim/truncated.webp                                                   # animated header, no frames
# files libav misreads as media (PDF, zip-based comics, binaries) must not be browsable; real containers must be (viewable_test.c)
mkdir -p $D/lookalike $D/media
python3 - <<'PY'
import io, os, zipfile
from PIL import Image
Image.new("RGB", (200, 300), (200, 50, 50)).save("lookalike/doc.pdf")
for n in ("cbz", "zip", "epub"):
    with zipfile.ZipFile("lookalike/comic." + n, "w") as z:
        for i in range(3):
            b = io.BytesIO(); Image.new("RGB", (100, 150), (i * 80, 50, 50)).save(b, "JPEG"); z.writestr("p%d.jpg" % i, b.getvalue())
open("lookalike/comic.cbr", "wb").write(b"Rar!\x1a\x07\x00" + bytes(range(256)) * 40)
open("lookalike/blob.bin", "wb").write(bytes(range(256)) * 200)
open("lookalike/random.dat", "wb").write(os.urandom(50000))
PY
for spec in wav:"-vn" aac:"-vn" ac3:"-vn" aiff:"-vn" au:"-vn" caf:"-vn" w64:"-vn" wv:"-vn" mp2:"-vn" mka:"" \
            flv:"-c:v flv1 -c:a mp3" mpg:"-c:v mpeg2video -c:a mp2" ogv:"-c:v libtheora -c:a libvorbis" 3gp:"-c:v h263 -s 176x144 -c:a aac"; do
  ext=${spec%%:*}
  ffmpeg -loglevel error -y -f lavfi -i sine=d=1 -f lavfi -i testsrc=d=1:size=64x64 -t 1 ${spec#*:} $D/media/tone.$ext
done
ffmpeg -loglevel error -y -f lavfi -i testsrc=d=1:size=64x64:rate=25 -c:v mpeg2video -f mpegts $D/media/clip.ts
# an archive that holds a film is still an archive: libav reads a STORED zip or tar of an mpeg as that mpeg
python3 - <<'PY'
import shutil, tarfile, zipfile
with zipfile.ZipFile("lookalike/stored_mpg.zip", "w", zipfile.ZIP_STORED) as z: z.write("media/tone.mpg", "film.mpg")
shutil.copy("lookalike/stored_mpg.zip", "lookalike/stored_mpg.cbz")
with tarfile.open("lookalike/stored_mpg.tar", "w") as t: t.add("media/tone.mpg", "film.mpg")
open("lookalike/sig.7z", "wb").write(b"7z\xbc\xaf\x27\x1c" + bytes(300))
open("lookalike/sig.ar", "wb").write(b"!<arch>\n" + bytes(300))
PY
# ftyp boxes: an image brand is an image, any other brand is media (viewable_test.c)
printf '\000\000\000\030ftypavif\000\000\000\000avifmif1' > $D/ftyp_avif.bin
printf '\000\000\000\030ftypisom\000\000\002\000isomiso2' > $D/ftyp_isom.bin
echo "fixtures ready in $D"
