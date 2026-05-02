gdImagePtr gdImageCrop(gdImagePtr src, const gdRectPtr crop)
{
	gdImagePtr dst;

	if (src->trueColor) {
		dst = gdImageCreateTrueColor(crop->width, crop->height);
		gdImageSaveAlpha(dst, 1);
	} else {
		dst = gdImageCreate(crop->width, crop->height);
		gdImagePaletteCopy(dst, src);
	}
	dst->transparent = src->transparent;

	if (src->sx < (crop->x + crop->width -1)) {
		crop->width = src->sx - crop->x + 1;
	}
	if (src->sy < (crop->y + crop->height -1)) {
		crop->height = src->sy - crop->y + 1;
	}
#if 0
printf("rect->x: %i\nrect->y: %i\nrect->width: %i\nrect->height: %i\n", crop->x, crop->y, crop->width, crop->height);
#endif
	if (dst == NULL) {
		return NULL;
	} else {
		int y = crop->y;
		if (src->trueColor) {
			unsigned int dst_y = 0;
			while (y < (crop->y + (crop->height - 1))) {
				/* TODO: replace 4 w/byte per channel||pitch once available */
				memcpy(dst->tpixels[dst_y++], src->tpixels[y++] + crop->x, crop->width * 4);
			}
		} else {
			int x;
			for (y = crop->y; y < (crop->y + (crop->height - 1)); y++) {
				for (x = crop->x; x < (crop->x + (crop->width - 1)); x++) {
					dst->pixels[y - crop->y][x - crop->x] = src->pixels[y][x];
				}
			}
		}
		return dst;
	}
}