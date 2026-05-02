void* leak_memalign(size_t alignment, size_t bytes)
{
    // we can just use malloc
    if (alignment <= MALLOC_ALIGNMENT)
        return leak_malloc(bytes);

    // need to make sure it's a power of two
    if (alignment & (alignment-1))
        alignment = 1L << (31 - __builtin_clz(alignment));

    // here, aligment is at least MALLOC_ALIGNMENT<<1 bytes
    // we will align by at least MALLOC_ALIGNMENT bytes
    // and at most alignment-MALLOC_ALIGNMENT bytes
    size_t size = (alignment-MALLOC_ALIGNMENT) + bytes;
    if (size < bytes) { // Overflow.
        return NULL;
    }

    void* base = leak_malloc(size);
    if (base != NULL) {
        intptr_t ptr = (intptr_t)base;
        if ((ptr % alignment) == 0)
            return base;

        // align the pointer
        ptr += ((-ptr) % alignment);

        // there is always enough space for the base pointer and the guard
        ((void**)ptr)[-1] = MEMALIGN_GUARD;
        ((void**)ptr)[-2] = base;

        return (void*)ptr;
    }
    return base;
}