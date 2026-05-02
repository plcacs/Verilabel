MonoReflectionMethod*
mono_reflection_bind_generic_method_parameters (MonoReflectionMethod *rmethod, MonoArray *types)
{
	MonoClass *klass;
	MonoMethod *method, *inflated;
	MonoMethodInflated *imethod;
	MonoGenericContext tmp_context;
	MonoGenericInst *ginst;
	MonoType **type_argv;
	int count, i;

	MONO_ARCH_SAVE_REGS;

	/*FIXME but this no longer should happen*/
	if (!strcmp (rmethod->object.vtable->klass->name, "MethodBuilder")) {
#ifndef DISABLE_REFLECTION_EMIT
		MonoReflectionMethodBuilder *mb = NULL;
		MonoReflectionTypeBuilder *tb;
		MonoClass *klass;

		mb = (MonoReflectionMethodBuilder *) rmethod;
		tb = (MonoReflectionTypeBuilder *) mb->type;
		klass = mono_class_from_mono_type (mono_reflection_type_get_handle ((MonoReflectionType*)tb));

		method = methodbuilder_to_mono_method (klass, mb);
#else
		g_assert_not_reached ();
		method = NULL;
#endif
	} else {
		method = rmethod->method;
	}

	klass = method->klass;

	if (method->is_inflated)
		method = ((MonoMethodInflated *) method)->declaring;

	count = mono_method_signature (method)->generic_param_count;
	if (count != mono_array_length (types))
		return NULL;

	type_argv = g_new0 (MonoType *, count);
	for (i = 0; i < count; i++) {
		MonoReflectionType *garg = mono_array_get (types, gpointer, i);
		type_argv [i] = mono_reflection_type_get_handle (garg);
	}
	ginst = mono_metadata_get_generic_inst (count, type_argv);
	g_free (type_argv);

	tmp_context.class_inst = klass->generic_class ? klass->generic_class->context.class_inst : NULL;
	tmp_context.method_inst = ginst;

	inflated = mono_class_inflate_generic_method (method, &tmp_context);
	imethod = (MonoMethodInflated *) inflated;

	/*FIXME but I think this is no longer necessary*/
	if (method->klass->image->dynamic) {
		MonoDynamicImage *image = (MonoDynamicImage*)method->klass->image;
		/*
		 * This table maps metadata structures representing inflated methods/fields
		 * to the reflection objects representing their generic definitions.
		 */
		mono_loader_lock ();
		mono_g_hash_table_insert (image->generic_def_objects, imethod, rmethod);
		mono_loader_unlock ();
	}
	
	return mono_method_get_object (mono_object_domain (rmethod), inflated, NULL);