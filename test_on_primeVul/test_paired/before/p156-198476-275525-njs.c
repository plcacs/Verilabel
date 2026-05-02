njs_await_fulfilled(njs_vm_t *vm, njs_value_t *args, njs_uint_t nargs,
    njs_index_t unused)
{
    njs_int_t           ret;
    njs_value_t         **cur_local, **cur_closures, **cur_temp, *value;
    njs_frame_t         *frame, *async_frame;
    njs_function_t      *function;
    njs_async_ctx_t     *ctx;
    njs_native_frame_t  *top, *async;

    ctx = vm->top_frame->function->context;

    value = njs_arg(args, nargs, 1);
    if (njs_is_error(value)) {
        goto failed;
    }

    async_frame = ctx->await;
    async = &async_frame->native;
    async->previous = vm->top_frame;

    function = async->function;

    cur_local = vm->levels[NJS_LEVEL_LOCAL];
    cur_closures = vm->levels[NJS_LEVEL_CLOSURE];
    cur_temp = vm->levels[NJS_LEVEL_TEMP];
    top = vm->top_frame;
    frame = vm->active_frame;

    vm->levels[NJS_LEVEL_LOCAL] = async->local;
    vm->levels[NJS_LEVEL_CLOSURE] = njs_function_closures(async->function);
    vm->levels[NJS_LEVEL_TEMP] = async->temp;

    vm->top_frame = async;
    vm->active_frame = async_frame;

    *njs_scope_value(vm, ctx->index) = *value;
    vm->retval = *value;

    vm->top_frame->retval = &vm->retval;

    function->context = ctx->capability;
    function->await = ctx;

    ret = njs_vmcode_interpreter(vm, ctx->pc);

    function->context = NULL;
    function->await = NULL;

    vm->levels[NJS_LEVEL_LOCAL] = cur_local;
    vm->levels[NJS_LEVEL_CLOSURE] = cur_closures;
    vm->levels[NJS_LEVEL_TEMP] = cur_temp;

    vm->top_frame = top;
    vm->active_frame = frame;

    if (ret == NJS_OK) {
        ret = njs_function_call(vm, njs_function(&ctx->capability->resolve),
                            &njs_value_undefined, &vm->retval, 1, &vm->retval);

        njs_async_context_free(vm, ctx);

    } else if (ret == NJS_AGAIN) {
        ret = NJS_OK;

    } else if (ret == NJS_ERROR) {
        if (njs_is_memory_error(vm, &vm->retval)) {
            return NJS_ERROR;
        }

        value = &vm->retval;

        goto failed;
    }

    return ret;

failed:

    (void) njs_function_call(vm, njs_function(&ctx->capability->reject),
                             &njs_value_undefined, value, 1, &vm->retval);

    njs_async_context_free(vm, ctx);

    return NJS_ERROR;
}