/* Harness: includes dv_export.c so static helpers are reachable, stubs the
 * four engine getters it needs, links the real viewport + prefs code.
 * Build twice: DV_EXPORT_SRC = original file vs patched file. */
#include DV_EXPORT_SRC
#include <gdk/gdk.h>
#include <sys/stat.h>
#include <stdlib.h>

static int        g_w = 64, g_h = 64;
static gboolean   g_svg, g_anim;
static GdkTexture *g_tex;

void        dv_engine_get_size (DvEngine *e, int *w, int *h) { (void) e; *w = g_w; *h = g_h; }
gboolean    dv_engine_is_svg (DvEngine *e)      { (void) e; return g_svg; }
gboolean    dv_engine_is_animated (DvEngine *e) { (void) e; return g_anim; }
GdkTexture *dv_engine_get_current_texture (DvEngine *e) { (void) e; return g_tex; }

static const char *workdir;
static int fails = 0;

static const DvExportFormat *
fmt_for (const char *ext)
{
    for (int i = 0; i < DV_EXPORT_NFORMATS; i++)
        if (strcmp (DV_EXPORT_FORMATS[i].ext, ext) == 0)
            return &DV_EXPORT_FORMATS[i];
    return NULL;
}

static long
fsize (const char *p)
{
    struct stat st;
    return stat (p, &st) == 0 ? (long) st.st_size : -1;
}

/* payload marker: relative path, so it lands in cwd = workdir */
static int
pwned (void)
{
    int r = g_file_test ("PWNED", G_FILE_TEST_EXISTS);
    g_remove ("PWNED");
    return r;
}

static void
run (const char *label, const char *src, const char *ext,
     gboolean svg, gboolean anim)
{
    g_svg = svg; g_anim = anim;
    DvViewport vp;
    dv_viewport_init (&vp);
    vp.image_w = g_w; vp.image_h = g_h; vp.widget_w = g_w; vp.widget_h = g_h;
    DvPrefs prefs = {0};
    prefs.fallback_save_dir = g_build_filename (workdir, "out", NULL);

    DvExportResult *r = dv_export (src, fmt_for (ext), NULL, &vp, &prefs);

    int  n = 0;
    long sz = -1;
    if (r->output_paths)
        for (; r->output_paths[n]; n++)
            if (n == 0) sz = fsize (r->output_paths[0]);
    int p = pwned ();
    printf ("%-32s ok=%d outputs=%-3d size0=%-6ld INJECTED=%d%s%s\n",
            label, r->ok, n, sz, p,
            r->ok ? "" : "  err=", r->ok ? "" : (r->error_msg ? r->error_msg : "?"));
    dv_export_result_free (r);
    dv_viewport_free (&vp);
    g_free (prefs.fallback_save_dir);
}

