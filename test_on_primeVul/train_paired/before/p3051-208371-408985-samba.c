static struct ldb_parse_tree *ldb_parse_simple(TALLOC_CTX *mem_ctx, const char **s)
{
	char *attr, *value;
	struct ldb_parse_tree *ret;
	enum ldb_parse_op filtertype;

	ret = talloc(mem_ctx, struct ldb_parse_tree);
	if (!ret) {
		errno = ENOMEM;
		return NULL;
	}

	filtertype = ldb_parse_filtertype(ret, &attr, &value, s);
	if (!filtertype) {
		talloc_free(ret);
		return NULL;
	}

	switch (filtertype) {

		case LDB_OP_PRESENT:
			ret->operation = LDB_OP_PRESENT;
			ret->u.present.attr = attr;
			break;

		case LDB_OP_EQUALITY:

			if (strcmp(value, "*") == 0) {
				ret->operation = LDB_OP_PRESENT;
				ret->u.present.attr = attr;
				break;
			}

			if (ldb_parse_find_wildcard(value) != NULL) {
				ret->operation = LDB_OP_SUBSTRING;
				ret->u.substring.attr = attr;
				ret->u.substring.start_with_wildcard = 0;
				ret->u.substring.end_with_wildcard = 0;
				ret->u.substring.chunks = ldb_wildcard_decode(ret, value);
				if (ret->u.substring.chunks == NULL){
					talloc_free(ret);
					return NULL;
				}
				if (value[0] == '*')
					ret->u.substring.start_with_wildcard = 1;
				if (value[strlen(value) - 1] == '*')
					ret->u.substring.end_with_wildcard = 1;
				talloc_free(value);

				break;
			}

			ret->operation = LDB_OP_EQUALITY;
			ret->u.equality.attr = attr;
			ret->u.equality.value = ldb_binary_decode(ret, value);
			if (ret->u.equality.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_GREATER:
			ret->operation = LDB_OP_GREATER;
			ret->u.comparison.attr = attr;
			ret->u.comparison.value = ldb_binary_decode(ret, value);
			if (ret->u.comparison.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_LESS:
			ret->operation = LDB_OP_LESS;
			ret->u.comparison.attr = attr;
			ret->u.comparison.value = ldb_binary_decode(ret, value);
			if (ret->u.comparison.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_APPROX:
			ret->operation = LDB_OP_APPROX;
			ret->u.comparison.attr = attr;
			ret->u.comparison.value = ldb_binary_decode(ret, value);
			if (ret->u.comparison.value.data == NULL) {
				talloc_free(ret);
				return NULL;
			}
			talloc_free(value);
			break;

		case LDB_OP_EXTENDED:

			ret = ldb_parse_extended(ret, attr, value);
			break;

		default:
			talloc_free(ret);
			return NULL;
	}

	return ret;
}