hook_process_child (struct t_hook *hook_process)
{
    char *exec_args[4] = { "sh", "-c", NULL, NULL };
    const char *ptr_url;
    int rc;

    /*
     * close stdin, so that process will fail to read stdin (process reading
     * stdin should not be run inside WeeChat!)
     */
    close (STDIN_FILENO);

    /* redirect stdout/stderr to pipe (so that father process can read them) */
    close (HOOK_PROCESS(hook_process, child_read[HOOK_PROCESS_STDOUT]));
    close (HOOK_PROCESS(hook_process, child_read[HOOK_PROCESS_STDERR]));
    if (dup2 (HOOK_PROCESS(hook_process, child_write[HOOK_PROCESS_STDOUT]),
              STDOUT_FILENO) < 0)
    {
        _exit (EXIT_FAILURE);
    }
    if (dup2 (HOOK_PROCESS(hook_process, child_write[HOOK_PROCESS_STDERR]),
              STDERR_FILENO) < 0)
    {
        _exit (EXIT_FAILURE);
    }

    rc = EXIT_SUCCESS;

    if (strncmp (HOOK_PROCESS(hook_process, command), "url:", 4) == 0)
    {
        /* get URL output (on stdout or file, depending on options) */
        ptr_url = HOOK_PROCESS(hook_process, command) + 4;
        while (ptr_url[0] == ' ')
        {
            ptr_url++;
        }
        rc = weeurl_download (ptr_url, HOOK_PROCESS(hook_process, options));
        if (rc != 0)
            fprintf (stderr, "Error with URL '%s'\n", ptr_url);
    }
    else
    {
        /* launch command */
        exec_args[2] = HOOK_PROCESS(hook_process, command);
        execvp (exec_args[0], exec_args);

        /* should not be executed if execvp was ok */
        fprintf (stderr, "Error with command '%s'\n",
                 HOOK_PROCESS(hook_process, command));
        rc = EXIT_FAILURE;
    }

    fflush (stdout);
    fflush (stderr);

    _exit (rc);
}