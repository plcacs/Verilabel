LogFilePrep(const char *fname, const char *backup, const char *idstring)
{
    char *logFileName = NULL;

    if (asprintf(&logFileName, fname, idstring) == -1)
        FatalError("Cannot allocate space for the log file name\n");

    if (backup && *backup) {
        struct stat buf;

        if (!stat(logFileName, &buf) && S_ISREG(buf.st_mode)) {
            char *suffix;
            char *oldLog;

            if ((asprintf(&suffix, backup, idstring) == -1) ||
                (asprintf(&oldLog, "%s%s", logFileName, suffix) == -1)) {
                FatalError("Cannot allocate space for the log file name\n");
            }
            free(suffix);

            if (rename(logFileName, oldLog) == -1) {
                FatalError("Cannot move old log file \"%s\" to \"%s\"\n",
                           logFileName, oldLog);
            }
            free(oldLog);
        }
    }
    else {
        if (remove(logFileName) != 0 && errno != ENOENT) {
            FatalError("Cannot remove old log file \"%s\": %s\n",
                       logFileName, strerror(errno));
        }
    }

    return logFileName;
}