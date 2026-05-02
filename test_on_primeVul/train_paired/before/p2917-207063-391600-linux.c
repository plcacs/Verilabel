static char *ptr_to_id(char *buf, char *end, void *ptr, struct printf_spec spec)
{
	unsigned long hashval;

	if (unlikely(!have_filled_random_ptr_key)) {
		spec.field_width = 2 * sizeof(ptr);
		/* string length must be less than default_width */
		return string(buf, end, "(ptrval)", spec);
	}

#ifdef CONFIG_64BIT
	hashval = (unsigned long)siphash_1u64((u64)ptr, &ptr_key);
	/*
	 * Mask off the first 32 bits, this makes explicit that we have
	 * modified the address (and 32 bits is plenty for a unique ID).
	 */
	hashval = hashval & 0xffffffff;
#else
	hashval = (unsigned long)siphash_1u32((u32)ptr, &ptr_key);
#endif
	return pointer_string(buf, end, (const void *)hashval, spec);
}