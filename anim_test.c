/* anim_test: animations play with their own frame delays, and animated WebP (which libav cannot decode) works.
 * usage: anim_test FIXTURE_DIR   (run make_fixtures.sh first). Real dv_engine.c and dv_webp_anim.c. */
#include "dv_engine.h"
#include "engine_util.h"
#include <stdio.h>

static int checks = 0, fails = 0;
static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-68s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

/* Colour of the top-left pixel of a texture as 0xRRGGBB. */
static guint
corner (GdkTexture *t)
{
    guint8 px[4 * 64 * 48] = {0};
    if (gdk_texture_get_width (t) != 64) return 0xffffffff;
    gdk_texture_download (t, px, 64 * 4);          /* B8G8R8A8 premultiplied */
    return (px[2] << 16) | (px[1] << 8) | px[0];
}

/* The delays the engine holds for the current load, as "50,200,50,200". */
static char *
delays_of (DvEngine *e)
{
    GString *s = g_string_new (NULL);
    for (guint i = 0; dv_engine_frame_delay_ms (e, i); i++)
        g_string_append_printf (s, "%s%u", i ? "," : "", dv_engine_frame_delay_ms (e, i));
    return g_string_free (s, FALSE);
}

/* How many times the displayed frame changes in ms milliseconds. */
static int
frame_changes (DvEngine *e, int ms)
{
    int n = 0;
    GdkTexture *prev = dv_engine_get_frame (e);
    for (int t = 0; t < ms; t += 5) {
        spin_ms (5);
        GdkTexture *cur = dv_engine_get_frame (e);
        if (cur != prev) { n++; prev = cur; }
    }
    return n;
}

int
main (int argc, char **argv)
{
    if (argc < 2) return 2;
    char *wp = g_build_filename (argv[1], "anim", "varied.webp", NULL);
    char *gf = g_build_filename (argv[1], "anim", "varied.gif", NULL);
    char *fg = g_build_filename (argv[1], "anim", "fast.gif", NULL);
    char *sw = g_build_filename (argv[1], "anim", "static.webp", NULL);
    char *tw = g_build_filename (argv[1], "anim", "truncated.webp", NULL);
    DvEngine *e = dv_engine_new ();
    DvLoad r;
    char *d;

    load_sync (e, wp, &r);
    check ("animated webp loads", r.final && !r.failed);
    check ("  and is animated", dv_engine_is_animated (e));
    check ("  at the file's size", r.w == 64 && r.h == 48);
    check ("  first frame is red, so the colour channels are the right way round", r.tex && corner (r.tex) == 0xff0000);
    g_clear_object (&r.tex);
    d = delays_of (e);
    check ("  frame delays are the file's: 50,200,50,200", !g_strcmp0 (d, "50,200,50,200"));
    g_free (d);
    int n = frame_changes (e, 3000);
    check ("  plays at the file's pace (6 cycles of 4 changes: 22 to 26)", n >= 22 && n <= 26);
    printf ("    changes in 3 s: %d\n", n);

    load_sync (e, gf, &r);
    g_clear_object (&r.tex);
    check ("gif loads and is animated", r.final && !r.failed && dv_engine_is_animated (e));
    d = delays_of (e);
    check ("  frame delays are the file's: 50,200,50,200", !g_strcmp0 (d, "50,200,50,200"));
    g_free (d);
    n = frame_changes (e, 3000);
    check ("  plays at the file's pace (22 to 26 changes)", n >= 22 && n <= 26);
    printf ("    changes in 3 s: %d\n", n);

    load_sync (e, fg, &r);
    g_clear_object (&r.tex);
    d = delays_of (e);
    check ("gif with delay 0 gets the browsers' 100 ms", !g_strcmp0 (d, "100,100,100,100"));
    g_free (d);

    load_sync (e, sw, &r);
    check ("static webp still loads, not animated", r.final && !r.failed && !dv_engine_is_animated (e));
    g_clear_object (&r.tex);

    load_sync (e, tw, &r);
    check ("webp that claims animation but has no frames fails cleanly", r.final && r.failed);

    dv_engine_free (e);
    g_free (wp); g_free (gf); g_free (fg); g_free (sw); g_free (tw);
    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
