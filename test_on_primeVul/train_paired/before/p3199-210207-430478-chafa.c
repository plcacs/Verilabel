load_header (XwdLoader *loader) // gconstpointer in, gsize in_max_len, XwdHeader *header_out)
{
    XwdHeader *h = &loader->header;
    XwdHeader in;
    const guint32 *p = (const guint32 *) &in;

    if (!file_mapping_taste (loader->mapping, &in, 0, sizeof (in)))
        return FALSE;

    h->header_size = g_ntohl (*(p++));
    h->file_version = g_ntohl (*(p++));
    h->pixmap_format = g_ntohl (*(p++));
    h->pixmap_depth = g_ntohl (*(p++));
    h->pixmap_width = g_ntohl (*(p++));
    h->pixmap_height = g_ntohl (*(p++));
    h->x_offset = g_ntohl (*(p++));
    h->byte_order = g_ntohl (*(p++));
    h->bitmap_unit = g_ntohl (*(p++));
    h->bitmap_bit_order = g_ntohl (*(p++));
    h->bitmap_pad = g_ntohl (*(p++));
    h->bits_per_pixel = g_ntohl (*(p++));
    h->bytes_per_line = g_ntohl (*(p++));
    h->visual_class = g_ntohl (*(p++));
    h->red_mask = g_ntohl (*(p++));
    h->green_mask = g_ntohl (*(p++));
    h->blue_mask = g_ntohl (*(p++));
    h->bits_per_rgb = g_ntohl (*(p++));
    h->color_map_entries = g_ntohl (*(p++));
    h->n_colors = g_ntohl (*(p++));
    h->window_width = g_ntohl (*(p++));
    h->window_height = g_ntohl (*(p++));
    h->window_x = g_ntohl (*(p++));
    h->window_y = g_ntohl (*(p++));
    h->window_border_width = g_ntohl (*(p++));

    /* Only support the most common/useful subset of XWD files out there;
     * namely, that corresponding to screen dumps from modern X.Org servers. */

    ASSERT_HEADER (h->header_size >= sizeof (XwdHeader));
    ASSERT_HEADER (h->file_version == 7);
    ASSERT_HEADER (h->pixmap_depth == 24);

    /* Xvfb sets bits_per_rgb to 8, but 'convert' uses 24 for the same image data. One
     * of them is likely misunderstanding. Let's be lenient and accept either. */
    ASSERT_HEADER (h->bits_per_rgb == 8 || h->bits_per_rgb == 24);

    ASSERT_HEADER (h->bytes_per_line >= h->pixmap_width * (h->bits_per_pixel / 8));
    ASSERT_HEADER (compute_pixel_type (loader) < CHAFA_PIXEL_MAX);

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        return FALSE;

    ASSERT_HEADER (loader->file_data_len >= h->header_size
                   + h->n_colors * sizeof (XwdColor)
                   + h->pixmap_height * h->bytes_per_line);

    loader->image_data = (const guint8 *) loader->file_data
        + h->header_size + h->n_colors * sizeof (XwdColor);

    return TRUE;
}