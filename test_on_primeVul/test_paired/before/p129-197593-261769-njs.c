njs_function_frame_save(njs_vm_t *vm, njs_frame_t *frame, u_char *pc)
{
    size_t              value_count, n;
    njs_value_t         *start, *end, *p, **new, *value, **local;
    njs_function_t      *function;
    njs_native_frame_t  *active, *native;

    *frame = *vm->active_frame;
    frame->previous_active_frame = NULL;

    native = &frame->native;

    active = &vm->active_frame->native;
    value_count = njs_function_frame_value_count(active);

    function = active->function;

    new = (njs_value_t **) ((u_char *) native + NJS_FRAME_SIZE);
    value = (njs_value_t *) (new + value_count
                             + function->u.lambda->temp);


    native->arguments = value;
    native->arguments_offset = value + (function->args_offset - 1);
    native->local = new + njs_function_frame_args_count(active);
    native->temp = new + value_count;
    native->pc = pc;

    start = njs_function_frame_values(active, &end);
    p = native->arguments;

    while (start < end) {
        *p = *start++;
        *new++ = p++;
    }

    /* Move all arguments. */

    p = native->arguments;
    local = native->local + function->args_offset;

    for (n = 0; n < function->args_count; n++) {
        if (!njs_is_valid(p)) {
            njs_set_undefined(p);
        }

        *local++ = p++;
    }

    return NJS_OK;
}