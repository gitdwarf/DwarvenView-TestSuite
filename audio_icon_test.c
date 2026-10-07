/* audio_icon_test: audio with no cover art shows the theme's audio icon.
 * usage: audio_icon_test FIXTURE_DIR   (needs a display, e.g. xvfb-run; run make_fixtures.sh first)
 * Real dv_themeicon.c and dv_engine.c. */
#include "dv_themeicon.h"
#include "dv_engine.h"
#include "engine_util.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <glib/gstdio.h>

static int checks = 0, fails = 0;
static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-66s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

/* Load one file; returns whether the engine reported failure. */
static gboolean
load_failed (DvEngine *e, const char *path, int *w, int *h)
{
    DvLoad r;
    load_sync (e, path, &r);
    g_clear_object (&r.tex);
    if (w) *w = r.w;
    if (h) *h = r.h;
    return r.failed;
}

/* How many pixels are not fully transparent. */
static guint
opaque_pixels (GdkTexture *t)
{
    int w = gdk_texture_get_width (t), h = gdk_texture_get_height (t);
    guint8 *buf = g_malloc0 ((gsize) w * h * 4);
    gdk_texture_download (t, buf, (gsize) w * 4);
    guint n = 0;
    for (int i = 0; i < w * h; i++) if (buf[i * 4 + 3]) n++;
    g_free (buf);
    return n;
}

int
main (int argc, char **argv)
{
    if (argc < 2) return 2;
    gtk_init ();
    const char *fx = argv[1];

    GdkTexture *icon  = dv_theme_icon_texture ("audio-x-generic", 256);
    GdkTexture *other = dv_theme_icon_texture ("folder", 256);
    check ("theme audio icon exists", icon != NULL);
    check ("theme audio icon is 256x256", icon && gdk_texture_get_width (icon) == 256 && gdk_texture_get_height (icon) == 256);
    check ("theme audio icon is not blank", icon && opaque_pixels (icon) > 1000);
    check ("an icon the theme lacks gives NULL, not a placeholder", dv_theme_icon_texture ("no-such-icon-xyzzy", 64) == NULL);
    check ("different icons draw differently", icon && other && opaque_pixels (icon) != opaque_pixels (other));

    char *nocover = g_build_filename (fx, "audio", "no_cover.mp3", NULL);
    char *flacno  = g_build_filename (fx, "audio", "no_cover.flac", NULL);
    char *cover   = g_build_filename (fx, "audio", "with_cover.mp3", NULL);
    char *text    = g_build_filename (fx, "audio", "text.txt", NULL);
    int w, h;

    DvEngine *e = dv_engine_new ();
    check ("without an icon set, audio with no cover fails as before", load_failed (e, nocover, &w, &h));

    dv_engine_set_audio_icon (e, icon);
    check ("with the icon set, no_cover.mp3 loads", !load_failed (e, nocover, &w, &h));
    check ("  and is shown at the icon's size", w == 256 && h == 256);
    check ("with the icon set, no_cover.flac loads", !load_failed (e, flacno, &w, &h));
    check ("with the icon set, mp3 WITH a cover shows the cover, not the icon",
           !load_failed (e, cover, &w, &h) && w == 320 && h == 240);
    check ("with the icon set, a text file still fails", load_failed (e, text, &w, &h));

    dv_engine_set_audio_icon (e, NULL);
    check ("icon cleared: audio with no cover fails again", load_failed (e, nocover, &w, &h));

    dv_engine_free (e);

    /* The placeholder graphic must survive viewing an SVG (it was once freed on every SVG load). */
    char *svg = g_build_filename (fx, "t.svg", NULL);
    DvEngine *e2 = dv_engine_new ();
    dv_engine_init_placeholder (e2);
    DvLoad r;
    load_sync (e2, text, &r);
    check ("a failed load delivers the placeholder graphic", r.failed && r.tex != NULL);
    g_clear_object (&r.tex);
    load_sync (e2, svg, &r);
    check ("an SVG loads", r.final && !r.failed);
    g_clear_object (&r.tex);
    load_sync (e2, text, &r);
    check ("after viewing an SVG a failed load still delivers the placeholder", r.failed && r.tex != NULL);
    g_clear_object (&r.tex);
    /* An SVG that is corrupt, or parses but draws nothing, fails like any other bad file. */
    char *tmp = g_dir_make_tmp ("dvsvg-XXXXXX", NULL);
    char *empty = g_build_filename (tmp, "empty.svg", NULL), *broken = g_build_filename (tmp, "broken.svg", NULL),
         *good = g_build_filename (tmp, "good.svg", NULL);
    g_file_set_contents (empty, "<svg xmlns=\"http://www.w3.org/2000/svg\"\n viewBox=\"0 0 1024 687\">\nPNG\n\n</svg>\n", -1, NULL);
    g_file_set_contents (broken, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\"><rect width=\"10\" height=</svg", -1, NULL);
    g_file_set_contents (good, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"20\" height=\"10\"><rect width=\"20\" height=\"10\" fill=\"red\"/></svg>", -1, NULL);
    load_sync (e2, empty, &r);
    check ("an SVG that draws nothing fails and shows the placeholder", r.failed && r.tex != NULL && !dv_engine_is_svg (e2));
    g_clear_object (&r.tex);
    load_sync (e2, broken, &r);
    check ("a corrupt SVG fails and shows the placeholder", r.failed && r.tex != NULL && !dv_engine_is_svg (e2));
    g_clear_object (&r.tex);
    load_sync (e2, good, &r);
    check ("a real SVG still loads as an SVG (control)", r.final && !r.failed && dv_engine_is_svg (e2));
    g_clear_object (&r.tex);
    g_unlink (empty); g_unlink (broken); g_unlink (good); g_rmdir (tmp);
    g_free (empty); g_free (broken); g_free (good); g_free (tmp);
    dv_engine_free (e2);
    g_free (svg);

    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
