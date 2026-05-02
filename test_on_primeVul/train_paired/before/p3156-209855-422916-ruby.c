initialize(int argc, VALUE argv[], VALUE self)
{
    ffi_cif * cif;
    ffi_type **arg_types;
    ffi_status result;
    VALUE ptr, args, ret_type, abi, kwds;
    long i;

    rb_scan_args(argc, argv, "31:", &ptr, &args, &ret_type, &abi, &kwds);
    if(NIL_P(abi)) abi = INT2NUM(FFI_DEFAULT_ABI);

    Check_Type(args, T_ARRAY);
    Check_Max_Args("args", RARRAY_LENINT(args));

    rb_iv_set(self, "@ptr", ptr);
    rb_iv_set(self, "@args", args);
    rb_iv_set(self, "@return_type", ret_type);
    rb_iv_set(self, "@abi", abi);

    if (!NIL_P(kwds)) rb_hash_foreach(kwds, parse_keyword_arg_i, self);

    TypedData_Get_Struct(self, ffi_cif, &function_data_type, cif);

    arg_types = xcalloc(RARRAY_LEN(args) + 1, sizeof(ffi_type *));

    for (i = 0; i < RARRAY_LEN(args); i++) {
	int type = NUM2INT(RARRAY_AREF(args, i));
	arg_types[i] = INT2FFI_TYPE(type);
    }
    arg_types[RARRAY_LEN(args)] = NULL;

    result = ffi_prep_cif (
	    cif,
	    NUM2INT(abi),
	    RARRAY_LENINT(args),
	    INT2FFI_TYPE(NUM2INT(ret_type)),
	    arg_types);

    if (result)
	rb_raise(rb_eRuntimeError, "error creating CIF %d", result);

    return self;
}