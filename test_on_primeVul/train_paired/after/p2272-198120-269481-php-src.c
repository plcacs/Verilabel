PHP_FUNCTION(imagetruecolortopalette)
{
	zval *IM;
	zend_bool dither;
	zend_long ncolors;
	gdImagePtr im;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "rbl", &IM, &dither, &ncolors) == FAILURE)  {
		return;
	}

	if ((im = (gdImagePtr)zend_fetch_resource(Z_RES_P(IM), "Image", le_gd)) == NULL) {
		RETURN_FALSE;
	}

	if (ncolors <= 0 || ZEND_LONG_INT_OVFL(ncolors)) {
		php_error_docref(NULL, E_WARNING, "Number of colors has to be greater than zero and no more than %d", INT_MAX);
		RETURN_FALSE;
	}
	gdImageTrueColorToPalette(im, dither, (int)ncolors);

	RETURN_TRUE;
}