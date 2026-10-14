#!/bin/bash
# browse_media.sh BIN -- the "Also browse audio and video files" option, in the real app.
# Default: next/previous take images only. Option on: they also take audio and video.
# Ctrl+Left and Ctrl+Right browse like Page Up and Page Down.
# Either way, a file named directly is shown (audio with no cover: the theme's music icon).
# needs: Xvfb, xdotool, ImageMagick (import), python3 with PIL, ffmpeg
. "$(dirname "$0")/uilib.sh"
BIN=$1
D=/tmp/bmtest; rm -rf $D; mkdir -p $D
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=1 -frames:v 1 $D/a_photo.jpg
ffmpeg -loglevel error -y -f lavfi -i sine=duration=1 -c:a libmp3lame $D/b_song.mp3
ffmpeg -loglevel error -y -f lavfi -i sine=duration=1 -i $D/a_photo.jpg -map 0:a -map 1:v -c:a libmp3lame -c:v copy \
  -id3v2_version 3 -disposition:v attached_pic $D/c_cover.mp3
ffmpeg -loglevel error -y -f lavfi -i color=c=red:size=320x240:rate=25:duration=3 -c:v mpeg4 $D/d_clip.mkv   # solid red: no green to confuse the icon check
ffmpeg -loglevel error -y -f lavfi -i testsrc=size=200x100:rate=1 -frames:v 1 $D/e_wide.png
echo "just text" > $D/f_note.txt
# a stored zip holding a film: libav reads it as that film, but it is an archive and must be skipped
ffmpeg -loglevel error -y -f lavfi -i color=c=red:size=160x120:rate=25:duration=2 -c:v mpeg2video -f mpeg $D/film.mpg
python3 -c "import zipfile; zipfile.ZipFile('$D/g_film.zip','w',zipfile.ZIP_STORED).write('$D/film.mpg','film.mpg')"
rm $D/film.mpg

green() { # count of bright green pixels on screen: the theme's music icon is green, the test pictures are not
  import -window root /tmp/bm_shot.png 2>/dev/null
  python3 -c "
from PIL import Image
raw=Image.open('/tmp/bm_shot.png').convert('RGB').tobytes()
print(sum(1 for i in range(0,len(raw),3) if raw[i+1]>170 and raw[i]<90 and raw[i+2]<150))"
}
run() { # $1=config dir  $2...=app arguments
  export XDG_CONFIG_HOME=$1; shift
  start_app $BIN "$@"
}
conf() { mkdir -p $1/dwarvenview; printf '[dwarvenview]\nbrowse_media=%s\n' $2 > $1/dwarvenview/dwarvenview.conf; }

start_xvfb

echo "-- default (images only)"
rm -rf /tmp/bmcfg_off; run /tmp/bmcfg_off $D/a_photo.jpg
t=$(step_key Page_Down); chk "images only: Page_Down from a_photo.jpg goes to e_wide.png (got $t)" "$([ "$t" = e_wide.png ] && echo 1 || echo 0)"
t=$(step_key Page_Down); chk "images only: Page_Down wraps to a_photo.jpg (got $t)" "$([ "$t" = a_photo.jpg ] && echo 1 || echo 0)"
kill $APP; sleep 1

echo "-- option on (g_film.zip, a stored zip of a film, must be skipped: the walk wraps from e_wide.png to a_photo.jpg)"
conf /tmp/bmcfg_on true; run /tmp/bmcfg_on $D/a_photo.jpg
for f in b_song.mp3 c_cover.mp3 d_clip.mkv e_wide.png a_photo.jpg; do
  t=$(step_key Page_Down); chk "media on: Page_Down reaches $f (got $t)" "$([ "$t" = $f ] && echo 1 || echo 0)"
done
kill $APP; sleep 1

echo "-- Ctrl+Left and Ctrl+Right browse like Page Up and Page Down"
run /tmp/bmcfg_off $D/a_photo.jpg
t=$(step_key ctrl+Right); chk "Ctrl+Right goes forward to e_wide.png (got $t)" "$([ "$t" = e_wide.png ] && echo 1 || echo 0)"
t=$(step_key ctrl+Left);  chk "Ctrl+Left goes back to a_photo.jpg (got $t)" "$([ "$t" = a_photo.jpg ] && echo 1 || echo 0)"
t=$(step_key ctrl+Left);  chk "Ctrl+Left wraps back to e_wide.png (got $t)" "$([ "$t" = e_wide.png ] && echo 1 || echo 0)"
focus_main; xdotool key Right; sleep 1; xdotool key Left; sleep 1
chk "plain Left and Right still pan, they do not change the file" "$([ "$(xdotool getwindowname $W)" = e_wide.png ] && echo 1 || echo 0)"
kill $APP; sleep 1

echo "-- opened directly, option off"
run /tmp/bmcfg_off $D/b_song.mp3; sleep 2
chk "direct open of audio with no cover: title is b_song.mp3" "$([ "$(xdotool getwindowname $W)" = b_song.mp3 ] && echo 1 || echo 0)"
G=$(green); chk "  and the music icon is on screen ($G green pixels)" "$([ $G -gt 3000 ] && echo 1 || echo 0)"
kill $APP; sleep 1
run /tmp/bmcfg_off $D/f_note.txt; sleep 2
G=$(green); chk "control: a text file shows no music icon ($G green pixels)" "$([ $G -lt 200 ] && echo 1 || echo 0)"
kill $APP; sleep 1
run /tmp/bmcfg_off $D/d_clip.mkv; sleep 2
G=$(green); chk "control: a video shows its frame, not the music icon ($G green pixels)" "$([ $G -lt 200 ] && echo 1 || echo 0)"
finish
