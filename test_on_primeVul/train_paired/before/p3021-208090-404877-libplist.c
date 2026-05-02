PLIST_API void plist_from_bin(const char *plist_bin, uint32_t length, plist_t * plist)
{
    bplist_trailer_t *trailer = NULL;
    uint8_t offset_size = 0;
    uint8_t ref_size = 0;
    uint64_t num_objects = 0;
    uint64_t root_object = 0;
    const char *offset_table = NULL;
    const char *start_data = NULL;
    const char *end_data = NULL;

    //first check we have enough data
    if (!(length >= BPLIST_MAGIC_SIZE + BPLIST_VERSION_SIZE + sizeof(bplist_trailer_t))) {
        PLIST_BIN_ERR("plist data is to small to hold a binary plist\n");
        return;
    }
    //check that plist_bin in actually a plist
    if (memcmp(plist_bin, BPLIST_MAGIC, BPLIST_MAGIC_SIZE) != 0) {
        PLIST_BIN_ERR("bplist magic mismatch\n");
        return;
    }
    //check for known version
    if (memcmp(plist_bin + BPLIST_MAGIC_SIZE, BPLIST_VERSION, BPLIST_VERSION_SIZE) != 0) {
        PLIST_BIN_ERR("unsupported binary plist version '%.2s\n", plist_bin+BPLIST_MAGIC_SIZE);
        return;
    }

    start_data = plist_bin + BPLIST_MAGIC_SIZE + BPLIST_VERSION_SIZE;
    end_data = plist_bin + length - sizeof(bplist_trailer_t);

    //now parse trailer
    trailer = (bplist_trailer_t*)end_data;

    offset_size = trailer->offset_size;
    ref_size = trailer->ref_size;
    num_objects = be64toh(trailer->num_objects);
    root_object = be64toh(trailer->root_object_index);
    offset_table = (char *)(plist_bin + be64toh(trailer->offset_table_offset));

    if (num_objects == 0) {
        PLIST_BIN_ERR("number of objects must be larger than 0\n");
        return;
    }

    if (offset_size == 0) {
        PLIST_BIN_ERR("offset size in trailer must be larger than 0\n");
        return;
    }

    if (ref_size == 0) {
        PLIST_BIN_ERR("object reference size in trailer must be larger than 0\n");
        return;
    }

    if (root_object >= num_objects) {
        PLIST_BIN_ERR("root object index (%" PRIu64 ") must be smaller than number of objects (%" PRIu64 ")\n", root_object, num_objects);
        return;
    }

    if (offset_table < start_data || offset_table >= end_data) {
        PLIST_BIN_ERR("offset table offset points outside of valid range\n");
        return;
    }

    if (num_objects * offset_size < num_objects) {
        PLIST_BIN_ERR("integer overflow when calculating offset table size (too many objects)\n");
        return;
    }

    if ((uint64_t)offset_table + num_objects * offset_size > (uint64_t)end_data) {
        PLIST_BIN_ERR("offset table points outside of valid range\n");
        return;
    }

    struct bplist_data bplist;
    bplist.data = plist_bin;
    bplist.size = length;
    bplist.num_objects = num_objects;
    bplist.ref_size = ref_size;
    bplist.offset_size = offset_size;
    bplist.offset_table = offset_table;
    bplist.level = 0;
    bplist.used_indexes = plist_new_array();

    if (!bplist.used_indexes) {
        PLIST_BIN_ERR("failed to create array to hold used node indexes. Out of memory?\n");
        return;
    }

    *plist = parse_bin_node_at_index(&bplist, root_object);

    plist_free(bplist.used_indexes);
}