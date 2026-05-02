static bool checkreturn decode_pointer_field(pb_istream_t *stream, pb_wire_type_t wire_type, pb_field_iter_t *iter)
{
#ifndef PB_ENABLE_MALLOC
    PB_UNUSED(wire_type);
    PB_UNUSED(iter);
    PB_RETURN_ERROR(stream, "no malloc support");
#else
    pb_type_t type;
    pb_decoder_t func;
    
    type = iter->pos->type;
    func = PB_DECODERS[PB_LTYPE(type)];
    
    switch (PB_HTYPE(type))
    {
        case PB_HTYPE_REQUIRED:
        case PB_HTYPE_OPTIONAL:
        case PB_HTYPE_ONEOF:
            if (PB_LTYPE(type) == PB_LTYPE_SUBMESSAGE &&
                *(void**)iter->pData != NULL)
            {
                /* Duplicate field, have to release the old allocation first. */
                pb_release_single_field(iter);
            }
        
            if (PB_HTYPE(type) == PB_HTYPE_ONEOF)
            {
                *(pb_size_t*)iter->pSize = iter->pos->tag;
            }

            if (PB_LTYPE(type) == PB_LTYPE_STRING ||
                PB_LTYPE(type) == PB_LTYPE_BYTES)
            {
                return func(stream, iter->pos, iter->pData);
            }
            else
            {
                if (!allocate_field(stream, iter->pData, iter->pos->data_size, 1))
                    return false;
                
                initialize_pointer_field(*(void**)iter->pData, iter);
                return func(stream, iter->pos, *(void**)iter->pData);
            }
    
        case PB_HTYPE_REPEATED:
            if (wire_type == PB_WT_STRING
                && PB_LTYPE(type) <= PB_LTYPE_LAST_PACKABLE)
            {
                /* Packed array, multiple items come in at once. */
                bool status = true;
                pb_size_t *size = (pb_size_t*)iter->pSize;
                size_t allocated_size = *size;
                void *pItem;
                pb_istream_t substream;
                
                if (!pb_make_string_substream(stream, &substream))
                    return false;
                
                while (substream.bytes_left)
                {
                    if ((size_t)*size + 1 > allocated_size)
                    {
                        /* Allocate more storage. This tries to guess the
                         * number of remaining entries. Round the division
                         * upwards. */
                        allocated_size += (substream.bytes_left - 1) / iter->pos->data_size + 1;
                        
                        if (!allocate_field(&substream, iter->pData, iter->pos->data_size, allocated_size))
                        {
                            status = false;
                            break;
                        }
                    }

                    /* Decode the array entry */
                    pItem = *(char**)iter->pData + iter->pos->data_size * (*size);
                    initialize_pointer_field(pItem, iter);
                    if (!func(&substream, iter->pos, pItem))
                    {
                        status = false;
                        break;
                    }
                    
                    if (*size == PB_SIZE_MAX)
                    {
#ifndef PB_NO_ERRMSG
                        stream->errmsg = "too many array entries";
#endif
                        status = false;
                        break;
                    }
                    
                    (*size)++;
                }
                if (!pb_close_string_substream(stream, &substream))
                    return false;
                
                return status;
            }
            else
            {
                /* Normal repeated field, i.e. only one item at a time. */
                pb_size_t *size = (pb_size_t*)iter->pSize;
                void *pItem;
                
                if (*size == PB_SIZE_MAX)
                    PB_RETURN_ERROR(stream, "too many array entries");
                
                (*size)++;
                if (!allocate_field(stream, iter->pData, iter->pos->data_size, *size))
                    return false;
            
                pItem = *(char**)iter->pData + iter->pos->data_size * (*size - 1);
                initialize_pointer_field(pItem, iter);
                return func(stream, iter->pos, pItem);
            }

        default:
            PB_RETURN_ERROR(stream, "invalid field type");
    }
#endif
}