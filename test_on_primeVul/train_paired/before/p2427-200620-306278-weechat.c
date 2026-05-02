irc_color_decode (const char *string, int keep_colors)
{
    unsigned char *out, *ptr_string;
    int out_length, length, out_pos;
    char str_fg[3], str_bg[3], str_color[128], str_key[128];
    const char *remapped_color;
    int fg, bg, bold, reverse, italic, underline, rc;

    out_length = (strlen (string) * 2) + 1;
    out = malloc (out_length);
    if (!out)
        return NULL;

    bold = 0;
    reverse = 0;
    italic = 0;
    underline = 0;

    ptr_string = (unsigned char *)string;
    out[0] = '\0';
    while (ptr_string && ptr_string[0])
    {
        switch (ptr_string[0])
        {
            case IRC_COLOR_BOLD_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((bold) ? "-bold" : "bold"));
                bold ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_RESET_CHAR:
                if (keep_colors)
                    strcat ((char *)out, weechat_color("reset"));
                bold = 0;
                reverse = 0;
                italic = 0;
                underline = 0;
                ptr_string++;
                break;
            case IRC_COLOR_FIXED_CHAR:
                ptr_string++;
                break;
            case IRC_COLOR_REVERSE_CHAR:
            case IRC_COLOR_REVERSE2_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((reverse) ? "-reverse" : "reverse"));
                reverse ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_ITALIC_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((italic) ? "-italic" : "italic"));
                italic ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_UNDERLINE_CHAR:
                if (keep_colors)
                    strcat ((char *)out,
                            weechat_color((underline) ? "-underline" : "underline"));
                underline ^= 1;
                ptr_string++;
                break;
            case IRC_COLOR_COLOR_CHAR:
                ptr_string++;
                str_fg[0] = '\0';
                str_bg[0] = '\0';
                if (isdigit (ptr_string[0]))
                {
                    str_fg[0] = ptr_string[0];
                    str_fg[1] = '\0';
                    ptr_string++;
                    if (isdigit (ptr_string[0]))
                    {
                        str_fg[1] = ptr_string[0];
                        str_fg[2] = '\0';
                        ptr_string++;
                    }
                }
                if ((ptr_string[0] == ',') && (isdigit (ptr_string[1])))
                {
                    ptr_string++;
                    str_bg[0] = ptr_string[0];
                    str_bg[1] = '\0';
                    ptr_string++;
                    if (isdigit (ptr_string[0]))
                    {
                        str_bg[1] = ptr_string[0];
                        str_bg[2] = '\0';
                        ptr_string++;
                    }
                }
                if (keep_colors)
                {
                    if (str_fg[0] || str_bg[0])
                    {
                        fg = -1;
                        bg = -1;
                        if (str_fg[0])
                        {
                            rc = sscanf (str_fg, "%d", &fg);
                            if ((rc != EOF) && (rc >= 1))
                            {
                                fg %= IRC_NUM_COLORS;
                            }
                        }
                        if (str_bg[0])
                        {
                            rc = sscanf (str_bg, "%d", &bg);
                            if ((rc != EOF) && (rc >= 1))
                            {
                                bg %= IRC_NUM_COLORS;
                            }
                        }
                        /* search "fg,bg" in hashtable of remapped colors */
                        snprintf (str_key, sizeof (str_key), "%d,%d", fg, bg);
                        remapped_color = weechat_hashtable_get (
                            irc_config_hashtable_color_mirc_remap,
                            str_key);
                        if (remapped_color)
                        {
                            snprintf (str_color, sizeof (str_color),
                                      "|%s", remapped_color);
                        }
                        else
                        {
                            snprintf (str_color, sizeof (str_color),
                                      "|%s%s%s",
                                      (fg >= 0) ? irc_color_to_weechat[fg] : "",
                                      (bg >= 0) ? "," : "",
                                      (bg >= 0) ? irc_color_to_weechat[bg] : "");
                        }
                        strcat ((char *)out, weechat_color(str_color));
                    }
                    else
                        strcat ((char *)out, weechat_color("resetcolor"));
                }
                break;
            default:
                length = weechat_utf8_char_size ((char *)ptr_string);
                if (length == 0)
                    length = 1;
                out_pos = strlen ((char *)out);
                memcpy (out + out_pos, ptr_string, length);
                out[out_pos + length] = '\0';
                ptr_string += length;
                break;
        }
    }

    return (char *)out;
}