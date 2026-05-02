initialize(int argc, VALUE argv[], VALUE self)
{
    ffi_cif * cif;
    ffi_type **arg_types, *rtype;
    ffi_status result;
    VALUE ptr, args, ret_type, abi, kwds;
    long i, len;
    int nabi;
    void *cfunc;

    rb_scan_args(argc, argv, "31:", &ptr, &args, &ret_type, &abi, &kwds);
    ptr = rb_Integer(ptr);
    cfunc = NUM2PTR(ptr);
    PTR2NUM(cfunc);
    nabi = NIL_P(abi) ? FFI_DEFAULT_ABI : NUM2INT(abi);
    abi = INT2FIX(nabi);
    i = NUM2INT(ret_type);
    rtype = INT2FFI_TYPE(i);
    ret_type = INT2FIX(i);

    Check_Type(args, T_ARRAY);
    len = RARRAY_LENINT(args);
    Check_Max_Args_Long("args", len);
    ary = rb_ary_subseq(ary, 0, len);
    for (i = 0; i < RARRAY_LEN(args); i++) {
	VALUE a = RARRAY_PTR(args)[i];
	int type = NUM2INT(a);
	(void)INT2FFI_TYPE(type); /* raise */
	if (INT2FIX(type) != a) rb_ary_store(ary, i, INT2FIX(type));
    }
    OBJ_FREEZE(ary);

    rb_iv_set(self, "@ptr", ptr);
    rb_iv_set(self, "@args", args);
    rb_iv_set(self, "@return_type", ret_type);
    rb_iv_set(self, "@abi", abi);

    if (!NIL_P(kwds)) rb_hash_foreach(kwds, parse_keyword_arg_i, self);

    TypedData_Get_Struct(self, ffi_cif, &function_data_type, cif);

    arg_types = xcalloc(len + 1, sizeof(ffi_type *));

    for (i = 0; i < RARRAY_LEN(args); i++) {
	int type = NUM2INT(RARRAY_AREF(args, i));
	arg_types[i] = INT2FFI_TYPE(type);
    }
    arg_types[len] = NULL;

    result = ffi_prep_cif(cif, nabi, len, rtype, arg_types);

    if (result)
	rb_raise(rb_eRuntimeError, "error creating CIF %d", result);

    return self;
}