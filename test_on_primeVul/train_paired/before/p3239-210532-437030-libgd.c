BGD_DECLARE(void *) gdImagePngPtr (gdImagePtr im, int *size)
{
	void *rv;
	gdIOCtx *out = gdNewDynamicCtx (2048, NULL);
	if (out == NULL) return NULL;
	gdImagePngCtxEx (im, out, -1);
	rv = gdDPExtractData (out, size);
	out->gd_free (out);
	return rv;
}