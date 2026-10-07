# Tests

Everything here is optional and is not built by default. This directory comes from the separate `dwarvenview-tests-VERSION.zip`; unpack that archive into the source directory to get it.

```bash
meson setup build -Dtests=true
meson test -C build --print-errorlogs
```

Needs `ffmpeg`, `exiftool`, `exiv2`, `mediainfo`, `python3-mutagen` and `python3-pillow` (with WebP support); the fixtures are built with them. `xvfb-run` is needed for the display tests, `strace` is optional.

## `meson test`

| Test | What it checks |
|------|----------------|
| `debug-log` | `debug_test.c`. The debug log writes, flushes and survives a simulated crash (forked child killed with SIGKILL, with a control showing plain stdio loses data). |
| `props-fixtures` | `make_fixtures.sh`. Builds the sample files and fake tools in `/tmp/props`. |
| `props` | `props_test.c`. Properties parsers and the exiftool, exiv2, mediainfo, ffprobe, built-in chain, including failing, hung and hostile-name cases. |
| `viewable` | `viewable_test.c`. Which files next and previous stop on, with and without the media option: images always (ftyp brands told apart), audio and video only with the option, never empty, text, directory or missing files. PDFs, zip-based comics (cbz, epub), archives holding a film (stored zip or tar), and binaries that libav misreads as media stay out, while real flv, mpeg, wav, aac and the rest stay in. |
| `animation` | `anim_test.c`. Frames play with the delays the file gives (webp and gif, checked against the clock), a delay of 0 becomes 100 ms, colours are the right way round, static webp still works, and a webp with a broken header fails cleanly. |
| `prefs` | `prefs_test.c`. The media browsing option defaults to off, saves, and reads back; an old config without the key still loads. |
| `audio-icon` | `audio_icon_test.c`. The theme's music icon draws, and the engine shows it for audio with no cover (and not for a text file, or when no icon is set). Needs a display. |
| `video` | `video_test.sh`. A film or a cover-art file loads as one picture, is not played, and (with strace) is not read through. An archive holding a film is refused, not decoded. |
| `props-dialog` | `dialog_test.c`. The Properties window under Xvfb. |

The Properties watchdog is shortened to 3 seconds for these tests.

## Tools you run by hand

Built by the same option, in `build/tests/`.

| Tool | Use |
|------|-----|
| `engine_probe FILE...` | Loads files in order through the real engine and prints failed, HDR and SVG state after each. `make_engine_fixtures.sh DIR` makes the samples. |
| `engine_race MODE ...` | Load cancellation, shutdown and animation checks. Modes are in the file header. Run it under AddressSanitizer. |
| `export_harness` | Export filename and temp-path handling, including hostile names. |

## Scripts that drive the real binary

These take the path to a built `dwarvenview` and need Xvfb and xdotool. They share `uilib.sh` (starting Xvfb and the app, focus and key helpers). `keytest.sh`, `rotkeys.sh`, `propsui.sh` and `tiers.sh` also use ImageMagick (`import`) for screenshots.

| Script | Use |
|--------|-----|
| `keytest.sh BIN KEY...` | Key handling. |
| `rotkeys.sh BIN` | R, Shift+R, L, Shift+L, H and V. |
| `propsui.sh BIN IMAGE` | Ctrl+I behaviour. |
| `props_follow.sh BIN` | Properties follows the image when you navigate. |
| `prefs_ui.sh BIN` | The Preferences dialog on a 720p screen: it fits without scrolling, and an option ticked then saved is written to the config. |
| `browse_media.sh BIN` | The media option in the real app: images only by default, audio and video with it on, Ctrl+Left and Ctrl+Right browse like Page Up and Page Down, and a file named directly is always shown (the music icon for audio with no cover). Also needs `import`, python3 PIL. |
| `fail_placeholder.sh BIN` | A file that is not an image shows the DV placeholder graphic and the "Not a valid image file" text with the loading splash on, off, or delayed. Also needs `import`, python3 PIL. |
| `tiers.sh BIN IMAGE` | Each Properties tool tier in the real app. |
| `debug_ui.sh BIN` | The debug log checkbox in About. |
| `attack_export.sh HARNESS` | Symlinks planted at predictable temp paths must not be followed. |
| `vg_repro.sh TOOL IMAGE KEY...` | The real app under valgrind (set `BIN`). Check for invalid writes after navigating between images. |

## Lessons that shaped these

- Poll for the expected state, never sleep and hope.
- Every runtime check needs a positive control, or a pass proves nothing.
- Test a fix against the old code too: a test the old code also passes is vacuous.
- GLib mutexes are futex based, so TSan and helgrind are unreliable here. Use deterministic tests instead.
