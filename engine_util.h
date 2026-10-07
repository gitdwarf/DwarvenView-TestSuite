/* engine_util.h -- helpers shared by the engine tests. Include after dv_engine.h; needs a GLib main loop. */
#pragma once
#include <glib.h>
#include <gdk/gdk.h>
#include "dv_engine.h"

static gboolean quit_cb (gpointer l) { g_main_loop_quit (l); return G_SOURCE_REMOVE; }

/* Run the main loop for ms milliseconds (the engine delivers results and frames through it). */
static void
spin_ms (int ms)
{
    GMainLoop *l = g_main_loop_new (NULL, FALSE);
    g_timeout_add (ms, quit_cb, l);
    g_main_loop_run (l);
    g_main_loop_unref (l);
}

/* What a finished load reported. */
typedef struct {
    GMainLoop  *loop;
    gboolean    final;     /* the final callback arrived (FALSE: timed out) */
    gboolean    failed;    /* the engine says every decoder gave up */
    int         w, h;      /* native size */
    GdkTexture *tex;       /* the final texture, a reference to release; NULL if none */
} DvLoad;

static void
load_sync_cb (GdkTexture *t, int pw, int nw, int nh, gboolean is_final, gpointer ud)
{
    (void) pw;
    DvLoad *r = ud;
    if (!is_final) return;
    r->final = TRUE; r->w = nw; r->h = nh;
    if (t) r->tex = g_object_ref (t);
    g_main_loop_quit (r->loop);
}

/* Load one file and wait (up to 20 s) for its final callback. */
static void
load_sync (DvEngine *e, const char *path, DvLoad *r)
{
    *r = (DvLoad) { g_main_loop_new (NULL, FALSE), FALSE, FALSE, 0, 0, NULL };
    guint to = g_timeout_add_seconds (20, quit_cb, r->loop);
    dv_engine_load (e, path, load_sync_cb, r, FALSE, 0.0);
    g_main_loop_run (r->loop);
    g_source_remove (to);
    g_main_loop_unref (r->loop);
    r->loop = NULL;
    r->failed = r->final && dv_engine_is_failed (e);
}
