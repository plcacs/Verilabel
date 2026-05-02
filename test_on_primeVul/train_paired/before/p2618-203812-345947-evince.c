tiff_document_get_thumbnail (EvDocument      *document,
			     EvRenderContext *rc)
{
	TiffDocument *tiff_document = TIFF_DOCUMENT (document);
	int width, height;
	int scaled_width, scaled_height;
	float x_res, y_res;
	gint rowstride, bytes;
	guchar *pixels = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GdkPixbuf *rotated_pixbuf;
	
	push_handlers ();
	if (TIFFSetDirectory (tiff_document->tiff, rc->page->index) != 1) {
		pop_handlers ();
		return NULL;
	}

	if (!TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGEWIDTH, &width)) {
		pop_handlers ();
		return NULL;
	}

	if (! TIFFGetField (tiff_document->tiff, TIFFTAG_IMAGELENGTH, &height)) {
		pop_handlers ();
		return NULL;
	}

	tiff_document_get_resolution (tiff_document, &x_res, &y_res);
	
	pop_handlers ();
  
	/* Sanity check the doc */
	if (width <= 0 || height <= 0)
		return NULL;                

	if (width >= INT_MAX / 4)
		/* overflow */
		return NULL;                
	rowstride = width * 4;
        
	if (height >= INT_MAX / rowstride)
		/* overflow */
		return NULL;                
	bytes = height * rowstride;
	
	pixels = g_try_malloc (bytes);
	if (!pixels)
		return NULL;
	
	pixbuf = gdk_pixbuf_new_from_data (pixels, GDK_COLORSPACE_RGB, TRUE, 8, 
					   width, height, rowstride,
					   (GdkPixbufDestroyNotify) g_free, NULL);
	TIFFReadRGBAImageOriented (tiff_document->tiff,
				   width, height,
				   (uint32 *)pixels,
				   ORIENTATION_TOPLEFT, 0);
	pop_handlers ();

	ev_render_context_compute_scaled_size (rc, width, height * (x_res / y_res),
					       &scaled_width, &scaled_height);
	scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf,
						 scaled_width, scaled_height,
						 GDK_INTERP_BILINEAR);
	g_object_unref (pixbuf);
	
	rotated_pixbuf = gdk_pixbuf_rotate_simple (scaled_pixbuf, 360 - rc->rotation);
	g_object_unref (scaled_pixbuf);
	
	return rotated_pixbuf;
}