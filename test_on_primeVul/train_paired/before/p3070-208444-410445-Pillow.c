PyImaging_MapBuffer(PyObject* self, PyObject* args)
{
    Py_ssize_t y, size;
    Imaging im;

    PyObject* target;
    Py_buffer view;
    char* mode;
    char* codec;
    PyObject* bbox;
    Py_ssize_t offset;
    int xsize, ysize;
    int stride;
    int ystep;

    if (!PyArg_ParseTuple(args, "O(ii)sOn(sii)", &target, &xsize, &ysize,
                          &codec, &bbox, &offset, &mode, &stride, &ystep))
        return NULL;

    if (!PyImaging_CheckBuffer(target)) {
        PyErr_SetString(PyExc_TypeError, "expected string or buffer");
        return NULL;
    }

    if (stride <= 0) {
        if (!strcmp(mode, "L") || !strcmp(mode, "P"))
            stride = xsize;
        else if (!strncmp(mode, "I;16", 4))
            stride = xsize * 2;
        else
            stride = xsize * 4;
    }

    size = (Py_ssize_t) ysize * stride;

    /* check buffer size */
    if (PyImaging_GetBuffer(target, &view) < 0)
        return NULL;

    if (view.len < 0) {
        PyErr_SetString(PyExc_ValueError, "buffer has negative size");
        return NULL;
    }
    if (offset + size > view.len) {
        PyErr_SetString(PyExc_ValueError, "buffer is not large enough");
        return NULL;
    }

    im = ImagingNewPrologueSubtype(
        mode, xsize, ysize, sizeof(ImagingBufferInstance)
        );
    if (!im)
        return NULL;

    /* setup file pointers */
    if (ystep > 0)
        for (y = 0; y < ysize; y++)
            im->image[y] = (char*)view.buf + offset + y * stride;
    else
        for (y = 0; y < ysize; y++)
            im->image[ysize-y-1] = (char*)view.buf + offset + y * stride;

    im->destroy = mapping_destroy_buffer;

    Py_INCREF(target);
    ((ImagingBufferInstance*) im)->target = target;
    ((ImagingBufferInstance*) im)->view = view;

    if (!ImagingNewEpilogue(im))
        return NULL;

    return PyImagingNew(im);
}