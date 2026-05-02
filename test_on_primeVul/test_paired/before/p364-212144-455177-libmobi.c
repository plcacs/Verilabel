MOBI_RET mobi_parse_huffdic(const MOBIData *m, MOBIHuffCdic *huffcdic) {
    MOBI_RET ret;
    const size_t offset = mobi_get_kf8offset(m);
    if (m->mh == NULL || m->mh->huff_rec_index == NULL || m->mh->huff_rec_count == NULL) {
        debug_print("%s", "HUFF/CDIC records metadata not found in MOBI header\n");
        return MOBI_DATA_CORRUPT;
    }
    const size_t huff_rec_index = *m->mh->huff_rec_index + offset;
    const size_t huff_rec_count = *m->mh->huff_rec_count;
    if (huff_rec_count > HUFF_RECORD_MAXCNT) {
        debug_print("Too many HUFF record (%zu)\n", huff_rec_count);
        return MOBI_DATA_CORRUPT;
    }
    const MOBIPdbRecord *curr = mobi_get_record_by_seqnumber(m, huff_rec_index);
    if (curr == NULL || huff_rec_count < 2) {
        debug_print("%s", "HUFF/CDIC record not found\n");
        return MOBI_DATA_CORRUPT;
    }
    if (curr->size < HUFF_RECORD_MINSIZE) {
        debug_print("HUFF record too short (%zu b)\n", curr->size);
        return MOBI_DATA_CORRUPT;
    }
    ret = mobi_parse_huff(huffcdic, curr);
    if (ret != MOBI_SUCCESS) {
        debug_print("%s", "HUFF parsing failed\n");
        return ret;
    }
    curr = curr->next;
    /* allocate memory for symbols data in each CDIC record */
    huffcdic->symbols = malloc((huff_rec_count - 1) * sizeof(*huffcdic->symbols));
    if (huffcdic->symbols == NULL) {
        debug_print("%s\n", "Memory allocation failed");
        return MOBI_MALLOC_FAILED;
    }
    /* get following CDIC records */
    size_t i = 0;
    while (i < huff_rec_count - 1) {
        if (curr == NULL) {
            debug_print("%s\n", "CDIC record not found");
            return MOBI_DATA_CORRUPT;
        }
        ret = mobi_parse_cdic(huffcdic, curr, i++);
        if (ret != MOBI_SUCCESS) {
            debug_print("%s", "CDIC parsing failed\n");
            return ret;
        }
        curr = curr->next;
    }
    return MOBI_SUCCESS;
}