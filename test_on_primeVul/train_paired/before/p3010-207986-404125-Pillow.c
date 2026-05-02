PyImaging_LibTiffDecoderNew(PyObject* self, PyObject* args)
{
    ImagingDecoderObject* decoder;
    char* mode;
    char* rawmode;
    char* compname;
    int compression;
    int fp;

    if (! PyArg_ParseTuple(args, "sssi", &mode, &rawmode, &compname, &fp))
        return NULL;

    TRACE(("new tiff decoder %s\n", compname));

    /* UNDONE -- we can probably do almost any arbitrary compression here,
     * since we're effective passing in the whole file in one shot and
     * getting back the data row by row. V2 maybe
     */

    if (strcasecmp(compname, "tiff_ccitt") == 0) {
        compression = COMPRESSION_CCITTRLE;

    } else if (strcasecmp(compname, "group3") == 0) {
        compression = COMPRESSION_CCITTFAX3;

    } else if (strcasecmp(compname, "group4") == 0) {
        compression = COMPRESSION_CCITTFAX4;

    } else if (strcasecmp(compname, "tiff_raw_16") == 0) {
        compression = COMPRESSION_CCITTRLEW;

    } else {
        PyErr_SetString(PyExc_ValueError, "unknown compession");
        return NULL;
    }

    decoder = PyImaging_DecoderNew(sizeof(TIFFSTATE));
    if (decoder == NULL)
        return NULL;

    if (get_unpacker(decoder, mode, rawmode) < 0)
        return NULL;

    if (! ImagingLibTiffInit(&decoder->state, compression, fp)) {
        Py_DECREF(decoder);
        PyErr_SetString(PyExc_RuntimeError, "tiff codec initialization failed");
        return NULL;
    }

    decoder->decode  = ImagingLibTiffDecode;

    return (PyObject*) decoder;
}