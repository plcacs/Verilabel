static plist_t parse_array_node(struct bplist_data *bplist, const char** bnode, uint64_t size)
{
    uint64_t j;
    uint64_t str_j = 0;
    uint64_t index1;
    plist_data_t data = plist_new_plist_data();
    const char *const end_data = bplist->data + bplist->size;
    const char *index1_ptr = NULL;

    data->type = PLIST_ARRAY;
    data->length = size;

    plist_t node = node_create(NULL, data);

    for (j = 0; j < data->length; j++) {
        str_j = j * bplist->ref_size;
        index1_ptr = (*bnode) + str_j;

        if (index1_ptr < bplist->data || index1_ptr + bplist->ref_size >= end_data) {
            plist_free(node);
            return NULL;
        }

        index1 = UINT_TO_HOST(index1_ptr, bplist->ref_size);

        if (index1 >= bplist->num_objects) {
            plist_free(node);
            return NULL;
        }

        /* process value node */
        plist_t val = parse_bin_node_at_index(bplist, index1);
        if (!val) {
            plist_free(node);
            return NULL;
        }

        node_attach(node, val);
    }

    return node;
}