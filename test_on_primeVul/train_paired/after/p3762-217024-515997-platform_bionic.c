void* leak_malloc(size_t bytes)
{
    // allocate enough space infront of the allocation to store the pointer for
    // the alloc structure. This will making free'ing the structer really fast!

    // 1. allocate enough memory and include our header
    // 2. set the base pointer to be right after our header

    size_t size = bytes + sizeof(AllocationEntry);
    if (size < bytes) { // Overflow.
        return NULL;
    }

    void* base = dlmalloc(size);
    if (base != NULL) {
        pthread_mutex_lock(&gAllocationsMutex);

            intptr_t backtrace[BACKTRACE_SIZE];
            size_t numEntries = get_backtrace(backtrace, BACKTRACE_SIZE);

            AllocationEntry* header = (AllocationEntry*)base;
            header->entry = record_backtrace(backtrace, numEntries, bytes);
            header->guard = GUARD;

            // now increment base to point to after our header.
            // this should just work since our header is 8 bytes.
            base = (AllocationEntry*)base + 1;

        pthread_mutex_unlock(&gAllocationsMutex);
    }

    return base;
}