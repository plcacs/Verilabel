hook_process_child (struct t_hook *hook_process)
{
    char **exec_args;
    const char *ptr_url;
    int rc, i;

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
        exec_args = string_split_shell (HOOK_PROCESS(hook_process, command));
        if (exec_args)
        {
            if (weechat_debug_core >= 1)
            {
                log_printf ("hook_process, command='%s'",
                            HOOK_PROCESS(hook_process, command));
                for (i = 0; exec_args[i]; i++)
                {
                    log_printf ("  args[%02d] == '%s'", i, exec_args[i]);
                }
            }
            execvp (exec_args[0], exec_args);
        }

        /* should not be executed if execvp was ok */
        if (exec_args)
            string_free_split (exec_args);
        fprintf (stderr, "Error with command '%s'\n",
                 HOOK_PROCESS(hook_process, command));
        rc = EXIT_FAILURE;
    }

    fflush (stdout);
    fflush (stderr);

    _exit (rc);
}