/* Harness for dv_props.c. Includes the source so static parsers are reachable.
 * usage: props_test FIXTURE_DIR
 * Scenarios restrict PATH to exactly the tools they allow. */
#include DV_PROPS_SRC
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <glib/gstdio.h>

static const char *fx;          /* fixture dir */
static char       *orig_path;   /* real PATH */
static int         fails = 0, checks = 0;
static int         bin_n = 0;

static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-66s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

/* PATH = [fake_dir:] dir containing symlinks to the listed real tools. */
static void
use_tools (const char *csv, const char *fake_dir)
{
    char *dir = g_strdup_printf ("/tmp/pt_bin_%d_%d", (int) getpid (), bin_n++);
    g_mkdir_with_parents (dir, 0755);
    gchar **names = g_strsplit (csv, ",", -1);
    g_setenv ("PATH", orig_path, TRUE);
    for (gchar **n = names; *n; n++) {
        if (!**n) continue;
        char *real = g_find_program_in_path (*n);
        if (real) {
            char *link = g_build_filename (dir, *n, NULL);
            if (symlink (real, link) != 0) perror ("symlink");
            g_free (link);
        }
        g_free (real);
    }
    g_strfreev (names);
    char *path = fake_dir ? g_strdup_printf ("%s:%s", fake_dir, dir) : g_strdup (dir);
    g_setenv ("PATH", path, TRUE);
    g_free (path);
    g_free (dir);
}

static char *
fxp (const char *name) { return g_build_filename (fx, name, NULL); }

static const char *
row (const DvProps *p, const char *key)
{
    for (guint i = 0; i < p->groups->len; i++) {
        DvPropGroup *g = g_ptr_array_index (p->groups, i);
        for (guint j = 0; j < g->rows->len; j++) {
            DvPropRow *r = g_ptr_array_index (g->rows, j);
            if (strcmp (r->key, key) == 0) return r->value;
        }
    }
    return NULL;
}

static gboolean
has_group (const DvProps *p, const char *name)
{
    for (guint i = 0; i < p->groups->len; i++)
        if (strcmp (((DvPropGroup *) g_ptr_array_index (p->groups, i))->name, name) == 0)
            return TRUE;
    return FALSE;
}

static DvProps *
fetch (const char *file) { char *f = fxp (file); DvProps *p = dv_props_fetch_sync (f); g_free (f); return p; }

static gboolean
eq (const char *a, const char *b) { return a && b && strcmp (a, b) == 0; }

/* ---- async / timeout ---- */
static GMainLoop *loop;
static DvProps   *async_result;
static gint64     async_t0, async_ms;
static void
async_done (GObject *o, GAsyncResult *res, gpointer d)
{
    (void) o; (void) d;
    async_result = dv_props_fetch_finish (res);
    async_ms = (g_get_monotonic_time () - async_t0) / 1000;
    g_main_loop_quit (loop);
}

