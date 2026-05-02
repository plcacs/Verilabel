BGD_DECLARE(void *) gdImageBmpPtr(gdImagePtr im, int *size, int compression)
{
	void *rv;
	gdIOCtx *out = gdNewDynamicCtx(2048, NULL);
	if (out == NULL) return NULL;
	if (!_gdImageBmpCtx(im, out, compression))
		rv = gdDPExtractData(out, size);
	else
		rv = NULL;
	out->gd_free(out);
	return rv;
}