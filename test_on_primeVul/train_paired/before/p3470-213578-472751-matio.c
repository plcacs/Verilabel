int SafeMulDims(const matvar_t *matvar, size_t* nelems)
{
    int i;

    for ( i = 0; i < matvar->rank; i++ ) {
        if ( !psnip_safe_size_mul(nelems, *nelems, matvar->dims[i]) ) {
            *nelems = 0;
            return 1;
        }
    }

    return 0;
}