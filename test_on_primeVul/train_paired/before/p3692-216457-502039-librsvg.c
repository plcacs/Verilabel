_rsvg_node_poly_build_path (const char *value,
                            gboolean close_path)
{
    double *pointlist;
    guint pointlist_len, i;
    GString *d;
    cairo_path_t *path;
    char buf[G_ASCII_DTOSTR_BUF_SIZE];

    pointlist = rsvg_css_parse_number_list (value, &pointlist_len);
    if (pointlist == NULL)
        return NULL;

    if (pointlist_len < 2) {
        g_free (pointlist);
        return NULL;
    }

    d = g_string_new (NULL);

    /*      "M %f %f " */
    g_string_append (d, " M ");
    g_string_append (d, g_ascii_dtostr (buf, sizeof (buf), pointlist[0]));
    g_string_append_c (d, ' ');
    g_string_append (d, g_ascii_dtostr (buf, sizeof (buf), pointlist[1]));

    /* "L %f %f " */
    for (i = 2; i < pointlist_len; i += 2) {
        g_string_append (d, " L ");
        g_string_append (d, g_ascii_dtostr (buf, sizeof (buf), pointlist[i]));
        g_string_append_c (d, ' ');
        g_string_append (d, g_ascii_dtostr (buf, sizeof (buf), pointlist[i + 1]));
    }

    if (close_path)
        g_string_append (d, " Z");

    path = rsvg_parse_path (d->str);

    g_string_free (d, TRUE);
    g_free (pointlist);

    return path;
}