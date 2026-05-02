function_call(int argc, VALUE argv[], VALUE self)
{
    ffi_cif * cif;
    fiddle_generic retval;
    fiddle_generic *generic_args;
    void **values;
    VALUE cfunc, types, cPointer;
    int i;

    cfunc    = rb_iv_get(self, "@ptr");
    types    = rb_iv_get(self, "@args");
    cPointer = rb_const_get(mFiddle, rb_intern("Pointer"));

    if(argc != RARRAY_LENINT(types)) {
	rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)",
		argc, RARRAY_LENINT(types));
    }

    TypedData_Get_Struct(self, ffi_cif, &function_data_type, cif);

    values = xcalloc((size_t)argc + 1, (size_t)sizeof(void *));
    generic_args = xcalloc((size_t)argc, (size_t)sizeof(fiddle_generic));

    for (i = 0; i < argc; i++) {
	VALUE type = RARRAY_PTR(types)[i];
	VALUE src = argv[i];

	if(NUM2INT(type) == TYPE_VOIDP) {
	    if(NIL_P(src)) {
		src = INT2FIX(0);
	    } else if(cPointer != CLASS_OF(src)) {
		src = rb_funcall(cPointer, rb_intern("[]"), 1, src);
	    }
	    src = rb_Integer(src);
	}

	VALUE2GENERIC(NUM2INT(type), src, &generic_args[i]);
	values[i] = (void *)&generic_args[i];
    }
    values[argc] = NULL;

    ffi_call(cif, NUM2PTR(rb_Integer(cfunc)), &retval, values);

    rb_funcall(mFiddle, rb_intern("last_error="), 1, INT2NUM(errno));
#if defined(_WIN32)
    rb_funcall(mFiddle, rb_intern("win32_last_error="), 1, INT2NUM(errno));
#endif

    xfree(values);
    xfree(generic_args);

    return GENERIC2VALUE(rb_iv_get(self, "@return_type"), retval);
}