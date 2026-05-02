BGD_DECLARE(void *) gdImagePngPtr (gdImagePtr im, int *size)
{
	void *rv;
	gdIOCtx *out = gdNewDynamicCtx (2048, NULL);
	if (out == NULL) return NULL;
	if (!_gdImagePngCtxEx (im, out, -1)) {
		rv = gdDPExtractData (out, size);
	} else {
		rv = NULL;
	}
	out->gd_free (out);
	return rv;
}