gs_font_map_glyph_to_unicode(gs_font *font, gs_glyph glyph, int ch, ushort *u, unsigned int length)
{
    font_data *pdata = pfont_data(font);
    const ref *UnicodeDecoding;
    uchar *unicode_return = (uchar *)u;

    if (r_type(&pdata->GlyphNames2Unicode) == t_dictionary) {
        int c = gs_font_map_glyph_by_dict(font->memory,
                                              &pdata->GlyphNames2Unicode, glyph, u, length);

        if (c != 0)
            return c;
        if (ch != -1) { /* -1 indicates a CIDFont */
            /* Its possible that we have a GlyphNames2Unicode dictionary
             * which contains integers and Unicode values, rather than names
             * and Unicode values. This happens if the input was PDF, the font
             * has a ToUnicode Cmap, but no Encoding. In this case we need to
             * use the character code as an index into the dictionary. Try that
             * now before we fall back to the UnicodeDecoding.
             */
            ref *v, n;

            make_int(&n, ch);
            if (dict_find(&pdata->GlyphNames2Unicode, &n, &v) > 0) {
                if (r_has_type(v, t_string)) {
                    int l = r_size(v);

                    if (l > length)
                        return l;

                    memcpy(unicode_return, v->value.const_bytes, l * sizeof(short));
                    return l;
                }
                if (r_type(v) == t_integer) {
                    if (v->value.intval > 65535) {
                        if (length < 4)
                            return 4;
                        unicode_return[0] = v->value.intval >> 24;
                        unicode_return[1] = (v->value.intval & 0x00FF0000) >> 16;
                        unicode_return[2] = (v->value.intval & 0x0000FF00) >> 8;
                        unicode_return[3] = v->value.intval & 0xFF;
                        return 4;
                    } else {
                        if (length < 2)
                            return 2;
                        unicode_return[0] = v->value.intval >> 8;
                        unicode_return[1] = v->value.intval & 0xFF;
                        return 2;
                    }
                }
            }
        }
        /*
         * Fall through, because test.ps for SF bug #529103 requres
         * to examine both tables. Due to that the Unicode Decoding resource
         * can't be a default value for FontInfo.GlyphNames2Unicode .
         */
    }
    if (glyph <= GS_MIN_CID_GLYPH) {
        UnicodeDecoding = zfont_get_to_unicode_map(font->dir);
        if (UnicodeDecoding != NULL && r_type(UnicodeDecoding) == t_dictionary)
            return gs_font_map_glyph_by_dict(font->memory, UnicodeDecoding, glyph, u, length);
    }
    return 0; /* No map. */
}