#ifdef HAVE_NEW_HELPERS
static void
check (const char *label, gboolean cond)
{
    printf ("%-52s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

static void
unit_tests (void)
{
    puts ("\n-- unit tests of new helpers --");

    /* U1: ffmpeg dies early (bad output dir); big stdin; must not SIGPIPE us */
    gsize big = 64u * 1024u * 1024u;
    guchar *buf = g_malloc0 (big);
    GPtrArray *a = ffmpeg_argv_new ();
    ffmpeg_argv_add (a, "-f", "rawvideo", "-pixel_format", "rgba",
                     "-video_size", "64x64", "-framerate", "1", "-i", "pipe:0",
                     "/nonexistent_dir_xyz/out.png", NULL);
    gboolean r1 = ffmpeg_run (a, buf, big);
    g_free (buf);
    check ("U1 early-exit ffmpeg + 64MB stdin: returns FALSE, survives", !r1);

    struct sigaction sa;
    sigaction (SIGPIPE, NULL, &sa);
    check ("U1b SIGPIPE disposition restored to default", sa.sa_handler == SIG_DFL);

    /* U2: garbage input file -> nonzero exit must give FALSE */
    char *junk = g_build_filename (workdir, "junk.bin", NULL);
    g_file_set_contents (junk, "not an image at all", -1, NULL);
    char *o2 = g_build_filename (workdir, "junk_out.png", NULL);
    a = ffmpeg_argv_new ();
    ffmpeg_argv_add_input (a, junk, NULL);
    ffmpeg_argv_add (a, o2, NULL);
    check ("U2 ffmpeg nonzero exit reported as FALSE", !ffmpeg_run (a, NULL, 0));

    /* U3: good stdin run with hostile output name */
    guchar *px = g_malloc0 (64 * 64 * 4);
    for (int i = 0; i < 64 * 64 * 4; i += 4) { px[i] = 200; px[i+3] = 255; }
    char *o3 = g_build_filename (workdir, "x'; touch PWNED; echo '.png", NULL);
    a = ffmpeg_argv_new ();
    ffmpeg_argv_add (a, "-f", "rawvideo", "-pixel_format", "rgba",
                     "-video_size", "64x64", "-framerate", "1", "-i", "pipe:0",
                     o3, NULL);
    gboolean r3 = ffmpeg_run (a, px, 64 * 64 * 4);
    check ("U3 stdin pipe + hostile output name: ok, file written",
           r3 && fsize (o3) > 0);
    check ("U3b no injection", !pwned ());
    g_free (px);

    /* U4: cross-device move (tmpfs -> disk), hostile name, content intact */
    char *s4 = g_strdup ("/dev/shm/dv_move_src_'q'.txt");
    char *d4 = g_build_filename (workdir, "moved ';touch PWNED;'.txt", NULL);
    g_file_set_contents (s4, "payload-1234", -1, NULL);
    gboolean r4 = move_file (s4, d4);
    char *c4 = NULL;
    g_file_get_contents (d4, &c4, NULL, NULL);
    check ("U4 cross-device move: ok, content intact, source gone",
           r4 && c4 && strcmp (c4, "payload-1234") == 0 &&
           !g_file_test (s4, G_FILE_TEST_EXISTS));
    check ("U4b no injection", !pwned ());

    /* U5: move into nonexistent dir fails cleanly */
    char *s5 = g_build_filename (workdir, "m5.txt", NULL);
    g_file_set_contents (s5, "x", -1, NULL);
    check ("U5 move to nonexistent dir returns FALSE",
           !move_file (s5, "/nonexistent_dir_xyz/m5.txt"));
}
#endif

int
main (int argc, char **argv)
{
    setvbuf (stdout, NULL, _IONBF, 0);
    if (argc < 2) return 2;
    workdir = argv[1];
    g_chdir (workdir);

    const char *H1 = "x'; touch PWNED; echo '";
    const char *H2 = "--help $(touch PWNED) `touch PWNED` \"q\" \\bs \xc3\xa9";

    /* ---- sources -------------------------------------------------------- */
    char *d = g_build_filename (workdir, "src", NULL);
    g_mkdir_with_parents (d, 0755);
    g_mkdir_with_parents ("out", 0755);

    const char *names[] = { "plain", H1, H2 };
    const char *tags[]  = { "plain", "H1", "H2" };
    char *png[3], *gif[3], *svg[3], *xcur[3];
    for (int i = 0; i < 3; i++) {
        png[i]  = g_strdup_printf ("%s/%s.png",  d, names[i]);
        gif[i]  = g_strdup_printf ("%s/%s.gif",  d, names[i]);
        svg[i]  = g_strdup_printf ("%s/%s.svg",  d, names[i]);
        xcur[i] = g_strdup_printf ("%s/%s.xcur", d, names[i]);
    }
    /* generate real media via ffmpeg using argv (independent of code under test) */
    for (int i = 0; i < 3; i++) {
        char *av1[] = { "ffmpeg", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                        "testsrc=size=64x64:rate=1", "-frames:v", "1", png[i], NULL };
        char *av2[] = { "ffmpeg", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                        "testsrc=size=64x64:rate=5", "-t", "1", gif[i], NULL };
        g_spawn_sync (NULL, av1, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
        g_spawn_sync (NULL, av2, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
        g_file_set_contents (svg[i],
            "<svg xmlns='http://www.w3.org/2000/svg' width='64' height='64'>"
            "<rect width='64' height='64' fill='teal'/></svg>", -1, NULL);
        g_file_set_contents (xcur[i], "Xcur\x10\0\0\0junk", 12, NULL);
    }
    guchar *px = g_malloc0 (64 * 64 * 4);
    for (int i = 0; i < 64 * 64 * 4; i += 4) { px[i+1] = 180; px[i+3] = 255; }
    g_tex = gdk_memory_texture_new (64, 64, GDK_MEMORY_R8G8B8A8,
                                    g_bytes_new (px, 64 * 64 * 4), 64 * 4);
    pwned ();   /* clear any stray marker from setup */

    /* ---- matrix ----------------------------------------------------------- */
    puts ("-- dv_export() matrix --");
    for (int i = 0; i < 3; i++) {
        char l[96];
        #define CASE(fmtstr, src, ext, svgf, anim) \
            g_snprintf (l, sizeof l, "%-5s " fmtstr, tags[i]); \
            run (l, src, ext, svgf, anim)
        CASE ("static png->jpg      [s3]",   png[i], "jpg",  FALSE, FALSE);
        CASE ("static png->webp     [s3]",   png[i], "webp", FALSE, FALSE);
        CASE ("anim gif->gif        [s4]",   gif[i], "gif",  FALSE, TRUE);
        CASE ("anim gif->webp       [s4]",   gif[i], "webp", FALSE, TRUE);
        CASE ("anim gif->apng       [s4]",   gif[i], "png",  FALSE, TRUE);
        CASE ("anim gif->jpg frames [s5,6]", gif[i], "jpg",  FALSE, TRUE);
        CASE ("svg->png             [s1]",   svg[i], "png",  TRUE,  FALSE);
        CASE ("svg->jpg             [s2]",   svg[i], "jpg",  TRUE,  FALSE);
        CASE ("xcursor->png         [s7]",   xcur[i], "png", FALSE, FALSE);
    }

#ifdef HAVE_NEW_HELPERS
    unit_tests ();
    printf ("\nunit failures: %d\n", fails);
    return fails ? 1 : 0;
#else
    return 0;
#endif
}
