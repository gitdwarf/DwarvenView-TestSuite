#!/bin/bash
# browse_webp.sh BIN -- next and previous stop on WebP files (static and animated), in the real app.
# needs: Xvfb, xdotool, python3 with PIL (WebP support)
. "$(dirname "$0")/uilib.sh"
BIN=$1
D=/tmp/bwtest; rm -rf $D; mkdir -p $D
python3 -I -c "
from PIL import Image
fr = [Image.new('RGB', (64, 64), c) for c in ('red', 'green', 'blue')]
fr[0].save('$D/a_one.png'); fr[0].save('$D/b_still.webp')
fr[0].save('$D/c_moving.webp', save_all=True, append_images=fr[1:], duration=100, loop=0)
fr[0].save('$D/d_two.png')"
export XDG_CONFIG_HOME=$D/cfg
start_xvfb
start_app $BIN $D/a_one.png
t=$(step_key Page_Down); chk "Page_Down from a_one.png goes to b_still.webp (got $t)" "$([ "$t" = b_still.webp ] && echo 1 || echo 0)"
t=$(step_key Page_Down); chk "Page_Down goes to the animated c_moving.webp (got $t)" "$([ "$t" = c_moving.webp ] && echo 1 || echo 0)"
t=$(step_key Page_Down); chk "Page_Down goes on to d_two.png (got $t)" "$([ "$t" = d_two.png ] && echo 1 || echo 0)"
t=$(step_key Page_Up);   chk "Page_Up comes back to c_moving.webp (got $t)" "$([ "$t" = c_moving.webp ] && echo 1 || echo 0)"
finish
