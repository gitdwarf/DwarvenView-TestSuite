/* Dialog harness: includes dv_props_dialog.c (statics reachable), links dv_props.c.
 * usage: dialog_test FIXTURE_DIR   (needs a display, e.g. Xvfb) */
#include DV_DIALOG_SRC
#include <stdlib.h>
#include <unistd.h>

static const char *fx;
static char       *orig_path;
static int         fails = 0, checks = 0, bin_n = 0;

static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-68s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

static void
use_tools (const char *csv, const char *fake_dir)
{
    char *dir = g_strdup_printf ("/tmp/dt_bin_%d_%d", (int) getpid (), bin_n++);
    g_mkdir_with_parents (dir, 0755);
    gchar **names = g_strsplit (csv, ",", -1);
    for (gchar **n = names; *n; n++) {
        if (!**n) continue;
        g_setenv ("PATH", orig_path, TRUE);
        char *real = g_find_program_in_path (*n);
        if (real) { char *l = g_build_filename (dir, *n, NULL); if (symlink (real, l)) perror ("symlink"); g_free (l); }
        g_free (real);
    }
    g_strfreev (names);
    char *path = fake_dir ? g_strdup_printf ("%s:%s", fake_dir, dir) : g_strdup (dir);
    g_setenv ("PATH", path, TRUE);
    g_free (path); g_free (dir);
}

/* Depth-first collection of every label's text and every button label. */
static void
collect (GtkWidget *w, GString *out, GtkWidget **found_button, const char *button_label)
{
    if (GTK_IS_LABEL (w)) {
        g_string_append (out, gtk_label_get_text (GTK_LABEL (w)));
        g_string_append_c (out, '\n');
    }
    if (GTK_IS_BUTTON (w) && button_label && gtk_button_get_label (GTK_BUTTON (w)) &&
        strcmp (gtk_button_get_label (GTK_BUTTON (w)), button_label) == 0)
        *found_button = w;
    for (GtkWidget *c = gtk_widget_get_first_child (w); c; c = gtk_widget_get_next_sibling (c))
        collect (c, out, found_button, button_label);
}

static char *
window_text (GtkWidget *win, GtkWidget **button, const char *button_label)
{
    GString *s = g_string_new (NULL);
    collect (win, s, button, button_label);
    return g_string_free (s, FALSE);
}

static void
pump (int ms)
{
    gint64 end = g_get_monotonic_time () + ms * 1000;
    while (g_get_monotonic_time () < end) {
        while (g_main_context_iteration (NULL, FALSE)) ;
        g_usleep (5000);
    }
}

/* wait until the window text contains needle, up to ms */
static gboolean
wait_for (GtkWidget *win, const char *needle, int ms)
{
    gint64 end = g_get_monotonic_time () + ms * 1000;
    while (g_get_monotonic_time () < end) {
        pump (50);
        char *t = window_text (win, NULL, NULL);
        gboolean hit = strstr (t, needle) != NULL;
        g_free (t);
        if (hit) return TRUE;
    }
    return FALSE;
}

static int
count_props_windows (void)
{
    int n = 0;
    GListModel *tl = gtk_window_get_toplevels ();
    for (guint i = 0; i < g_list_model_get_n_items (tl); i++) {
        GtkWindow *w = g_list_model_get_item (tl, i);
        const char *t = gtk_window_get_title (w);
        if (t && g_str_has_prefix (t, "Properties:")) n++;
        g_object_unref (w);
    }
    return n;
}

static char *clip_text;
static void
clip_done (GObject *src, GAsyncResult *res, gpointer d)
{
    (void) d;
    clip_text = gdk_clipboard_read_text_finish (GDK_CLIPBOARD (src), res, NULL);
}

