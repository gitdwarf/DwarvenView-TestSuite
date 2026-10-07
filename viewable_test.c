/* viewable_test: which files do next/previous stop on, with and without media browsing?
 * usage: viewable_test FIXTURE_DIR   (run make_fixtures.sh first)
 * Links the real dv_viewable.c. Images only: signatures. With media: also anything libav finds audio
 * or video in (a film, a song, a song with cover art). */
#include "dv_viewable.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib/gstdio.h>

static int checks = 0, fails = 0;

static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-62s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

typedef struct { const char *file; gboolean images; gboolean media; const char *why; } Case;   /* result without / with media browsing */

static const Case CASES[] = {
    /* images by signature */
    { "photo.jpg",             TRUE, TRUE, "jpeg" },
    { "anim.gif",              TRUE, TRUE, "gif" },
    { "plain.png",             TRUE, TRUE, "png" },
    { "t.svg",                 TRUE, TRUE, "svg" },
    { "corrupt.jpg",           TRUE, TRUE, "truncated jpeg: signature is enough, the loader decides" },
    { "junk.bin",              FALSE, FALSE, "not an image" },
    /* audio with a picture inside */
    { "audio/with_cover.mp3",     FALSE, TRUE, "mp3, ID3v2.3 cover" },
    { "audio/with_cover_v24.mp3", FALSE, TRUE, "mp3, ID3v2.4 cover" },
    { "audio/with_cover.flac",    FALSE, TRUE, "flac cover" },
    { "audio/with_cover.m4a",     FALSE, TRUE, "m4a cover" },
    { "audio/with_cover.mka",     FALSE, TRUE, "matroska audio cover" },
    { "audio/with_cover.wma",     FALSE, TRUE, "wma cover" },
    { "audio/with_cover.ogg",     FALSE, TRUE, "ogg vorbis cover" },
    { "audio/with_cover.opus",    FALSE, TRUE, "opus cover" },
    { "video/clip.mkv",           FALSE, TRUE, "matroska video" },
    { "video/clip.mp4",           FALSE, TRUE, "mp4 video" },
    { "audio/ünï & cövér.mp3",    FALSE, TRUE, "odd characters in the name" },
    /* audio and look-alikes without one */
    { "audio/no_cover.mp3",    FALSE, TRUE, "mp3 with no picture" },
    { "audio/no_cover.flac",   FALSE, TRUE, "flac with no picture" },
    { "audio/tags_only.mp3",   FALSE, TRUE, "ID3 tag with text only" },
    { "audio/fake_id3.mp3",    FALSE, FALSE, "ID3 header that promises a tag and ends" },
    { "audio/empty.bin",       FALSE, FALSE, "empty file" },
    { "audio/text.txt",        FALSE, FALSE, "plain text" },
    { "audio",                 FALSE, FALSE, "a directory" },
    { "ftyp_avif.bin",         TRUE,  TRUE,  "ftyp box with an image brand" },
    { "ftyp_isom.bin",         FALSE, FALSE, "ftyp box with a video brand and nothing else" },
    { "lookalike/doc.pdf", FALSE, FALSE, "pdf, read by libav as a bare mjpeg at score 12" },
    { "lookalike/comic.cbz", FALSE, FALSE, "cbz: a zip of jpegs" },
    { "lookalike/comic.zip", FALSE, FALSE, "zip of jpegs" },
    { "lookalike/comic.epub", FALSE, FALSE, "epub: a zip" },
    { "lookalike/comic.cbr", FALSE, FALSE, "cbr: rar, libav cannot open it" },
    { "lookalike/blob.bin", FALSE, FALSE, "arbitrary binary, read as text art at score 50" },
    { "lookalike/random.dat", FALSE, FALSE, "random bytes" },
    { "media/tone.wav", FALSE, TRUE, "real wav" },
    { "media/tone.aac", FALSE, TRUE, "real aac" },
    { "media/tone.ac3", FALSE, TRUE, "real ac3" },
    { "media/tone.aiff", FALSE, TRUE, "real aiff" },
    { "media/tone.au", FALSE, TRUE, "real au" },
    { "media/tone.caf", FALSE, TRUE, "real caf" },
    { "media/tone.w64", FALSE, TRUE, "real w64" },
    { "media/tone.wv", FALSE, TRUE, "real wv" },
    { "media/tone.mp2", FALSE, TRUE, "real mp2" },
    { "media/tone.mka", FALSE, TRUE, "real mka" },
    { "media/tone.flv", FALSE, TRUE, "real flv" },
    { "media/tone.mpg", FALSE, TRUE, "real mpg" },
    { "media/tone.ogv", FALSE, TRUE, "real ogv" },
    { "media/tone.3gp", FALSE, TRUE, "real 3gp" },
    { "media/clip.ts",         FALSE, TRUE,  "real mpeg transport stream (score 50)" },
    { "lookalike/stored_mpg.zip", FALSE, FALSE, "stored zip holding an mpeg: libav sees mpeg at score 26" },
    { "lookalike/stored_mpg.cbz", FALSE, FALSE, "cbz holding an mpeg" },
    { "lookalike/stored_mpg.tar", FALSE, FALSE, "tar holding an mpeg" },
    { "lookalike/sig.7z", FALSE, FALSE, "7z signature" },
    { "lookalike/sig.ar", FALSE, FALSE, "ar signature" },
    { "audio/does-not-exist",  FALSE, FALSE, "missing file" },
};

int
main (int argc, char **argv)
{
    if (argc < 2) return 2;
    const char *fx = argv[1];

    for (guint i = 0; i < G_N_ELEMENTS (CASES); i++) {
        char    *p   = g_build_filename (fx, CASES[i].file, NULL);
        char    *l1  = g_strdup_printf ("images only  %s: %s", CASES[i].file, CASES[i].why);
        char    *l2  = g_strdup_printf ("with media   %s: %s", CASES[i].file, CASES[i].why);
        check (l1, dv_file_is_browsable (p, FALSE) == CASES[i].images);
        check (l2, dv_file_is_browsable (p, TRUE)  == CASES[i].media);
        g_free (l1);
        g_free (l2);
        g_free (p);
    }

    /* A bare relative name must be read as a file, never as a libav protocol.
     * Control: the file really is a valid mp3, so the answer has to be yes. */
    char *cwd = g_get_current_dir ();
    char *dir = g_build_filename (fx, "audio", NULL);
    if (chdir (dir) != 0) { perror ("chdir"); return 2; }
    check ("relative name 'with_cover.mp3'", dv_file_is_browsable ("with_cover.mp3", TRUE));
    check ("relative name 'concat:evil.mp3' is a file, not a protocol",
           dv_file_is_browsable ("concat:evil.mp3", TRUE));
    if (chdir (cwd) != 0) perror ("chdir back");
    g_free (dir);
    g_free (cwd);

    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
