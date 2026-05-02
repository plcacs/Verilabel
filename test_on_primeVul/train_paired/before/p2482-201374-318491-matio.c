ReadNextFunctionHandle(mat_t *mat, matvar_t *matvar)
{
    int err;
    size_t nelems = 1;

    err = Mat_MulDims(matvar, &nelems);
    matvar->data_size = sizeof(matvar_t *);
    err |= Mul(&matvar->nbytes, nelems, matvar->data_size);
    if ( err )
        return 0;

    matvar->data = malloc(matvar->nbytes);
    if ( matvar->data != NULL ) {
        size_t i;
        matvar_t **functions = (matvar_t **)matvar->data;
        for ( i = 0; i < nelems; i++ ) {
            functions[i] = Mat_VarReadNextInfo(mat);
            err = NULL == functions[i];
            if ( err )
                break;
        }
        if ( err ) {
            free(matvar->data);
            matvar->data = NULL;
            matvar->data_size = 0;
            matvar->nbytes = 0;
        }
    } else {
        matvar->data_size = 0;
        matvar->nbytes = 0;
    }

    return 0;
}