adv_error adv_png_read_ihdr(
	unsigned* pix_width, unsigned* pix_height, unsigned* pix_pixel,
	unsigned char** dat_ptr, unsigned* dat_size,
	unsigned char** pix_ptr, unsigned* pix_scanline,
	unsigned char** pal_ptr, unsigned* pal_size,
	unsigned char** rns_ptr, unsigned* rns_size,
	adv_fz* f, const unsigned char* data, unsigned data_size)
{
	unsigned char* ptr;
	unsigned ptr_size;
	unsigned type;
	unsigned long res_size;
	unsigned pixel;
	unsigned width;
	unsigned width_align;
	unsigned height;
	unsigned depth;
	int r;
	z_stream z;
	adv_bool has_palette;

	*dat_ptr = 0;
	*pix_ptr = 0;
	*pal_ptr = 0;
	*pal_size = 0;
	*rns_ptr = 0;
	*rns_size = 0;

	if (data_size != 13) {
		error_set("Invalid IHDR size %d instead of 13", data_size);
		goto err;
	}

	*pix_width = width = be_uint32_read(data + 0);
	*pix_height = height = be_uint32_read(data + 4);

	depth = data[8];
	if (data[9] == 3 && depth == 8) {
		pixel = 1;
		width_align = width;
		has_palette = 1;
	} else if (data[9] == 3 && depth == 4) {
		pixel = 1;
		width_align = (width + 1) & ~1;
		has_palette = 1;
	} else if (data[9] == 3 && depth == 2) {
		pixel = 1;
		width_align = (width + 3) & ~3;
		has_palette = 1;
	} else if (data[9] == 3 && depth == 1) {
		pixel = 1;
		width_align = (width + 7) & ~7;
		has_palette = 1;
	} else if (data[9] == 2 && depth == 8) {
		pixel = 3;
		width_align = width;
		has_palette = 0;
	} else if (data[9] == 6 && depth == 8) {
		pixel = 4;
		width_align = width;
		has_palette = 0;
	} else {
		error_unsupported_set("Unsupported bit depth/color type, %d/%d", (unsigned)data[8], (unsigned)data[9]);
		goto err;
	}
	*pix_pixel = pixel;

	if (data[10] != 0) { /* compression */
		error_unsupported_set("Unsupported compression, %d instead of 0", (unsigned)data[10]);
		goto err;
	}
	if (data[11] != 0) { /* filter */
		error_unsupported_set("Unsupported filter, %d instead of 0", (unsigned)data[11]);
		goto err;
	}
	if (data[12] != 0) { /* interlace */
		error_unsupported_set("Unsupported interlace %d", (unsigned)data[12]);
		goto err;
	}

	if (adv_png_read_chunk(f, &ptr, &ptr_size, &type) != 0)
		goto err;

	while (type != ADV_PNG_CN_IDAT) {
		if (type == ADV_PNG_CN_PLTE) {
			if (ptr_size > 256*3) {
				error_set("Invalid palette size in PLTE chunk");
				goto err_ptr;
			}

			if (*pal_ptr) {
				error_set("Double palette specification");
				goto err_ptr;
			}

			*pal_ptr = ptr;
			*pal_size = ptr_size;
		} else if (type == ADV_PNG_CN_tRNS) {
			if (*rns_ptr) {
				error_set("Double rns specification");
				goto err_ptr;
			}

			*rns_ptr = ptr;
			*rns_size = ptr_size;
		} else {
			/* ancillary bit. bit 5 of first byte. 0 (uppercase) = critical, 1 (lowercase) = ancillary. */
			if ((type & 0x20000000) == 0) {
				char buf[4];
				be_uint32_write(buf, type);
				error_unsupported_set("Unsupported critical chunk '%c%c%c%c'", buf[0], buf[1], buf[2], buf[3]);
				goto err_ptr;
			}

			free(ptr);
		}

		if (adv_png_read_chunk(f, &ptr, &ptr_size, &type) != 0)
			goto err;
	}

	if (has_palette && !*pal_ptr) {
		error_set("Missing PLTE chunk");
		goto err_ptr;
	}

	if (!has_palette && *pal_ptr) {
		error_set("Unexpected PLTE chunk");
		goto err_ptr;
	}

	*dat_size = height * (width_align * pixel + 1);
	*dat_ptr = malloc(*dat_size);
	*pix_scanline = width_align * pixel + 1;
	*pix_ptr = *dat_ptr + 1;

	z.zalloc = 0;
	z.zfree = 0;
	z.next_out = *dat_ptr;
	z.avail_out = *dat_size;
	z.next_in = 0;
	z.avail_in = 0;

	r = inflateInit(&z);

	while (r == Z_OK && type == ADV_PNG_CN_IDAT) {
		z.next_in = ptr;
		z.avail_in = ptr_size;

		r = inflate(&z, Z_NO_FLUSH);

		free(ptr);

		if (adv_png_read_chunk(f, &ptr, &ptr_size, &type) != 0) {
			inflateEnd(&z);
			goto err;
		}
	}

	res_size = z.total_out;

	inflateEnd(&z);

	if (r != Z_STREAM_END) {
		error_set("Invalid compressed data");
		goto err_ptr;
	}

	if (depth == 8) {
		if (res_size != *dat_size) {
			error_set("Invalid decompressed size");
			goto err_ptr;
		}

		if (pixel == 1)
			adv_png_unfilter_8(width * pixel, height, *dat_ptr, width_align * pixel + 1);
		else if (pixel == 3)
			adv_png_unfilter_24(width * pixel, height, *dat_ptr, width_align * pixel + 1);
		else if (pixel == 4)
			adv_png_unfilter_32(width * pixel, height, *dat_ptr, width_align * pixel + 1);
		else {
			error_set("Unsupported format");
			goto err_ptr;
		}
	} else if (depth == 4) {
		if (res_size != height * (width_align / 2 + 1)) {
			error_set("Invalid decompressed size");
			goto err_ptr;
		}

		adv_png_unfilter_8(width_align / 2, height, *dat_ptr, width_align / 2 + 1);

		adv_png_expand_4(width_align, height, *dat_ptr);
	} else if (depth == 2) {
		if (res_size != height * (width_align / 4 + 1)) {
			error_set("Invalid decompressed size");
			goto err_ptr;
		}

		adv_png_unfilter_8(width_align / 4, height, *dat_ptr, width_align / 4 + 1);

		adv_png_expand_2(width_align, height, *dat_ptr);
	} else if (depth == 1) {
		if (res_size != height * (width_align / 8 + 1)) {
			error_set("Invalid decompressed size");
			goto err_ptr;
		}

		adv_png_unfilter_8(width_align / 8, height, *dat_ptr, width_align / 8 + 1);

		adv_png_expand_1(width_align, height, *dat_ptr);
	}

	if (adv_png_read_iend(f, ptr, ptr_size, type)!=0) {
		goto err_ptr;
	}

	free(ptr);
	return 0;

err_ptr:
	free(ptr);
err:
	free(*dat_ptr);
	free(*pal_ptr);
	free(*rns_ptr);
	return -1;
}