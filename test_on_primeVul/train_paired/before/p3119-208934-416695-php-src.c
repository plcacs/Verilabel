gdImagePtr gdImageScaleTwoPass(const gdImagePtr src, const unsigned int src_width, const unsigned int src_height, const unsigned int new_width, const unsigned int new_height)
{
	gdImagePtr tmp_im;
	gdImagePtr dst;

	/* Convert to truecolor if it isn't; this code requires it. */
	if (!src->trueColor) {
		gdImagePaletteToTrueColor(src);
	}

	tmp_im = gdImageCreateTrueColor(new_width, src_height);
	if (tmp_im == NULL) {
		return NULL;
	}
	gdImageSetInterpolationMethod(tmp_im, src->interpolation_id);
	_gdScaleHoriz(src, src_width, src_height, tmp_im, new_width, src_height);

	dst = gdImageCreateTrueColor(new_width, new_height);
	if (dst == NULL) {
		gdFree(tmp_im);
		return NULL;
	}
	gdImageSetInterpolationMethod(dst, src->interpolation_id);
	_gdScaleVert(tmp_im, new_width, src_height, dst, new_width, new_height);
	gdFree(tmp_im);

	return dst;
}