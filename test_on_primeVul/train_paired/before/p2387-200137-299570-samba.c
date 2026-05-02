static int ldb_kv_index_dn_attr(struct ldb_module *module,
				struct ldb_kv_private *ldb_kv,
				const char *attr,
				struct ldb_dn *dn,
				struct dn_list *list,
				enum key_truncation *truncation)
{
	struct ldb_context *ldb;
	struct ldb_dn *key;
	struct ldb_val val;
	int ret;

	ldb = ldb_module_get_ctx(module);

	/* work out the index key from the parent DN */
	val.data = (uint8_t *)((uintptr_t)ldb_dn_get_casefold(dn));
	val.length = strlen((char *)val.data);
	key = ldb_kv_index_key(ldb, ldb_kv, attr, &val, NULL, truncation);
	if (!key) {
		ldb_oom(ldb);
		return LDB_ERR_OPERATIONS_ERROR;
	}

	ret = ldb_kv_dn_list_load(module, ldb_kv, key, list);
	talloc_free(key);
	if (ret != LDB_SUCCESS) {
		return ret;
	}

	if (list->count == 0) {
		return LDB_ERR_NO_SUCH_OBJECT;
	}

	return LDB_SUCCESS;
}