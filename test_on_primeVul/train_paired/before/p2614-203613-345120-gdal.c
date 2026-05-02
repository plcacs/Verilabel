static void* OGRExpatRealloc( void *ptr, size_t size )
{
    if( CanAlloc(size) )
        return realloc(ptr, size);

    free(ptr);
    return nullptr;
}