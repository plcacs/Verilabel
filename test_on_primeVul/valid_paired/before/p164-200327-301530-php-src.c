void *_zend_shared_memdup(void *source, size_t size, zend_bool free_source TSRMLS_DC)
{
	void **old_p, *retval;

	if (zend_hash_index_find(&xlat_table, (ulong)source, (void **)&old_p) == SUCCESS) {
		/* we already duplicated this pointer */
		return *old_p;
	}
	retval = ZCG(mem);;
	ZCG(mem) = (void*)(((char*)ZCG(mem)) + ZEND_ALIGNED_SIZE(size));
	memcpy(retval, source, size);
	if (free_source) {
		interned_efree((char*)source);
	}
	zend_shared_alloc_register_xlat_entry(source, retval);
	return retval;
}