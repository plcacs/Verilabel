static int decode_font(ASS_Track *track)
{
    unsigned char *p;
    unsigned char *q;
    size_t i;
    size_t size;                   // original size
    size_t dsize;                  // decoded size
    unsigned char *buf = 0;

    ass_msg(track->library, MSGL_V, "Font: %d bytes encoded data",
            track->parser_priv->fontdata_used);
    size = track->parser_priv->fontdata_used;
    if (size % 4 == 1) {
        ass_msg(track->library, MSGL_ERR, "Bad encoded data size");
        goto error_decode_font;
    }
    buf = malloc(size / 4 * 3 + FFMAX(size % 4 - 1, 0));
    if (!buf)
        goto error_decode_font;
    q = buf;
    for (i = 0, p = (unsigned char *) track->parser_priv->fontdata;
         i < size / 4; i++, p += 4) {
        q = decode_chars(p, q, 4);
    }
    if (size % 4 == 2) {
        q = decode_chars(p, q, 2);
    } else if (size % 4 == 3) {
        q = decode_chars(p, q, 3);
    }
    dsize = q - buf;
    assert(dsize == size / 4 * 3 + FFMAX(size % 4 - 1, 0));

    if (track->library->extract_fonts) {
        ass_add_font(track->library, track->parser_priv->fontname,
                     (char *) buf, dsize);
    }

error_decode_font:
    free(buf);
    reset_embedded_font_parsing(track->parser_priv);
    return 0;
}