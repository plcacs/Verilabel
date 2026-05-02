static MOBI_RET mobi_parse_index_entry(MOBIIndx *indx, const MOBIIdxt idxt, const MOBITagx *tagx, const MOBIOrdt *ordt, MOBIBuffer *buf, const size_t curr_number) {
    if (indx == NULL) {
        debug_print("%s", "INDX structure not initialized\n");
        return MOBI_INIT_FAILED;
    }
    const size_t entry_offset = indx->entries_count;
    const size_t entry_length = idxt.offsets[curr_number + 1] - idxt.offsets[curr_number];
    mobi_buffer_setpos(buf, idxt.offsets[curr_number]);
    size_t entry_number = curr_number + entry_offset;
    if (entry_number >= indx->total_entries_count) {
        debug_print("Entry number beyond array: %zu\n", entry_number);
        return MOBI_DATA_CORRUPT;
    }
    /* save original record maxlen */
    const size_t buf_maxlen = buf->maxlen;
    if (buf->offset + entry_length >= buf_maxlen) {
        debug_print("Entry length too long: %zu\n", entry_length);
        return MOBI_DATA_CORRUPT;
    }
    buf->maxlen = buf->offset + entry_length;
    size_t label_length = mobi_buffer_get8(buf);
    if (label_length > entry_length) {
        debug_print("Label length too long: %zu\n", label_length);
        return MOBI_DATA_CORRUPT;
    }
    char text[INDX_LABEL_SIZEMAX];
    /* FIXME: what is ORDT1 for? */
    if (ordt->ordt2) {
        label_length = mobi_getstring_ordt(ordt, buf, (unsigned char*) text, label_length);
    } else {
        label_length = mobi_indx_get_label((unsigned char*) text, buf, label_length, indx->ligt_entries_count);
    }
    indx->entries[entry_number].label = malloc(label_length + 1);
    if (indx->entries[entry_number].label == NULL) {
        debug_print("Memory allocation failed (%zu bytes)\n", label_length);
        return MOBI_MALLOC_FAILED;
    }
    strncpy(indx->entries[entry_number].label, text, label_length + 1);
    //debug_print("tag label[%zu]: %s\n", entry_number, indx->entries[entry_number].label);
    unsigned char *control_bytes;
    control_bytes = buf->data + buf->offset;
    mobi_buffer_seek(buf, (int) tagx->control_byte_count);
    indx->entries[entry_number].tags_count = 0;
    indx->entries[entry_number].tags = NULL;
    if (tagx->tags_count > 0) {
        typedef struct {
            uint8_t tag;
            uint8_t tag_value_count;
            uint32_t value_count;
            uint32_t value_bytes;
        } MOBIPtagx;
        MOBIPtagx *ptagx = malloc(tagx->tags_count * sizeof(MOBIPtagx));
        if (ptagx == NULL) {
            debug_print("Memory allocation failed (%zu bytes)\n", tagx->tags_count * sizeof(MOBIPtagx));
            return MOBI_MALLOC_FAILED;
        }
        uint32_t ptagx_count = 0;
        size_t len;
        size_t i = 0;
        while (i < tagx->tags_count) {
            if (tagx->tags[i].control_byte == 1) {
                control_bytes++;
                i++;
                continue;
            }
            uint32_t value = control_bytes[0] & tagx->tags[i].bitmask;
            if (value != 0) {
                /* FIXME: is it safe to use MOBI_NOTSET? */
                uint32_t value_count = MOBI_NOTSET;
                uint32_t value_bytes = MOBI_NOTSET;
                /* all bits of masked value are set */
                if (value == tagx->tags[i].bitmask) {
                    /* more than 1 bit set */
                    if (mobi_bitcount(tagx->tags[i].bitmask) > 1) {
                        /* read value bytes from entry */
                        len = 0;
                        value_bytes = mobi_buffer_get_varlen(buf, &len);
                    } else {
                        value_count = 1;
                    }
                } else {
                    uint8_t mask = tagx->tags[i].bitmask;
                    while ((mask & 1) == 0) {
                        mask >>= 1;
                        value >>= 1;
                    }
                    value_count = value;
                }
                ptagx[ptagx_count].tag = tagx->tags[i].tag;
                ptagx[ptagx_count].tag_value_count = tagx->tags[i].values_count;
                ptagx[ptagx_count].value_count = value_count;
                ptagx[ptagx_count].value_bytes = value_bytes;
                ptagx_count++;
            }
            i++;
        }
        indx->entries[entry_number].tags = malloc(tagx->tags_count * sizeof(MOBIIndexTag));
        if (indx->entries[entry_number].tags == NULL) {
            debug_print("Memory allocation failed (%zu bytes)\n", tagx->tags_count * sizeof(MOBIIndexTag));
            free(ptagx);
            return MOBI_MALLOC_FAILED;
        }
        i = 0;
        while (i < ptagx_count) {
            uint32_t tagvalues_count = 0;
            /* FIXME: is it safe to use MOBI_NOTSET? */
            /* value count is set */
            uint32_t tagvalues[INDX_TAGVALUES_MAX];
            if (ptagx[i].value_count != MOBI_NOTSET) {
                size_t count = ptagx[i].value_count * ptagx[i].tag_value_count;
                while (count-- && tagvalues_count < INDX_TAGVALUES_MAX) {
                    len = 0;
                    const uint32_t value_bytes = mobi_buffer_get_varlen(buf, &len);
                    tagvalues[tagvalues_count++] = value_bytes;
                }
            /* value count is not set */
            } else {
                /* read value_bytes bytes */
                len = 0;
                while (len < ptagx[i].value_bytes && tagvalues_count < INDX_TAGVALUES_MAX) {
                    const uint32_t value_bytes = mobi_buffer_get_varlen(buf, &len);
                    tagvalues[tagvalues_count++] = value_bytes;
                }
            }
            if (tagvalues_count) {
                const size_t arr_size = tagvalues_count * sizeof(*indx->entries[entry_number].tags[i].tagvalues);
                indx->entries[entry_number].tags[i].tagvalues = malloc(arr_size);
                if (indx->entries[entry_number].tags[i].tagvalues == NULL) {
                    debug_print("Memory allocation failed (%zu bytes)\n", arr_size);
                    free(ptagx);
                    return MOBI_MALLOC_FAILED;
                }
                memcpy(indx->entries[entry_number].tags[i].tagvalues, tagvalues, arr_size);
            } else {
                indx->entries[entry_number].tags[i].tagvalues = NULL;
            }
            indx->entries[entry_number].tags[i].tagid = ptagx[i].tag;
            indx->entries[entry_number].tags[i].tagvalues_count = tagvalues_count;
            indx->entries[entry_number].tags_count++;
            i++;
        }
        free(ptagx);
    }
    /* restore buffer maxlen */
    buf->maxlen = buf_maxlen;
    return MOBI_SUCCESS;
}