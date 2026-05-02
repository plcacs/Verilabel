static plist_t parse_dict_node(struct bplist_data *bplist, const char** bnode, uint64_t size)
{
    uint64_t j;
    uint64_t str_i = 0, str_j = 0;
    uint64_t index1, index2;
    plist_data_t data = plist_new_plist_data();
    const char *const end_data = bplist->data + bplist->size;
    const char *index1_ptr = NULL;
    const char *index2_ptr = NULL;

    data->type = PLIST_DICT;
    data->length = size;

    plist_t node = node_create(NULL, data);

    for (j = 0; j < data->length; j++) {
        str_i = j * bplist->dict_size;
        str_j = (j + size) * bplist->dict_size;
        index1_ptr = (*bnode) + str_i;
        index2_ptr = (*bnode) + str_j;

        if ((index1_ptr < bplist->data || index1_ptr + bplist->dict_size >= end_data) ||
            (index2_ptr < bplist->data || index2_ptr + bplist->dict_size >= end_data)) {
            plist_free(node);
            return NULL;
        }

        index1 = UINT_TO_HOST(index1_ptr, bplist->dict_size);
        index2 = UINT_TO_HOST(index2_ptr, bplist->dict_size);

        if (index1 >= bplist->num_objects) {
            plist_free(node);
            return NULL;
        }
        if (index2 >= bplist->num_objects) {
            plist_free(node);
            return NULL;
        }

        /* process key node */
        plist_t key = parse_bin_node_at_index(bplist, index1);
        if (!key) {
            plist_free(node);
            return NULL;
        }
        /* enforce key type */
        plist_get_data(key)->type = PLIST_KEY;
        if (!plist_get_data(key)->strval) {
            fprintf(stderr, "ERROR: Malformed binary plist dict, invalid key node encountered!\n");
            plist_free(key);
            plist_free(node);
            return NULL;
        }

        /* process value node */
        plist_t val = parse_bin_node_at_index(bplist, index2);
        if (!val) {
            plist_free(key);
            plist_free(node);
            return NULL;
        }

        node_attach(node, key);
        node_attach(node, val);
    }

    return node;
}