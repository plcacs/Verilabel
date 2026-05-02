udisks_log (UDisksLogLevel     level,
            const gchar       *function,
            const gchar       *location,
            const gchar       *format,
            ...)
{
  va_list var_args;
  gchar *message;

  va_start (var_args, format);
  message = g_strdup_vprintf (format, var_args);
  va_end (var_args);

#if GLIB_CHECK_VERSION(2, 50, 0)
  g_log_structured ("udisks", (GLogLevelFlags) level,
                    "MESSAGE", "%s", message, "THREAD_ID", "%d", (gint) syscall (SYS_gettid),
                    "CODE_FUNC", function, "CODE_FILE", location);
#else
  g_log ("udisks", level, "[%d]: %s [%s, %s()]", (gint) syscall (SYS_gettid), message, location, function);
#endif

  g_free (message);
}