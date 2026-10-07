/* engine_race: deterministic checks for load cancellation, stale-thread writes and shutdown.
 * usage: engine_race anim|free SLOW_FILE FAST_FILE
 *        engine_race stress FILE... (last file must load fast)
 *        engine_race play ANIMATED_FILE
 *  anim:   start SLOW, 60ms later load FAST, report engine state at once and 4s later
 *  free:   start SLOW, load FAST, dv_engine_free, wait 3s (a stray thread would touch freed memory)
 *  stress: 80 rapid loads across the files with random gaps, then free the engine
 *  play:   load one file, sample the displayed frame 12 times; an animation must show several
 * Build with -fsanitize=address and run with ASAN_OPTIONS=detect_leaks=0 (the harness itself leaks a main loop). */
#include <glib.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include <dirent.h>
#include "dv_engine.h"
#include "engine_util.h"

typedef struct { GMainLoop *loop; } Ctx;
static void on_pass (GdkTexture *t, int pw, int nw, int nh, gboolean fin, gpointer ud)
{ (void) t; (void) pw; (void) nw; (void) nh; if (fin) g_main_loop_quit (((Ctx *) ud)->loop); }
static int nthreads (void) { int n = 0; DIR *d = opendir ("/proc/self/task"); struct dirent *e; while ((e = readdir (d))) if (e->d_name[0] != '.') n++; closedir (d); return n; }
static void load_wait (DvEngine *e, const char *p)
{ Ctx c = { g_main_loop_new (NULL, FALSE) }; g_timeout_add_seconds (20, quit_cb, c.loop);
  dv_engine_load (e, p, on_pass, &c, FALSE, 0.0); g_main_loop_run (c.loop); }
static void report (DvEngine *e, const char *tag, int base)
{ int w, h; dv_engine_get_size (e, &w, &h);
  printf ("%-10s animated=%d size=%dx%d extra_threads=%d\n", tag, dv_engine_is_animated (e), w, h, nthreads () - base); fflush (stdout); }

int main (int argc, char **argv)
{
    if (argc < 3) return 2;
    int base = nthreads ();
    DvEngine *e = dv_engine_new ();
    if (!g_strcmp0 (argv[1], "play")) {
        /* load an animation, then sample the current frame: an advancing animation shows several distinct textures */
        load_wait (e, argv[2]);
        GHashTable *seen = g_hash_table_new (NULL, NULL);
        for (int i = 0; i < 12; i++) { g_hash_table_add (seen, dv_engine_get_frame (e)); spin_ms (100); }
        printf ("play: animated=%d distinct_frames_seen=%u\n", dv_engine_is_animated (e), g_hash_table_size (seen));
        return 0;
    }
    Ctx slow = { g_main_loop_new (NULL, FALSE) };
    dv_engine_load (e, argv[2], on_pass, &slow, FALSE, 0.0);
    spin_ms (60);
    load_wait (e, argv[3]);
    if (!g_strcmp0 (argv[1], "stress")) {
        /* argv[2..] = files; rapid round-robin switching with random gaps, then shutdown */
        for (int i = 0; i < 80; i++) {
            Ctx c = { g_main_loop_new (NULL, FALSE) };
            dv_engine_load (e, argv[2 + i % (argc - 2)], on_pass, &c, FALSE, 0.0);
            spin_ms (g_random_int_range (0, 25));
        }
        load_wait (e, argv[argc - 1]);
        dv_engine_free (e);
        g_usleep (1500000);
        printf ("stress done, no crash\n");
        return 0;
    }
    if (!g_strcmp0 (argv[1], "anim")) { report (e, "fast done", base); spin_ms (4000); report (e, "+4s", base); }
    else { dv_engine_free (e); printf ("freed, running main loop 500ms then waiting\n"); fflush (stdout); spin_ms (500); g_usleep (2000000); printf ("survived\n"); }
    return 0;
}
