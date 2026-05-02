gxps_fonts_new_font_face (GXPSArchive *zip,
			  const gchar *font_uri,
			  GError     **error)
{
	GHashTable        *ft_cache;
	FtFontFace         ft_face;
	FtFontFace        *ft_font_face;
	FT_Face            face;
	cairo_font_face_t *font_face;
	guchar            *font_data;
	gsize              font_data_len;
	gboolean           res;

	res = gxps_archive_read_entry (zip, font_uri,
				       &font_data, &font_data_len,
				       error);
	if (!res) {
		g_set_error (error,
			     GXPS_ERROR,
			     GXPS_ERROR_SOURCE_NOT_FOUND,
			     "Font source %s not found in archive",
			     font_uri);
		return NULL;
	}

	ft_face.font_data = font_data;
	ft_face.font_data_len = (gssize)font_data_len;

	ft_cache = get_ft_font_face_cache ();
	font_face = g_hash_table_lookup (ft_cache, &ft_face);
	if (font_face) {
		g_free (font_data);

		return font_face;
	}

	if (!gxps_fonts_new_ft_face (font_uri, font_data, font_data_len, &face)) {
		g_set_error (error,
			     GXPS_ERROR,
			     GXPS_ERROR_FONT,
			     "Failed to load font %s", font_uri);
		g_free (font_data);

		return NULL;
	}

	font_face = cairo_ft_font_face_create_for_ft_face (face, 0);
	if (cairo_font_face_set_user_data (font_face,
					   &ft_cairo_key,
					   face,
					   (cairo_destroy_func_t) FT_Done_Face)) {
		g_set_error (error,
			     GXPS_ERROR,
			     GXPS_ERROR_FONT,
			     "Failed to load font %s: %s",
			     font_uri,
			     cairo_status_to_string (cairo_font_face_status (font_face)));
		cairo_font_face_destroy (font_face);
		FT_Done_Face (face);

		return NULL;
	}

	ft_font_face = ft_font_face_new (font_data, (gssize)font_data_len);
	g_hash_table_insert (ft_cache, ft_font_face, font_face);

	return font_face;
}