int
main (int argc, char **argv)
{
    if (argc < 2) return 2;
    fx = argv[1];
    orig_path = g_strdup (g_getenv ("PATH"));
    DvProps *p;

    puts ("-- parsers on hand-written edge cases --");
    {
        DvProps *t = props_new ("/x", "t");
        gboolean ok = parse_exiftool (
            "[ExifTool]      ExifTool Version Number         : 12.76\n"
            "[XMP-dc]        Description                     : Multi word : description\n"
            "[IFD0]          Make                            : Canon\n"
            "[IFD0]          Empty                           : \n", t);
        check ("exiftool: parsed usable", ok);
        check ("exiftool: value containing ' : ' kept whole",
               eq (row (t, "Description"), "Multi word : description"));
        check ("exiftool: tool's own version row dropped", row (t, "ExifTool Version Number") == NULL);
        check ("exiftool: empty value dropped", row (t, "Empty") == NULL);
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("exiftool: Error row makes result unusable",
               !parse_exiftool ("[ExifTool]      Error                           : File not found\n", t));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("exiftool: Warning surfaced in its own group",
               parse_exiftool ("[ExifTool]      Warning                         : JPEG format error\n"
                               "[File]          File Type                       : JPEG\n", t)
               && eq (row (t, "Warning"), "JPEG format error"));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("mediainfo: name+size only is NOT usable (no Format row)",
               !parse_mediainfo ("General\nComplete name  : junk.bin\nFile size  : 12 Bytes\n\n", t));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("mediainfo: section with Format row is usable",
               parse_mediainfo ("General\nFormat   : GIF\n\nImage\nWidth   : 64 pixels\n\n", t)
               && has_group (t, "Image") && eq (row (t, "Width"), "64 pixels"));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("ffprobe: N/A and zero DISPOSITION dropped, tags kept",
               parse_ffprobe ("[STREAM]\nindex=0\ncodec_type=video\nbit_rate=N/A\n"
                              "DISPOSITION:default=0\nDISPOSITION:forced=1\nTAG:title=Hello\n[/STREAM]\n", t)
               && row (t, "bit rate") == NULL && row (t, "DISPOSITION default") == NULL
               && eq (row (t, "DISPOSITION forced"), "1") && eq (row (t, "title"), "Hello"));
        check ("ffprobe: stream group named from index and type", has_group (t, "Stream 0 (video)"));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("ffprobe: empty output not usable", !parse_ffprobe ("", t));
        dv_props_free (t);
    }

    puts ("\n-- parser: exiv2 --");
    {
        DvProps *t = props_new ("/x", "t");
        gboolean ok = parse_exiv2 (
            "Exif.Image.Make                              Ascii      12  Dwarven Cam\n"
            "Exif.Photo.ISOSpeedRatings                   Short       1  400\n"
            "Xmp.dc.title                                 LangAlt     1  lang=\"x-default\" A : B\n", t);
        check ("exiv2: parsed usable", ok);
        check ("exiv2: value with spaces kept whole", eq (row (t, "Make"), "Dwarven Cam"));
        check ("exiv2: grouped by Family.Group", has_group (t, "Exif.Image") && has_group (t, "Exif.Photo") && has_group (t, "Xmp.dc"));
        check ("exiv2: value containing ' : ' kept whole", row (t, "title") && strstr (row (t, "title"), "A : B"));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("exiv2: IFD pointer tags dropped, real tags kept",
               parse_exiv2 ("Exif.Image.ExifTag Long 1  202\nExif.Image.GPSTag Long 1  388\n"
                            "Exif.Photo.InteroperabilityTag Long 1  9\nExif.Image.Make Ascii 6  Canon\n", t)
               && row (t, "ExifTag") == NULL && row (t, "GPSTag") == NULL
               && row (t, "InteroperabilityTag") == NULL && eq (row (t, "Make"), "Canon"));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("exiv2: empty output not usable", !parse_exiv2 ("", t));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("exiv2: prose without key.group.tag not usable", !parse_exiv2 ("No Exif data found in the file\n", t));
        dv_props_free (t);

        t = props_new ("/x", "t");
        check ("exiv2: two-segment key and empty value ignored",
               !parse_exiv2 ("Exif.Image                 Ascii 1  x\nExif.Image.Make   Ascii 0  \n", t));
        dv_props_free (t);
    }

    puts ("\n-- chain: all three tools installed --");
    use_tools ("exiftool,mediainfo,ffprobe", NULL);
    p = fetch ("photo.jpg");
    check ("source is exiftool", eq (p->source, "exiftool"));
    check ("camera model read from EXIF", eq (row (p, "Camera Model Name"), "EOS R5"));
    check ("GPS position composite present", row (p, "GPS Position") != NULL);
    check ("XMP title with colon intact", eq (row (p, "Title"), "Test title: with colon"));
    check ("no hint when exiftool used", p->hint == NULL);
    check ("no ExifTool version noise group", !has_group (p, "ExifTool"));
    { char *t = dv_props_to_text (p); check ("to_text has [IFD0] heading and Make row",
        strstr (t, "[IFD0]\n") && strstr (t, "Make: Canon\n")); g_free (t); }
    dv_props_free (p);

    puts ("\n-- chain: exiv2 --");
    use_tools ("exiftool,exiv2,mediainfo,ffprobe", NULL);
    p = fetch ("photo.jpg");
    check ("exiftool still wins when both are installed", eq (p->source, "exiftool"));
    dv_props_free (p);

    use_tools ("exiv2,mediainfo,ffprobe", NULL);
    p = fetch ("photo.jpg");
    check ("exiftool missing: exiv2 answers a jpeg with EXIF", eq (p->source, "exiv2"));
    check ("exiv2: Make read from the real file", eq (row (p, "Make"), "Canon"));
    check ("exiv2: hint still recommends exiftool", p->hint && strstr (p->hint, "exiftool"));
    dv_props_free (p);

    p = fetch ("anim.gif");
    check ("exiv2 has nothing for a gif: falls through to mediainfo", eq (p->source, "mediainfo"));
    dv_props_free (p);

    p = fetch ("junk.bin");
    check ("junk file: exiv2 does not claim it", !eq (p->source, "exiv2"));
    dv_props_free (p);

    p = fetch ("-h.jpg");
    check ("file named -h.jpg reaches exiv2 as a path, not an option", eq (p->source, "exiv2") && eq (row (p, "Make"), "Canon"));
    dv_props_free (p);

    puts ("\n-- chain: exiftool missing -> mediainfo --");
    use_tools ("mediainfo,ffprobe", NULL);
    p = fetch ("photo.jpg");
    check ("source is mediainfo", eq (p->source, "mediainfo"));
    check ("width from mediainfo", eq (row (p, "Width"), "320 pixels"));
    check ("hint recommends exiftool", p->hint && strstr (p->hint, "exiftool"));
    dv_props_free (p);

    puts ("\n-- chain: only ffprobe --");
    use_tools ("ffprobe", NULL);
    p = fetch ("photo.jpg");
    check ("source is ffprobe", eq (p->source, "ffprobe"));
    check ("stream group present", has_group (p, "Stream 0 (video)"));
    check ("codec read", eq (row (p, "codec name"), "mjpeg"));
    check ("no N/A rows leaked", ({ gboolean bad = FALSE;
        for (guint i = 0; i < p->groups->len; i++) { DvPropGroup *g = g_ptr_array_index (p->groups, i);
            for (guint j = 0; j < g->rows->len; j++)
                if (strcmp (((DvPropRow *) g_ptr_array_index (g->rows, j))->value, "N/A") == 0) bad = TRUE; }
        !bad; }));
    dv_props_free (p);

    puts ("\n-- chain: no tools at all -> built-in --");
    use_tools ("", NULL);
    p = fetch ("photo.jpg");
    check ("source is built-in", eq (p->source, "built-in"));
    check ("dimensions via gdk-pixbuf", eq (row (p, "Dimensions"), "320 x 240 pixels"));
    check ("file name row", eq (row (p, "Name"), "photo.jpg"));
    check ("size row has bytes", row (p, "Size") && strstr (row (p, "Size"), "12611 bytes"));
    check ("modified row present", row (p, "Modified") != NULL);
    check ("hint says no tool found", p->hint && strstr (p->hint, "No metadata tool found"));
    dv_props_free (p);

    puts ("\n-- usefulness beats exit code --");
    use_tools ("mediainfo,ffprobe", NULL);
    p = fetch ("t.svg");
    check ("svg: mediainfo exits 0 but is unusable -> ffprobe answers", eq (p->source, "ffprobe"));
    dv_props_free (p);
    use_tools ("mediainfo", NULL);
    p = fetch ("t.svg");
    check ("svg with only mediainfo -> built-in", eq (p->source, "built-in"));
    check ("hint says installed tools could not read it", p->hint && strstr (p->hint, "could not read"));
    dv_props_free (p);
    use_tools ("mediainfo", NULL);
    p = fetch ("junk.bin");
    check ("junk: mediainfo unusable -> built-in", eq (p->source, "built-in"));
    dv_props_free (p);
    use_tools ("exiftool,mediainfo,ffprobe", NULL);
    p = fetch ("corrupt.jpg");
    check ("truncated jpeg: exiftool still answers with its warning",
           eq (p->source, "exiftool") && row (p, "Warning") != NULL);
    dv_props_free (p);

    puts ("\n-- hostile and unusual paths --");
    use_tools ("exiftool,mediainfo,ffprobe", NULL);
    p = fetch ("ünï cödé 'q' $(x).jpg");
    check ("quotes/$()/non-ASCII name: exiftool sees the exact name",
           eq (p->source, "exiftool") && eq (row (p, "File Name"), "ünï cödé 'q' $(x).jpg"));
    dv_props_free (p);
    g_chdir (fx);
    p = dv_props_fetch_sync ("-h.jpg");
    check ("relative name starting with '-' is not read as an option",
           eq (p->source, "exiftool") && eq (row (p, "File Name"), "-h.jpg"));
    dv_props_free (p);
    p = dv_props_fetch_sync ("./sub/../photo.jpg");
    check ("relative path with .. is normalised", eq (p->source, "exiftool") && p->path[0] == '/'
           && strstr (p->path, "..") == NULL);
    dv_props_free (p);
    use_tools ("ffprobe", NULL);
    p = dv_props_fetch_sync ("-h.jpg");
    check ("ffprobe with '-'-leading name works too", eq (p->source, "ffprobe"));
    dv_props_free (p);
    use_tools ("exiftool,mediainfo,ffprobe", NULL);
    p = fetch ("plain.png");
    check ("UTF-8 tag values round-trip exactly", eq (row (p, "Artist"), "Zoë 山田")
           && eq (row (p, "Title"), "日本語のタイトル"));
    dv_props_free (p);
    p = fetch ("does_not_exist.jpg");
    check ("missing file: no crash, built-in with Error row",
           eq (p->source, "built-in") && eq (row (p, "Error"), "Cannot read this file"));
    dv_props_free (p);

    puts ("\n-- misbehaving tools fall through --");
    char *fk;
    fk = fxp ("fake/fail");    use_tools ("mediainfo,ffprobe", fk); p = fetch ("photo.jpg");
    check ("exiftool exits 1 -> mediainfo", eq (p->source, "mediainfo")); dv_props_free (p); g_free (fk);
    fk = fxp ("fake/error");   use_tools ("mediainfo,ffprobe", fk); p = fetch ("photo.jpg");
    check ("exiftool prints Error (rc 0) -> mediainfo", eq (p->source, "mediainfo")); dv_props_free (p); g_free (fk);
    fk = fxp ("fake/badutf8"); use_tools ("mediainfo,ffprobe", fk); p = fetch ("photo.jpg");
    check ("invalid UTF-8 from a tool: no crash, output valid UTF-8",
           eq (p->source, "exiftool") && g_utf8_validate (row (p, "Comment"), -1, NULL)
           && strstr (row (p, "Comment"), "bad")); dv_props_free (p); g_free (fk);

    puts ("\n-- watchdog: a hung tool is killed and the built-in answers --");
    fk = fxp ("fake/slow"); use_tools ("mediainfo,ffprobe", fk);
    loop = g_main_loop_new (NULL, FALSE);
    char *f = fxp ("photo.jpg");
    async_t0 = g_get_monotonic_time ();
    dv_props_fetch_async (f, async_done, NULL);
    g_main_loop_run (loop);
    check ("async completed after the timeout, not before",
           async_ms >= DV_PROPS_TIMEOUT_SECS * 1000 - 200 && async_ms < DV_PROPS_TIMEOUT_SECS * 1000 + 3000);
    check ("result is the built-in fallback", async_result && eq (async_result->source, "built-in"));
    /* run the check with the REAL path: under the restricted PATH these
     * commands do not exist and the test would pass vacuously */
    g_setenv ("PATH", orig_path, TRUE);
    check ("orphan check can see processes (control: a spawned sleep is found)",
           system ("/usr/bin/sleep 31415 & P=$!; /usr/bin/sleep 0.3; "
                   "pgrep -f '^/usr/bin/sleep 31415' >/dev/null; r=$?; kill $P; exit $r") == 0);
    check ("hung tool was killed (no orphan left)",
           system ("/usr/bin/sleep 0.5; ! pgrep -f '^/usr/bin/sleep 31415' >/dev/null") == 0);
    dv_props_free (async_result);
    g_free (f); g_free (fk);

    puts ("\n-- async happy path --");
    use_tools ("exiftool", NULL);
    f = fxp ("photo.jpg");
    async_result = NULL; async_t0 = g_get_monotonic_time ();
    dv_props_fetch_async (f, async_done, NULL);
    g_main_loop_run (loop);
    check ("async exiftool result", async_result && eq (async_result->source, "exiftool"));
    check ("async is fast (well under the timeout)", async_ms < 3000);
    dv_props_free (async_result); g_free (f);

    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
