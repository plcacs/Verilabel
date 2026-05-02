irc_mode_channel_update (struct t_irc_server *server,
                         struct t_irc_channel *channel,
                         char set_flag,
                         char chanmode,
                         const char *argument)
{
    char *pos_args, *str_modes, **argv, *pos, *ptr_arg;
    char *new_modes, *new_args, str_mode[2], *str_temp;
    int argc, current_arg, chanmode_found, length;

    if (!channel->modes)
        channel->modes = strdup ("+");
    if (!channel->modes)
        return;

    argc = 0;
    argv = NULL;
    pos_args = strchr (channel->modes, ' ');
    if (pos_args)
    {
        str_modes = weechat_strndup (channel->modes, pos_args - channel->modes);
        if (!str_modes)
            return;
        pos_args++;
        while (pos_args[0] == ' ')
            pos_args++;
        argv = weechat_string_split (pos_args, " ", NULL,
                                     WEECHAT_STRING_SPLIT_STRIP_LEFT
                                     | WEECHAT_STRING_SPLIT_STRIP_RIGHT
                                     | WEECHAT_STRING_SPLIT_COLLAPSE_SEPS,
                                     0, &argc);
    }
    else
    {
        str_modes = strdup (channel->modes);
        if (!str_modes)
            return;
    }

    new_modes = malloc (strlen (channel->modes) + 1 + 1);
    new_args = malloc (((pos_args) ? strlen (pos_args) : 0)
                       + ((argument) ? 1 + strlen (argument) : 0) + 1);
    if (new_modes && new_args)
    {
        new_modes[0] = '\0';
        new_args[0] = '\0';

        /* loop on current modes and build "new_modes" + "new_args" */
        current_arg = 0;
        chanmode_found = 0;
        pos = str_modes;
        while (pos && pos[0])
        {
            if ((pos[0] == '+') || (pos[0] == '-'))
            {
                str_mode[0] = pos[0];
                str_mode[1] = '\0';
                strcat (new_modes, str_mode);
            }
            else
            {
                ptr_arg = NULL;
                switch (irc_mode_get_chanmode_type (server, pos[0]))
                {
                    case 'A': /* always argument */
                    case 'B': /* always argument */
                    case 'C': /* argument if set */
                        ptr_arg = (current_arg < argc) ?
                            argv[current_arg] : NULL;
                        break;
                    case 'D': /* no argument */
                        break;
                }
                if (ptr_arg)
                    current_arg++;
                if (pos[0] == chanmode)
                {
                    chanmode_found = 1;
                    if (set_flag == '+')
                    {
                        str_mode[0] = pos[0];
                        str_mode[1] = '\0';
                        strcat (new_modes, str_mode);
                        if (argument)
                        {
                            if (new_args[0])
                                strcat (new_args, " ");
                            strcat (new_args, argument);
                        }
                    }
                }
                else
                {
                    str_mode[0] = pos[0];
                    str_mode[1] = '\0';
                    strcat (new_modes, str_mode);
                    if (ptr_arg)
                    {
                        if (new_args[0])
                            strcat (new_args, " ");
                        strcat (new_args, ptr_arg);
                    }
                }
            }
            pos++;
        }
        if (!chanmode_found)
        {
            /*
             * chanmode was not in channel modes: if set_flag is '+', add
             * it to channel modes
             */
            if (set_flag == '+')
            {
                if (argument)
                {
                    /* add mode with argument at the end of modes */
                    str_mode[0] = chanmode;
                    str_mode[1] = '\0';
                    strcat (new_modes, str_mode);
                    if (new_args[0])
                        strcat (new_args, " ");
                    strcat (new_args, argument);
                }
                else
                {
                    /* add mode without argument at the beginning of modes */
                    pos = new_modes;
                    while (pos[0] == '+')
                        pos++;
                    memmove (pos + 1, pos, strlen (pos) + 1);
                    pos[0] = chanmode;
                }
            }
        }
        if (new_args[0])
        {
            length = strlen (new_modes) + 1 + strlen (new_args) + 1;
            str_temp = malloc (length);
            if (str_temp)
            {
                snprintf (str_temp, length, "%s %s", new_modes, new_args);
                if (channel->modes)
                    free (channel->modes);
                channel->modes = str_temp;
            }
        }
        else
        {
            if (channel->modes)
                free (channel->modes);
            channel->modes = strdup (new_modes);
        }
    }

    if (new_modes)
        free (new_modes);
    if (new_args)
        free (new_args);
    if (str_modes)
        free (str_modes);
    if (argv)
        weechat_string_free_split (argv);
}