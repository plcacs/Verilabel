njs_array_prototype_splice(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    int64_t      i, n, start, length, items, delta, delete;
    njs_int_t    ret;
    njs_value_t  *this, value, del_object;
    njs_array_t  *array, *deleted;

    this = njs_argument(args, 0);

    ret = njs_value_to_object(vm, this);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    ret = njs_object_length(vm, this, &length);
    if (njs_slow_path(ret == NJS_ERROR)) {
        return ret;
    }

    ret = njs_value_to_integer(vm, njs_arg(args, nargs, 1), &start);
    if (njs_slow_path(ret != NJS_OK)) {
        return ret;
    }

    start = (start < 0) ? njs_max(length + start, 0) : njs_min(start, length);

    items = 0;
    delete = 0;

    if (nargs == 2) {
        delete = length - start;

    } else if (nargs > 2) {
        items = nargs - 3;

        ret = njs_value_to_integer(vm, njs_arg(args, nargs, 2), &delete);
        if (njs_slow_path(ret != NJS_OK)) {
            return ret;
        }

        delete = njs_min(njs_max(delete, 0), length - start);
    }

    delta = items - delete;

    if (njs_slow_path((length + delta) > NJS_MAX_LENGTH)) {
        njs_type_error(vm, "Invalid length");
        return NJS_ERROR;
    }

    /* TODO: ArraySpeciesCreate(). */

    deleted = njs_array_alloc(vm, 0, delete, 0);
    if (njs_slow_path(deleted == NULL)) {
        return NJS_ERROR;
    }

    if (njs_fast_path(njs_is_fast_array(this) && deleted->object.fast_array)) {
        array = njs_array(this);
        for (i = 0, n = start; i < delete; i++, n++) {
            deleted->start[i] = array->start[n];
        }

    } else {
        njs_set_array(&del_object, deleted);

        for (i = 0, n = start; i < delete; i++, n++) {
            ret = njs_value_property_i64(vm, this, n, &value);
            if (njs_slow_path(ret == NJS_ERROR)) {
                return NJS_ERROR;
            }

            if (ret == NJS_OK) {
                /* TODO:  CreateDataPropertyOrThrow(). */
                ret = njs_value_property_i64_set(vm, &del_object, i, &value);
                if (njs_slow_path(ret == NJS_ERROR)) {
                    return ret;
                }
            }
        }

        ret = njs_object_length_set(vm, &del_object, delete);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }
    }

    if (njs_fast_path(njs_is_fast_array(this))) {
        array = njs_array(this);

        if (delta != 0) {
            /*
             * Relocate the rest of items.
             * Index of the first item is in "n".
             */
            if (delta > 0) {
                ret = njs_array_expand(vm, array, 0, delta);
                if (njs_slow_path(ret != NJS_OK)) {
                    return ret;
                }
            }

            ret = njs_array_copy_within(vm, this, start + items, start + delete,
                                        array->length - (start + delete), 0);
            if (njs_slow_path(ret != NJS_OK)) {
                return ret;
            }

            array->length += delta;
        }

        /* Copy new items. */

        if (items > 0) {
            memcpy(&array->start[start], &args[3],
                   items * sizeof(njs_value_t));
        }

    } else {

       if (delta != 0) {
           ret = njs_array_copy_within(vm, this, start + items, start + delete,
                                       length - (start + delete), delta < 0);
            if (njs_slow_path(ret != NJS_OK)) {
                return ret;
            }

            for (i = length - 1; i >= length + delta; i--) {
                ret = njs_value_property_i64_delete(vm, this, i, NULL);
                if (njs_slow_path(ret == NJS_ERROR)) {
                    return NJS_ERROR;
                }
            }
       }

        /* Copy new items. */

        for (i = 3, n = start; items-- > 0; i++, n++) {
            ret = njs_value_property_i64_set(vm, this, n, &args[i]);
            if (njs_slow_path(ret == NJS_ERROR)) {
                return NJS_ERROR;
            }
        }

        ret = njs_object_length_set(vm, this, length + delta);
        if (njs_slow_path(ret != NJS_OK)) {
            return NJS_ERROR;
        }
    }

    njs_set_array(&vm->retval, deleted);

    return NJS_OK;
}