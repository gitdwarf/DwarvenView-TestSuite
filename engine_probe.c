/* engine_probe: load files in sequence through the real dv_engine.c and print
 * what the engine reports after each final callback. No display needed. */
#include <glib.h>
#include <gdk/gdk.h>
#include <stdio.h>
#include "dv_engine.h"

typedef struct { GMainLoop *loop; gboolean final; } Ctx;

static void
on_pass (GdkTexture *t, int pw, int nw, int nh, gboolean is_final, gpointer ud)
{
    (void) t; (void) pw; (void) nw; (void) nh;
    Ctx *c = ud;
    if (is_final) { c->final = TRUE; g_main_loop_quit (c->loop); }
}

static gboolean on_timeout (gpointer ud) { g_main_loop_quit (((Ctx *) ud)->loop); return G_SOURCE_REMOVE; }

int
main (int argc, char **argv)
{
    DvEngine *eng = dv_engine_new ();
    for (int i = 1; i < argc; i++) {
        Ctx c = { g_main_loop_new (NULL, FALSE), FALSE };
        guint to = g_timeout_add_seconds (15, on_timeout, &c);
        dv_engine_load (eng, argv[i], on_pass, &c, FALSE, 0.0);
        g_main_loop_run (c.loop);
        g_source_remove (to);
        printf ("%-18s final=%d failed=%d hdr=%d svg=%d\n", g_path_get_basename (argv[i]),
                c.final, dv_engine_is_failed (eng), dv_engine_is_hdr (eng), dv_engine_is_svg (eng));
        g_main_loop_unref (c.loop);
    }
    return 0;
}
