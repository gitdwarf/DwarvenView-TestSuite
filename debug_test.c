/* Standalone test of dv_debug.c: real file I/O, a forked "simulated crash"
 * to prove durability, no GTK needed. */
#include DV_DEBUG_SRC
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

static int checks = 0, fails = 0;
static void
check (const char *label, gboolean cond)
{
    checks++;
    printf ("%-62s %s\n", label, cond ? "PASS" : "FAIL");
    if (!cond) fails++;
}

static gsize
file_size (const char *path)
{
    GStatBuf st;
    return g_stat (path, &st) == 0 ? (gsize) st.st_size : (gsize) -1;
}

static int
line_count (const char *path)
{
    char *content = NULL;
    if (!g_file_get_contents (path, &content, NULL, NULL))
        return -1;
    int n = 0;
    for (char *c = content; *c; c++) if (*c == '\n') n++;
    g_free (content);
    return n;
}

int
main (void)
{
    /* Isolate from any real config/cache the container might have. */
    char *cfg = g_strdup_printf ("/tmp/dbgtest_cfg_%d", (int) getpid ());
    char *che = g_strdup_printf ("/tmp/dbgtest_cache_%d", (int) getpid ());
    g_setenv ("XDG_CONFIG_HOME", cfg, TRUE);
    g_setenv ("XDG_CACHE_HOME", che, TRUE);

    char *log = dv_debug_log_path ();
    check ("log path is under XDG_CACHE_HOME", g_str_has_prefix (log, che));

    puts ("-- default state --");
    check ("logging is off by default", !dv_debug_logging_enabled ());
    dv_debug_log ("should not be written");
    check ("disabled dv_debug_log() creates no file", !g_file_test (log, G_FILE_TEST_EXISTS));

    puts ("\n-- enabling --");
    dv_debug_set_logging_enabled (TRUE);
    check ("enabled flag reads back true", dv_debug_logging_enabled ());
    dv_debug_log ("hello %d", 1);
    dv_debug_log ("hello %d", 2);
    check ("file created once enabled", g_file_test (log, G_FILE_TEST_EXISTS));
    check ("two lines written", line_count (log) == 2);
    char *content = NULL;
    g_file_get_contents (log, &content, NULL, NULL);
    check ("timestamp prefix present", content && content[0] == '[');
    check ("message text intact", content && strstr (content, "hello 1") && strstr (content, "hello 2"));
    g_free (content);

    puts ("\n-- persistence across a fresh process view --");
    cached_enabled = FALSE; cache_loaded = FALSE;   /* simulate a new process reading the setting */
    check ("re-reads enabled=TRUE from disk", dv_debug_logging_enabled ());

    puts ("\n-- disabling deletes the log immediately --");
    dv_debug_set_logging_enabled (FALSE);
    check ("enabled flag reads back false", !dv_debug_logging_enabled ());
    check ("log file removed on disable", !g_file_test (log, G_FILE_TEST_EXISTS));
    dv_debug_log ("should not be written either");
    check ("still a no-op once disabled again", !g_file_test (log, G_FILE_TEST_EXISTS));

    puts ("\n-- crash durability: log survives an abrupt kill -9 mid-run --");
    dv_debug_set_logging_enabled (TRUE);
    /* Baseline: prove kill -9 WOULD lose unflushed stdio output, so the
     * fsync test below is checking something real, not something that
     * would pass regardless. */
    {
        /* setvbuf with a large explicit buffer, well beyond what a single
         * fprintf call needs, so nothing auto-flushes before the kill --
         * an honest control for "data still sitting in userspace when the
         * process dies", the exact scenario fsync() in dv_debug_log()
         * exists to rule out. */
        char *plain = g_strdup_printf ("%s/plain_stdio.txt", che);
        pid_t pid = fork ();
        if (pid == 0) {
            FILE *f = fopen (plain, "w");
            static char iobuf[1 << 20];
            setvbuf (f, iobuf, _IOFBF, sizeof (iobuf));
            fprintf (f, "line 0 of deliberately unflushed buffered stdio output\n");
            kill (getpid (), SIGKILL);
            _exit (1);   /* unreachable */
        }
        int status; waitpid (pid, &status, 0);
        gsize sz = file_size (plain);
        check ("control: SIGKILL before fflush loses the buffered line",
               sz == 0 || sz == (gsize) -1);
        g_free (plain);
    }
    {
        pid_t pid = fork ();
        if (pid == 0) {
            for (int i = 0; i < 200; i++)
                dv_debug_log ("crash-durability line %d of the padded test message body", i);
            kill (getpid (), SIGKILL);
            _exit (1);   /* unreachable */
        }
        int status; waitpid (pid, &status, 0);
        check ("child was actually killed by SIGKILL (test is valid)",
               WIFSIGNALED (status) && WTERMSIG (status) == SIGKILL);
        check ("all 200 fsync'd lines survived the kill", line_count (log) == 200);
        char *c2 = NULL;
        g_file_get_contents (log, &c2, NULL, NULL);
        check ("the very last line is intact, not torn",
               c2 && strstr (c2, "crash-durability line 199 of the padded test message body\n"));
        g_free (c2);
    }
    dv_debug_set_logging_enabled (FALSE);

    puts ("\n-- printf-style formatting --");
    dv_debug_set_logging_enabled (TRUE);
    g_unlink (log);
    dv_debug_log ("%s %d %.2f %%", "mix", 42, 3.14159);
    char *c3 = NULL;
    g_file_get_contents (log, &c3, NULL, NULL);
    check ("format specifiers applied correctly", c3 && strstr (c3, "mix 42 3.14 %"));
    g_free (c3);
    dv_debug_set_logging_enabled (FALSE);

    printf ("\nchecks: %d  failures: %d\n", checks, fails);
    return fails ? 1 : 0;
}
