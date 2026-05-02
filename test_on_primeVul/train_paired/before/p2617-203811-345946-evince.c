tiff_document_render (EvDocument      *document,
		      EvRenderContext *rc)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);
	int width, height;
	int scaled_width, scaled_height;
	float x_res, y_res;
	gint rowstride, bytes;
	guchar *pixels = NULL;
	guchar *p;
	int orientation;
	cairo_surface_t *surface;
	cairo_surface_t *rotated_surface;
	static const cairo_user_data_key_t key;
	
	g_return_val_if_fail (TIFF_IS_DOCUMENT (document), NULL);
	g_return_val_if_fail (tiff_document->tiff != NULL, NULL);
  
	push_handlers ();
	if (TIFFSetDirectory (tiff_document->tiff, rc->page->index) != 1) {
		pop_handlers ();
		g_warning("Failed to select page %d", rc->page->index);
		return NULL;
	}

	if (!TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &width)) {
		pop_handlers ();
		g_warning("Failed to read image width");
		return NULL;
	}

	if (! TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &height)) {
		pop_handlers ();
		g_warning("Failed to read image height");
		return NULL;
	}

	if (! TIFFGetField (tiff_document->tiff, TIFFTAG_ORIENTATION, &orientation)) {
		orientation = ORIENTATION_TOPLEFT;
	}

	tiff_document_get_resolution (tiff_document, &x_res, &y_res);
	
	pop_handlers ();
  
	/* Sanity check the doc */
	if (width <= 0 || height <= 0) {
		g_warning("Invalid width or height.");
		return NULL;
	}

	rowstride = cairo_format_stride_for_width (CAIRO_FORMAT_RGB24, width);
	if (rowstride / 4 != width) {
		g_warning("Overflow while rendering document.");
		/* overflow, or cairo was changed in an unsupported way */
		return NULL;                
	}
	
	if (height >= INT_MAX / rowstride) {
		g_warning("Overflow while rendering document.");
		/* overflow */
		return NULL;
	}
	bytes = height * rowstride;
	
	pixels = g_try_malloc (bytes);
	if (!pixels) {
		g_warning("Failed to allocate memory for rendering.");
		return NULL;
	}
	
	surface = cairo_image_surface_create_for_data (pixels,
						       CAIRO_FORMAT_RGB24,
						       width, height,
						       rowstride);
	cairo_surface_set_user_data (surface, &key,
				     pixels, (cairo_destroy_func_t)g_free);

	TIFFReadRGBAImageOriented (tiff_document->tiff,
				   width, height,
				   (uint32 *)pixels,
				   orientation, 0);
	pop_handlers ();

	/* Convert the format returned by libtiff to
	* what cairo expects
	*/
	p = pixels;
	while (p < pixels + bytes) {
		guint32 *pixel = (guint32*)p;
		guint8 r = TIFFGetR(*pixel);
		guint8 g = TIFFGetG(*pixel);
		guint8 b = TIFFGetB(*pixel);
		guint8 a = TIFFGetA(*pixel);

		*pixel = (a << 24) | (r << 16) | (g << 8) | b;

		p += 4;
	}

	ev_render_context_compute_scaled_size (rc, width, height * (x_res / y_res),
					       &scaled_width, &scaled_height);
	rotated_surface = ev_document_misc_surface_rotate_and_scale (surface,
								     scaled_width, scaled_height,
								     rc->rotation);
	cairo_surface_destroy (surface);
	
	return rotated_surface;
}