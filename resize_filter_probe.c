/* resize_filter_probe: prints the zscale filter a scaled export would use.
 * Includes dv_export.c so the static filter builder is reachable; stubs the engine getters.
 * usage: resize_filter_probe   (reads ffmpeg from PATH; run by resize_filter.sh with fake ffmpegs) */
#include DV_EXPORT_SRC
#include <gdk/gdk.h>

void        dv_engine_get_size (DvEngine *e, int *w, int *h) { (void) e; *w = 64; *h = 64; }
gboolean    dv_engine_is_svg (DvEngine *e)      { (void) e; return FALSE; }
gboolean    dv_engine_is_animated (DvEngine *e) { (void) e; return FALSE; }
GdkTexture *dv_engine_get_current_texture (DvEngine *e) { (void) e; return NULL; }

int main (void)
{
    DvViewport vp;
    dv_viewport_init (&vp);
    vp.image_w = 64; vp.image_h = 64; vp.widget_w = 128; vp.widget_h = 128;
    vp.soft_mode = FALSE; vp.zoom = 2.0;
    for (int i = 0; i < 2; i++) {          /* twice: the probe must run once only */
        char *f = build_transform_filter (&vp, 64, 64);
        printf ("%s\n", f ? f : "(none)");
        g_free (f);
    }
    dv_viewport_free (&vp);
    return 0;
}
