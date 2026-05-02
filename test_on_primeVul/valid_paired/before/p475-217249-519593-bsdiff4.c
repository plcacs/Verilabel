static PyObject* patch(PyObject* self, PyObject* args)
{
    char *origData, *newData, *diffBlock, *extraBlock, *diffPtr, *extraPtr;
    Py_ssize_t origDataLength, newDataLength, diffBlockLength, extraBlockLength;
    PyObject *controlTuples, *tuple, *results;
    off_t oldpos, newpos, x, y, z;
    int i, j, numTuples;

    if (!PyArg_ParseTuple(args, "s#nO!s#s#",
                          &origData, &origDataLength, &newDataLength,
                          &PyList_Type, &controlTuples,
                          &diffBlock, &diffBlockLength,
                          &extraBlock, &extraBlockLength))
        return NULL;

    /* allocate the memory for the new data */
    newData = PyMem_Malloc(newDataLength + 1);
    if (!newData)
        return PyErr_NoMemory();

    oldpos = 0;
    newpos = 0;
    diffPtr = diffBlock;
    extraPtr = extraBlock;
    numTuples = PyList_GET_SIZE(controlTuples);
    for (i = 0; i < numTuples; i++) {
        tuple = PyList_GET_ITEM(controlTuples, i);
        if (!PyTuple_Check(tuple)) {
            PyMem_Free(newData);
            PyErr_SetString(PyExc_TypeError, "expecting tuple");
            return NULL;
        }
        if (PyTuple_GET_SIZE(tuple) != 3) {
            PyMem_Free(newData);
            PyErr_SetString(PyExc_TypeError, "expecting tuple of size 3");
            return NULL;
        }
        x = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 0));
        y = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 1));
        z = PyLong_AsLong(PyTuple_GET_ITEM(tuple, 2));
        if (newpos + x > newDataLength ||
                diffPtr + x > diffBlock + diffBlockLength ||
                extraPtr + y > extraBlock + extraBlockLength) {
            PyMem_Free(newData);
            PyErr_SetString(PyExc_ValueError, "corrupt patch (overflow)");
            return NULL;
        }
        memcpy(newData + newpos, diffPtr, x);
        diffPtr += x;
        for (j = 0; j < x; j++)
            if ((oldpos + j >= 0) && (oldpos + j < origDataLength))
                newData[newpos + j] += origData[oldpos + j];
        newpos += x;
        oldpos += x;
        memcpy(newData + newpos, extraPtr, y);
        extraPtr += y;
        newpos += y;
        oldpos += z;
    }

    /* confirm that a valid patch was applied */
    if (newpos != newDataLength ||
            diffPtr != diffBlock + diffBlockLength ||
            extraPtr != extraBlock + extraBlockLength) {
        PyMem_Free(newData);
        PyErr_SetString(PyExc_ValueError, "corrupt patch (underflow)");
        return NULL;
    }

    results = PyBytes_FromStringAndSize(newData, newDataLength);
    PyMem_Free(newData);
    return results;
}