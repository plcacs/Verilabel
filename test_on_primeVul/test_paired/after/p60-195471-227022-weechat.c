IRC_PROTOCOL_CALLBACK(352)
{
    char *pos_attr, *pos_hopcount, *pos_realname, *str_host;
    int arg_start, length;
    struct t_irc_channel *ptr_channel;
    struct t_irc_nick *ptr_nick;

    IRC_PROTOCOL_MIN_ARGS(5);

    /* silently ignore malformed 352 message (missing infos) */
    if (argc < 8)
        return WEECHAT_RC_OK;

    pos_attr = NULL;
    pos_hopcount = NULL;
    pos_realname = NULL;

    if (argc > 8)
    {
        arg_start = ((argc > 9) && (strcmp (argv[8], "*") == 0)) ? 9 : 8;
        if (argv[arg_start][0] == ':')
        {
            pos_attr = NULL;
            pos_hopcount = (argc > arg_start) ? argv[arg_start] + 1 : NULL;
            pos_realname = (argc > arg_start + 1) ? argv_eol[arg_start + 1] : NULL;
        }
        else
        {
            pos_attr = argv[arg_start];
            pos_hopcount = (argc > arg_start + 1) ? argv[arg_start + 1] + 1 : NULL;
            pos_realname = (argc > arg_start + 2) ? argv_eol[arg_start + 2] : NULL;
        }
    }

    ptr_channel = irc_channel_search (server, argv[3]);
    ptr_nick = (ptr_channel) ?
        irc_nick_search (server, ptr_channel, argv[7]) : NULL;

    /* update host in nick */
    if (ptr_nick)
    {
        length = strlen (argv[4]) + 1 + strlen (argv[5]) + 1;
        str_host = malloc (length);
        if (str_host)
        {
            snprintf (str_host, length, "%s@%s", argv[4], argv[5]);
            irc_nick_set_host (ptr_nick, str_host);
            free (str_host);
        }
    }

    /* update away flag in nick */
    if (ptr_channel && ptr_nick && pos_attr)
    {
        irc_nick_set_away (server, ptr_channel, ptr_nick,
                           (pos_attr[0] == 'G') ? 1 : 0);
    }

    /* update realname in nick */
    if (ptr_channel && ptr_nick && pos_realname)
    {
        if (ptr_nick->realname)
            free (ptr_nick->realname);
        if (pos_realname &&
            weechat_hashtable_has_key (server->cap_list, "extended-join"))
        {
            ptr_nick->realname = strdup (pos_realname);
        }
        else
        {
            ptr_nick->realname = NULL;
        }
    }

    /* display output of who (manual who from user) */
    if (!ptr_channel || (ptr_channel->checking_whox <= 0))
    {
        weechat_printf_date_tags (
            irc_msgbuffer_get_target_buffer (
                server, NULL, command, "who", NULL),
            date,
            irc_protocol_tags (command, "irc_numeric", NULL, NULL),
            "%s%s[%s%s%s] %s%s %s(%s%s@%s%s)%s %s%s%s%s(%s)",
            weechat_prefix ("network"),
            IRC_COLOR_CHAT_DELIMITERS,
            IRC_COLOR_CHAT_CHANNEL,
            argv[3],
            IRC_COLOR_CHAT_DELIMITERS,
            irc_nick_color_for_msg (server, 1, NULL, argv[7]),
            argv[7],
            IRC_COLOR_CHAT_DELIMITERS,
            IRC_COLOR_CHAT_HOST,
            argv[4],
            argv[5],
            IRC_COLOR_CHAT_DELIMITERS,
            IRC_COLOR_RESET,
            (pos_attr) ? pos_attr : "",
            (pos_attr) ? " " : "",
            (pos_hopcount) ? pos_hopcount : "",
            (pos_hopcount) ? " " : "",
            (pos_realname) ? pos_realname : "");
    }

    return WEECHAT_RC_OK;
}