/* prefs_test: the browse_media preference defaults to off, is saved, and is read back.
 * Real dv_prefs.c; uses a throwaway config directory. */
#include "dv_prefs.h"
#include <stdio.h>
#include <stdlib.h>

static int checks = 0, fails = 0;
static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-60s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

int
main (void)
{
    char *dir = g_dir_make_tmp ("dvprefs-XXXXXX", NULL);
    g_setenv ("XDG_CONFIG_HOME", dir, TRUE);

    DvPrefs *p = dv_prefs_load ();
    check ("no config file: browse_media is off", !p->browse_media);

    p->browse_media = TRUE;
    dv_prefs_save (p);
    dv_prefs_free (p);
    p = dv_prefs_load ();
    check ("saved on: reads back on", p->browse_media);

    p->browse_media = FALSE;
    dv_prefs_save (p);
    dv_prefs_free (p);
    p = dv_prefs_load ();
    check ("saved off: reads back off", !p->browse_media);

    /* A config written before the option existed has no such key. */
    char *conf = g_build_filename (dir, "dwarvenview", "dwarvenview.conf", NULL);
    g_file_set_contents (conf, "[dwarvenview]\nctx_dynamic=true\n", -1, NULL);
    dv_prefs_free (p);
    p = dv_prefs_load ();
    check ("old config without the key: off, other keys still read", !p->browse_media && p->ctx_dynamic);
    dv_prefs_free (p);

    g_free (conf);
    g_free (dir);
    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
