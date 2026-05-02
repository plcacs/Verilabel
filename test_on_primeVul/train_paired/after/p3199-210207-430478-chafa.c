load_header (XwdLoader *loader)
{
    XwdHeader *h = &loader->header;
    XwdHeader in;
    const XwdHeader *inp;

    if (!file_mapping_taste (loader->mapping, &in, 0, sizeof (in)))
        return FALSE;

    inp = &in;

    UNPACK_FIELD_U32 (h, inp, header_size);
    UNPACK_FIELD_U32 (h, inp, file_version);
    UNPACK_FIELD_U32 (h, inp, pixmap_format);
    UNPACK_FIELD_U32 (h, inp, pixmap_depth);
    UNPACK_FIELD_U32 (h, inp, pixmap_width);
    UNPACK_FIELD_U32 (h, inp, pixmap_height);
    UNPACK_FIELD_U32 (h, inp, x_offset);
    UNPACK_FIELD_U32 (h, inp, byte_order);
    UNPACK_FIELD_U32 (h, inp, bitmap_unit);
    UNPACK_FIELD_U32 (h, inp, bitmap_bit_order);
    UNPACK_FIELD_U32 (h, inp, bitmap_pad);
    UNPACK_FIELD_U32 (h, inp, bits_per_pixel);
    UNPACK_FIELD_U32 (h, inp, bytes_per_line);
    UNPACK_FIELD_U32 (h, inp, visual_class);
    UNPACK_FIELD_U32 (h, inp, red_mask);
    UNPACK_FIELD_U32 (h, inp, green_mask);
    UNPACK_FIELD_U32 (h, inp, blue_mask);
    UNPACK_FIELD_U32 (h, inp, bits_per_rgb);
    UNPACK_FIELD_U32 (h, inp, color_map_entries);
    UNPACK_FIELD_U32 (h, inp, n_colors);
    UNPACK_FIELD_U32 (h, inp, window_width);
    UNPACK_FIELD_U32 (h, inp, window_height);
    UNPACK_FIELD_S32 (h, inp, window_x);
    UNPACK_FIELD_S32 (h, inp, window_y);
    UNPACK_FIELD_U32 (h, inp, window_border_width);

    /* Only support the most common/useful subset of XWD files out there;
     * namely, that corresponding to screen dumps from modern X.Org servers.
     * We could check visual_class == 5 too, but the other fields convey all
     * the info we need. */

    ASSERT_HEADER (h->header_size >= sizeof (XwdHeader));
    ASSERT_HEADER (h->header_size <= 65535);
    ASSERT_HEADER (h->file_version == 7);
    ASSERT_HEADER (h->pixmap_depth == 24);

    /* Should be zero for truecolor/directcolor. Cap it to prevent overflows. */
    ASSERT_HEADER (h->color_map_entries <= 256);

    /* Xvfb sets bits_per_rgb to 8, but 'convert' uses 24 for the same image data. One
     * of them is likely misunderstanding. Let's be lenient and accept either. */
    ASSERT_HEADER (h->bits_per_rgb == 8 || h->bits_per_rgb == 24);

    /* These are the pixel formats we allow. */
    ASSERT_HEADER (h->bits_per_pixel == 24 || h->bits_per_pixel == 32);

    /* Enforce sane dimensions. */
    ASSERT_HEADER (h->pixmap_width >= 1 && h->pixmap_width <= 65535);
    ASSERT_HEADER (h->pixmap_height >= 1 && h->pixmap_height <= 65535);

    /* Make sure rowstride can actually hold a row's worth of data but is not padded to
     * something ridiculous. */
    ASSERT_HEADER (h->bytes_per_line >= h->pixmap_width * (h->bits_per_pixel / 8));
    ASSERT_HEADER (h->bytes_per_line <= h->pixmap_width * (h->bits_per_pixel / 8) + 1024);

    /* Make sure the total allocation/map is not too big. */
    ASSERT_HEADER (h->bytes_per_line * h->pixmap_height < (1UL << 31) - 65536 - 256 * 32);

    ASSERT_HEADER (compute_pixel_type (loader) < CHAFA_PIXEL_MAX);

    loader->file_data = file_mapping_get_data (loader->mapping, &loader->file_data_len);
    if (!loader->file_data)
        return FALSE;

    ASSERT_HEADER (loader->file_data_len >= h->header_size
                   + h->color_map_entries * sizeof (XwdColor)
                   + h->pixmap_height * (gsize) h->bytes_per_line);

    loader->image_data = (const guint8 *) loader->file_data
        + h->header_size + h->color_map_entries * sizeof (XwdColor);

    return TRUE;
}