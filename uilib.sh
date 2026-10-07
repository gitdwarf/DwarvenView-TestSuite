# uilib.sh -- shared helpers for the scripts that drive the real binary under Xvfb. Source it, do not run it.
# needs: Xvfb, xdotool
export DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo NO_AT_BRIDGE=1
FAILS=0

start_xvfb() { # $1=WxH, default 1280x800
  pkill Xvfb 2>/dev/null; sleep 0.5; Xvfb :99 -screen 0 ${1:-1280x800}x24 >/dev/null 2>&1 & sleep 2
}

chk() { # $1=label $2=1 for pass
  if [ "$2" = 1 ]; then echo "PASS  $1"; else echo "FAIL  $1"; FAILS=$((FAILS+1)); fi
}

start_app() { # $1=binary $2...=its arguments. Sets APP (pid) and W (main window id).
  local bin=$1; shift
  $bin "$@" >/dev/null 2>&1 & APP=$!
  for i in $(seq 1 30); do W=$(xdotool search --onlyvisible --pid $APP 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.5; done
  xdotool windowfocus $W; sleep 2
}

focus_main() { # without a window manager nothing hands focus back, and the Properties window can retake it: poll until it sticks
  for i in $(seq 1 25); do xdotool windowfocus $W; sleep 0.2; [ "$(xdotool getwindowfocus)" = "$W" ] && return 0; done
  return 1
}

nav_key() { # $1=key $2=expected main-window title. Without a window manager the keypress can land on another window,
  # so refocus and resend until the main window really navigated.
  for a in 1 2 3 4 5 6; do
    focus_main; sleep 0.3; xdotool key $1
    for i in $(seq 1 10); do [ "$(xdotool getwindowname $W)" = "$2" ] && return 0; sleep 0.2; done
  done
  return 1
}

step_key() { # $1=key. Presses it once and prints the main window title it ended on. A key lost to the focus race is
  # resent only while the title has not changed at all, so a skipped file can never be walked through by repeating.
  local prev; prev=$(xdotool getwindowname $W)
  for a in 1 2 3 4 5 6; do
    focus_main; sleep 0.3; xdotool key $1
    for i in $(seq 1 10); do
      [ "$(xdotool getwindowname $W)" != "$prev" ] && { sleep 0.5; xdotool getwindowname $W; return 0; }
      sleep 0.2
    done
  done
  echo "$prev"; return 1
}

wait_title() { # $1=winid $2=expected $3=timeout_s
  for i in $(seq 1 $(($3*5))); do [ "$(xdotool getwindowname $1 2>/dev/null)" = "$2" ] && return 0; sleep 0.2; done
  return 1
}

finish() { kill $APP 2>/dev/null; pkill Xvfb; echo "FAILURES: $FAILS"; [ $FAILS = 0 ]; }
