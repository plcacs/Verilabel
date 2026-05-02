gdk_pixbuf__jpeg_image_load_increment (gpointer data,
                                       const guchar *buf, guint size,
                                       GError **error)
{
	JpegProgContext *context = (JpegProgContext *)data;
	struct           jpeg_decompress_struct *cinfo;
	my_src_ptr       src;
	guint            num_left, num_copy;
	guint            last_num_left, last_bytes_left;
	guint            spinguard;
	gboolean         first;
	const guchar    *bufhd;
	gint             width, height;
	char             otag_str[5];
	gchar 		*icc_profile_base64;
	char            *density_str;
	JpegExifContext  exif_context = { 0, };
	gboolean	 retval;

	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);

	src = (my_src_ptr) context->cinfo.src;

	cinfo = &context->cinfo;

        context->jerr.error = error;
        
	/* check for fatal error */
	if (sigsetjmp (context->jerr.setjmp_buffer, 1)) {
		retval = FALSE;
		goto out;
	}

	/* skip over data if requested, handle unsigned int sizes cleanly */
	/* only can happen if we've already called jpeg_get_header once   */
	if (context->src_initialized && src->skip_next) {
		if (src->skip_next > size) {
			src->skip_next -= size;
			retval = TRUE;
			goto out;
		} else {
			num_left = size - src->skip_next;
			bufhd = buf + src->skip_next;
			src->skip_next = 0;
		}
	} else {
		num_left = size;
		bufhd = buf;
	}

	if (num_left == 0) {
		retval = TRUE;
		goto out;
	}

	last_num_left = num_left;
	last_bytes_left = 0;
	spinguard = 0;
	first = TRUE;
	while (TRUE) {

		/* handle any data from caller we haven't processed yet */
		if (num_left > 0) {
			if(src->pub.bytes_in_buffer && 
			   src->pub.next_input_byte != src->buffer)
				memmove(src->buffer, src->pub.next_input_byte,
					src->pub.bytes_in_buffer);


			num_copy = MIN (JPEG_PROG_BUF_SIZE - src->pub.bytes_in_buffer,
					num_left);

			memcpy(src->buffer + src->pub.bytes_in_buffer, bufhd,num_copy);
			src->pub.next_input_byte = src->buffer;
			src->pub.bytes_in_buffer += num_copy;
			bufhd += num_copy;
			num_left -= num_copy;
		}

                /* did anything change from last pass, if not return */
                if (first) {
                        last_bytes_left = src->pub.bytes_in_buffer;
                        first = FALSE;
                } else if (src->pub.bytes_in_buffer == last_bytes_left
			   && num_left == last_num_left) {
                        spinguard++;
		} else {
                        last_bytes_left = src->pub.bytes_in_buffer;
			last_num_left = num_left;
		}

		/* should not go through twice and not pull bytes out of buf */
		if (spinguard > 2) {
			retval = TRUE;
			goto out;
		}

		/* try to load jpeg header */
		if (!context->got_header) {
			int rc;
			gchar* comment;
		
			jpeg_save_markers (cinfo, JPEG_APP0+1, 0xffff);
			jpeg_save_markers (cinfo, JPEG_APP0+2, 0xffff);
			jpeg_save_markers (cinfo, JPEG_COM, 0xffff);
			rc = jpeg_read_header (cinfo, TRUE);
			context->src_initialized = TRUE;
			
			if (rc == JPEG_SUSPENDED)
				continue;
			
			context->got_header = TRUE;

			/* parse exif data */
			jpeg_parse_exif (&exif_context, cinfo);
		
			width = cinfo->image_width;
			height = cinfo->image_height;
			if (context->size_func) {
				(* context->size_func) (&width, &height, context->user_data);
				if (width == 0 || height == 0) {
					g_set_error_literal (error,
                                                             GDK_PIXBUF_ERROR,
                                                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                                                             _("Transformed JPEG has zero width or height."));
					retval = FALSE;
					goto out;
				}
			}
			
			cinfo->scale_num = 1;
			for (cinfo->scale_denom = 2; cinfo->scale_denom <= 8; cinfo->scale_denom *= 2) {
				jpeg_calc_output_dimensions (cinfo);
				if (cinfo->output_width < width || cinfo->output_height < height) {
					cinfo->scale_denom /= 2;
					break;
				}
			}
			jpeg_calc_output_dimensions (cinfo);
			
			context->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
							  cinfo->output_components == 4 ? TRUE : FALSE,
							  8, 
							  cinfo->output_width,
							  cinfo->output_height);

			if (context->pixbuf == NULL) {
                                g_set_error_literal (error,
                                                     GDK_PIXBUF_ERROR,
                                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                                     _("Couldn't allocate memory for loading JPEG file"));
                                retval = FALSE;
				goto out;
			}

			comment = jpeg_get_comment (cinfo);
			if (comment != NULL) {
				gdk_pixbuf_set_option (context->pixbuf, "comment", comment);
				g_free (comment);
			}

			switch (cinfo->density_unit) {
			case 1:
				/* Dots per inch (no conversion required) */
				density_str = g_strdup_printf ("%d", cinfo->X_density);
				gdk_pixbuf_set_option (context->pixbuf, "x-dpi", density_str);
				g_free (density_str);
				density_str = g_strdup_printf ("%d", cinfo->Y_density);
				gdk_pixbuf_set_option (context->pixbuf, "y-dpi", density_str);
				g_free (density_str);
				break;
			case 2:
				/* Dots per cm - convert into dpi */
				density_str = g_strdup_printf ("%d", DPCM_TO_DPI (cinfo->X_density));
				gdk_pixbuf_set_option (context->pixbuf, "x-dpi", density_str);
				g_free (density_str);
				density_str = g_strdup_printf ("%d", DPCM_TO_DPI (cinfo->Y_density));
				gdk_pixbuf_set_option (context->pixbuf, "y-dpi", density_str);
				g_free (density_str);
				break;
			}
		
		        /* if orientation tag was found set an option to remember its value */
			if (exif_context.orientation != 0) {
				g_snprintf (otag_str, sizeof (otag_str), "%d", exif_context.orientation);
		                gdk_pixbuf_set_option (context->pixbuf, "orientation", otag_str);
		        }
			/* if icc profile was found */
			if (exif_context.icc_profile != NULL) {
				icc_profile_base64 = g_base64_encode ((const guchar *) exif_context.icc_profile, exif_context.icc_profile_size);
				gdk_pixbuf_set_option (context->pixbuf, "icc-profile", icc_profile_base64);
				g_free (icc_profile_base64);
			}


			/* Use pixbuf buffer to store decompressed data */
			context->dptr = context->pixbuf->pixels;
			
			/* Notify the client that we are ready to go */
			if (context->prepared_func)
				(* context->prepared_func) (context->pixbuf,
							    NULL,
							    context->user_data);
			
		} else if (!context->did_prescan) {
			int rc;			
			
			/* start decompression */
			cinfo->buffered_image = cinfo->progressive_mode;
			rc = jpeg_start_decompress (cinfo);
			cinfo->do_fancy_upsampling = FALSE;
			cinfo->do_block_smoothing = FALSE;

			if (rc == JPEG_SUSPENDED)
				continue;

			context->did_prescan = TRUE;
		} else if (!cinfo->buffered_image) {
                        /* we're decompressing unbuffered so
                         * simply get scanline by scanline from jpeg lib
                         */
                        if (! gdk_pixbuf__jpeg_image_load_lines (context,
                                                                 error)) {
                                retval = FALSE;
				goto out;
			}

			if (cinfo->output_scanline >= cinfo->output_height) {
				retval = TRUE;
				goto out;
			}
		} else {
                        /* we're decompressing buffered (progressive)
                         * so feed jpeg lib scanlines
                         */

			/* keep going until we've done all passes */
			while (!jpeg_input_complete (cinfo)) {
				if (!context->in_output) {
					if (jpeg_start_output (cinfo, cinfo->input_scan_number)) {
						context->in_output = TRUE;
						context->dptr = context->pixbuf->pixels;
					}
					else
						break;
				}

                                /* get scanlines from jpeg lib */
                                if (! gdk_pixbuf__jpeg_image_load_lines (context,
                                                                         error)) {
                                        retval = FALSE;
					goto out;
				}

				if (cinfo->output_scanline >= cinfo->output_height &&
				    jpeg_finish_output (cinfo))
					context->in_output = FALSE;
				else
					break;
			}
			if (jpeg_input_complete (cinfo)) {
				/* did entire image */
				retval = TRUE;
				goto out;
			}
			else
				continue;
		}
	}
out:
	jpeg_destroy_exif_context (&exif_context);
	return retval;
}