int
main (int argc, char **argv)
{
    if (argc < 2) return 2;
    fx = argv[1];
    orig_path = g_strdup (g_getenv ("PATH"));
    if (!gtk_init_check ()) { puts ("no display"); return 2; }

    GtkWidget *parent = gtk_window_new ();
    gtk_window_present (GTK_WINDOW (parent));
    pump (200);

    char *photo = g_build_filename (fx, "photo.jpg", NULL);

    puts ("-- content, real exiftool --");
    use_tools ("exiftool,mediainfo,ffprobe", NULL);
    dv_props_dialog_show (GTK_WINDOW (parent), photo);
    check ("window opened", props_win != NULL);
    check ("shows a 'reading' indicator immediately", ({ char *t = window_text (props_win, NULL, NULL);
        gboolean h = strstr (t, "Reading properties") != NULL; g_free (t); h; }));
    check ("populated within 10s", wait_for (props_win, "Source:", 10000));
    GtkWidget *copy = NULL;
    char *t = window_text (props_win, &copy, "Copy all");
    check ("title names the file", strcmp (gtk_window_get_title (GTK_WINDOW (props_win)), "Properties: photo.jpg") == 0);
    check ("source label says exiftool", strstr (t, "Source: exiftool") != NULL);
    check ("camera model value shown", strstr (t, "EOS R5") != NULL);
    check ("key label shown", strstr (t, "Camera Model Name") != NULL);
    check ("group heading shown", strstr (t, "IFD0") != NULL && strstr (t, "GPS") != NULL);
    check ("XMP value with colon shown whole", strstr (t, "Test title: with colon") != NULL);
    check ("'Reading' indicator gone", strstr (t, "Reading properties") == NULL);
    check ("no hint shown when exiftool used", strstr (t, "not installed") == NULL);
    check ("Copy all enabled after load", copy && gtk_widget_get_sensitive (copy));
    g_free (t);
    check ("initial focus is not a selectable label", !GTK_IS_LABEL (gtk_window_get_focus (GTK_WINDOW (props_win))));

    puts ("\n-- copy to clipboard --");
    g_signal_emit_by_name (copy, "clicked");
    pump (100);
    gdk_clipboard_read_text_async (gtk_widget_get_clipboard (props_win), NULL, clip_done, NULL);
    pump (500);
    check ("clipboard holds the plain-text rendering",
           clip_text && strstr (clip_text, "[IFD0]") && strstr (clip_text, "Camera Model Name: EOS R5"));
    g_free (clip_text);

    puts ("\n-- single instance --");
    dv_props_dialog_show (GTK_WINDOW (parent), photo);
    dv_props_dialog_show (GTK_WINDOW (parent), photo);
    pump (200);
    check ("showing twice leaves exactly one Properties window", count_props_windows () == 1);
    wait_for (props_win, "Source:", 10000);

    puts ("\n-- Escape closes --");
    check ("on_key(Escape) consumes the key", on_key (NULL, GDK_KEY_Escape, 0, 0, props_win));
    pump (200);
    check ("window gone after Escape", props_win == NULL && count_props_windows () == 0);
    check ("other keys are not consumed", ({ dv_props_dialog_show (GTK_WINDOW (parent), photo);
        gboolean r = !on_key (NULL, GDK_KEY_a, 0, 0, props_win); r; }));
    wait_for (props_win, "Source:", 10000);
    gtk_window_destroy (GTK_WINDOW (props_win));
    pump (100);

    puts ("\n-- fallback hint visible when exiftool absent --");
    use_tools ("mediainfo,ffprobe", NULL);
    dv_props_dialog_show (GTK_WINDOW (parent), photo);
    check ("populated from mediainfo", wait_for (props_win, "Source: mediainfo", 10000));
    t = window_text (props_win, NULL, NULL);
    check ("hint text is shown", strstr (t, "exiftool is not installed") != NULL);
    g_free (t);
    gtk_window_destroy (GTK_WINDOW (props_win));
    pump (100);

    puts ("\n-- built-in tier renders too --");
    use_tools ("", NULL);
    dv_props_dialog_show (GTK_WINDOW (parent), photo);
    check ("populated from built-in", wait_for (props_win, "Source: built-in", 10000));
    t = window_text (props_win, NULL, NULL);
    check ("dimensions row shown", strstr (t, "320 x 240 pixels") != NULL);
    g_free (t);
    gtk_window_destroy (GTK_WINDOW (props_win));
    pump (100);

    puts ("\n-- closing while a slow tool is still running (lifetime) --");
    char *slow = g_build_filename (fx, "fake/slow", NULL);
    use_tools ("mediainfo,ffprobe", slow);
    dv_props_dialog_show (GTK_WINDOW (parent), photo);
    pump (300);
    gtk_window_destroy (GTK_WINDOW (props_win));
    pump (100);
    check ("window closed while fetch pending", props_win == NULL);
    pump (DV_PROPS_TIMEOUT_SECS * 1000 + 1500);       /* let the watchdog fire and the callback land */
    check ("survived the late callback (no crash)", TRUE);
    check ("no window resurrected", count_props_windows () == 0);

    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
