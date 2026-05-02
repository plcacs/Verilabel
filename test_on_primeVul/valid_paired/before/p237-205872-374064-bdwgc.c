GC_API GC_ATTR_MALLOC void * GC_CALL GC_calloc_explicitly_typed(size_t n,
                                                        size_t lb, GC_descr d)
{
    word *op;
    size_t lg;
    GC_descr simple_descr;
    complex_descriptor *complex_descr;
    int descr_type;
    struct LeafDescriptor leaf;

    GC_ASSERT(GC_explicit_typing_initialized);
    descr_type = GC_make_array_descriptor((word)n, (word)lb, d, &simple_descr,
                                          &complex_descr, &leaf);
    switch(descr_type) {
        case NO_MEM: return(0);
        case SIMPLE: return(GC_malloc_explicitly_typed(n*lb, simple_descr));
        case LEAF:
            lb *= n;
            lb += sizeof(struct LeafDescriptor) + TYPD_EXTRA_BYTES;
            break;
        case COMPLEX:
            lb *= n;
            lb += TYPD_EXTRA_BYTES;
            break;
    }
    op = GC_malloc_kind(lb, GC_array_kind);
    if (EXPECT(NULL == op, FALSE))
        return NULL;
    lg = SMALL_OBJ(lb) ? GC_size_map[lb] : BYTES_TO_GRANULES(GC_size(op));
    if (descr_type == LEAF) {
       /* Set up the descriptor inside the object itself. */
       volatile struct LeafDescriptor * lp =
           (struct LeafDescriptor *)
               (op + GRANULES_TO_WORDS(lg)
                - (BYTES_TO_WORDS(sizeof(struct LeafDescriptor)) + 1));

       lp -> ld_tag = LEAF_TAG;
       lp -> ld_size = leaf.ld_size;
       lp -> ld_nelements = leaf.ld_nelements;
       lp -> ld_descriptor = leaf.ld_descriptor;
       ((volatile word *)op)[GRANULES_TO_WORDS(lg) - 1] = (word)lp;
   } else {
#    ifndef GC_NO_FINALIZATION
       size_t lw = GRANULES_TO_WORDS(lg);

       op[lw - 1] = (word)complex_descr;
       /* Make sure the descriptor is cleared once there is any danger  */
       /* it may have been collected.                                   */
       if (EXPECT(GC_general_register_disappearing_link(
                                                (void **)(op + lw - 1), op)
                  == GC_NO_MEMORY, FALSE))
#    endif
       {
           /* Couldn't register it due to lack of memory.  Punt.        */
           /* This will probably fail too, but gives the recovery code  */
           /* a chance.                                                 */
            return GC_malloc(lb);
       }
   }
   return op;
}