static int db_dict_iter_lookup_key_values(struct db_dict_value_iter *iter)
{
	struct db_dict_iter_key *key;
	string_t *path;
	const char *error;
	int ret;

	/* sort the keys so that we'll first lookup the keys without
	   default value. if their lookup fails, the user doesn't exist. */
	array_sort(&iter->keys, db_dict_iter_key_cmp);

	path = t_str_new(128);
	str_append(path, DICT_PATH_SHARED);

	array_foreach_modifiable(&iter->keys, key) {
		if (!key->used)
			continue;

		str_truncate(path, strlen(DICT_PATH_SHARED));
		ret = var_expand(path, key->key->key, iter->var_expand_table, &error);
		if (ret <= 0) {
			auth_request_log_error(iter->auth_request, AUTH_SUBSYS_DB,
				"Failed to expand key %s: %s", key->key->key, error);
			return -1;
		}
		ret = dict_lookup(iter->conn->dict, iter->pool,
				  str_c(path), &key->value, &error);
		if (ret > 0) {
			auth_request_log_debug(iter->auth_request, AUTH_SUBSYS_DB,
					       "Lookup: %s = %s", str_c(path),
					       key->value);
		} else if (ret < 0) {
			auth_request_log_error(iter->auth_request, AUTH_SUBSYS_DB,
				"Failed to lookup key %s: %s", str_c(path), error);
			return -1;
		} else if (key->key->default_value != NULL) {
			auth_request_log_debug(iter->auth_request, AUTH_SUBSYS_DB,
				"Lookup: %s not found, using default value %s",
				str_c(path), key->key->default_value);
			key->value = key->key->default_value;
		} else {
			return 0;
		}
	}
	return 1;
}