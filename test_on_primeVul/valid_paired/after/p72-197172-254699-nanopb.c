static bool pb_release_union_field(pb_istream_t *stream, pb_field_iter_t *field)
{
    pb_field_iter_t old_field = *field;
    pb_size_t old_tag = *(pb_size_t*)field->pSize; /* Previous which_ value */
    pb_size_t new_tag = field->tag; /* New which_ value */

    if (old_tag == 0)
        return true; /* Ok, no old data in union */

    if (old_tag == new_tag)
        return true; /* Ok, old data is of same type => merge */

    /* Release old data. The find can fail if the message struct contains
     * invalid data. */
    if (!pb_field_iter_find(&old_field, old_tag))
        PB_RETURN_ERROR(stream, "invalid union tag");

    pb_release_single_field(&old_field);

    if (PB_ATYPE(field->type) == PB_ATYPE_POINTER)
    {
        /* Initialize the pointer to NULL to make sure it is valid
         * even in case of error return. */
        *(void**)field->pField = NULL;
        field->pData = NULL;
    }

    return true;
}