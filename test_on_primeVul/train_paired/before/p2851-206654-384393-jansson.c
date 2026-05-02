void json_object_seed(size_t seed) {
    uint32_t new_seed = (uint32_t)seed;

    if (hashtable_seed == 0) {
        if (__atomic_test_and_set(&seed_initialized, __ATOMIC_RELAXED) == 0) {
            /* Do the seeding ourselves */
            if (new_seed == 0)
                new_seed = generate_seed();

            __atomic_store_n(&hashtable_seed, new_seed, __ATOMIC_ACQ_REL);
        } else {
            /* Wait for another thread to do the seeding */
            do {
#ifdef HAVE_SCHED_YIELD
                sched_yield();
#endif
            } while(__atomic_load_n(&hashtable_seed, __ATOMIC_ACQUIRE) == 0);
        }
    }
}