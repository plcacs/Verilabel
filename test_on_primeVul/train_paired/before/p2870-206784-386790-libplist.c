static plist_t parse_array_node(struct bplist_data *bplist, const char** bnode, uint64_t size)
{
    uint64_t j;
    uint32_t str_j = 0;
    uint32_t index1;

    plist_data_t data = plist_new_plist_data();

    data->type = PLIST_ARRAY;
    data->length = size;

    plist_t node = node_create(NULL, data);

    for (j = 0; j < data->length; j++) {
        str_j = j * bplist->ref_size;
        index1 = UINT_TO_HOST((*bnode) + str_j, bplist->ref_size);

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