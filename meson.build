# Built only with -Dtests=true. Run: meson test -C build --print-errorlogs
# The C tests #include the source file under test so static helpers are reachable.

src_dir = meson.project_source_root() / 'src'
bash    = find_program('bash')

def_src = ['-DDV_DEBUG_SRC="@0@"'.format(src_dir / 'dv_debug.c'),
           '-DDV_PROPS_SRC="@0@"'.format(src_dir / 'dv_props.c'),
           '-DDV_DIALOG_SRC="@0@"'.format(src_dir / 'dv_props_dialog.c'),
           '-DDV_EXPORT_SRC="@0@"'.format(src_dir / 'dv_export.c'),
           '-DDV_PROPS_TIMEOUT_SECS=3']   # short watchdog so the hung-tool test is fast
inc = include_directories('..', '../src')

# ---- assertion tests (meson test) ----

engine_srcs = [src_dir / 'dv_engine.c', src_dir / 'dv_surface_tex.c', src_dir / 'dv_webp_anim.c', src_dir / 'dv_viewable.c', src_dir / 'dv_debug.c']

debug_test = executable('debug_test', 'debug_test.c',
  c_args : def_src, include_directories : inc, dependencies : dwarvenview_deps)
test('debug-log', debug_test)

props_test = executable('props_test', 'props_test.c',
  c_args : def_src, include_directories : inc, dependencies : dwarvenview_deps)
# Needs ffmpeg, exiftool, exiv2, mediainfo. make_fixtures.sh builds /tmp/props.
test('props-fixtures', bash, args : [files('make_fixtures.sh')], priority : 100,
     is_parallel : false)
test('props', props_test, args : ['/tmp/props'], is_parallel : false, timeout : 180)

viewable_test = executable('viewable_test', 'viewable_test.c',
  sources : [src_dir / 'dv_viewable.c', src_dir / 'dv_debug.c'],
  include_directories : inc, dependencies : dwarvenview_deps)
test('viewable', viewable_test, args : ['/tmp/props'], is_parallel : false)

prefs_test = executable('prefs_test', 'prefs_test.c',
  sources : [src_dir / 'dv_prefs.c'],
  include_directories : inc, dependencies : dwarvenview_deps)
test('prefs', prefs_test)

anim_test = executable('anim_test', 'anim_test.c', sources : engine_srcs,
  include_directories : inc, dependencies : dwarvenview_deps)
test('animation', anim_test, args : ['/tmp/props'], is_parallel : false, timeout : 60)

# These need a display.

xvfb = find_program('xvfb-run', required : false)
if xvfb.found()
  dialog_test = executable('dialog_test', 'dialog_test.c',
    sources : [src_dir / 'dv_props.c'],
    c_args : def_src, include_directories : inc, dependencies : dwarvenview_deps)
  test('props-dialog', xvfb, args : ['-a', dialog_test, '/tmp/props'],
       is_parallel : false, timeout : 120)

  audio_icon_test = executable('audio_icon_test', 'audio_icon_test.c',
    sources : [src_dir / 'dv_engine.c', src_dir / 'dv_themeicon.c',
               src_dir / 'dv_surface_tex.c', src_dir / 'dv_webp_anim.c', src_dir / 'dv_viewable.c', src_dir / 'dv_debug.c'],
    include_directories : inc, dependencies : dwarvenview_deps)
  test('audio-icon', xvfb, args : ['-a', audio_icon_test, '/tmp/props'],
       is_parallel : false, timeout : 60)
endif

# ---- tools that print results (run by hand, see README.md) ----

engine_probe = executable('engine_probe', 'engine_probe.c', sources : engine_srcs,
  include_directories : inc, dependencies : dwarvenview_deps)
engine_race = executable('engine_race', 'engine_race.c', sources : engine_srcs,
  include_directories : inc, dependencies : dwarvenview_deps)
test('video', bash, args : [files('video_test.sh'), engine_probe, engine_race, '/tmp/props'],
     is_parallel : false, timeout : 120)
executable('export_harness', 'harness.c',
  sources : [src_dir / 'dv_viewport.c', src_dir / 'dv_prefs.c'],
  c_args : def_src, include_directories : inc, dependencies : dwarvenview_deps)
