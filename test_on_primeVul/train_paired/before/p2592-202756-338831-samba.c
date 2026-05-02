static int ldb_wildcard_compare(struct ldb_context *ldb,
				const struct ldb_parse_tree *tree,
				const struct ldb_val value, bool *matched)
{
	const struct ldb_schema_attribute *a;
	struct ldb_val val;
	struct ldb_val cnk;
	struct ldb_val *chunk;
	uint8_t *save_p = NULL;
	unsigned int c = 0;

	if (tree->operation != LDB_OP_SUBSTRING) {
		*matched = false;
		return LDB_ERR_INAPPROPRIATE_MATCHING;
	}

	a = ldb_schema_attribute_by_name(ldb, tree->u.substring.attr);
	if (!a) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	if (tree->u.substring.chunks == NULL) {
		*matched = false;
		return LDB_SUCCESS;
	}

	if (a->syntax->canonicalise_fn(ldb, ldb, &value, &val) != 0) {
		return LDB_ERR_INVALID_ATTRIBUTE_SYNTAX;
	}

	save_p = val.data;
	cnk.data = NULL;

	if ( ! tree->u.substring.start_with_wildcard ) {

		chunk = tree->u.substring.chunks[c];
		if (a->syntax->canonicalise_fn(ldb, ldb, chunk, &cnk) != 0) goto mismatch;

		/* This deals with wildcard prefix searches on binary attributes (eg objectGUID) */
		if (cnk.length > val.length) {
			goto mismatch;
		}
		/*
		 * Empty strings are returned as length 0. Ensure
		 * we can cope with this.
		 */
		if (cnk.length == 0) {
			goto mismatch;
		}

		if (memcmp((char *)val.data, (char *)cnk.data, cnk.length) != 0) goto mismatch;
		val.length -= cnk.length;
		val.data += cnk.length;
		c++;
		talloc_free(cnk.data);
		cnk.data = NULL;
	}

	while (tree->u.substring.chunks[c]) {
		uint8_t *p;

		chunk = tree->u.substring.chunks[c];
		if(a->syntax->canonicalise_fn(ldb, ldb, chunk, &cnk) != 0) goto mismatch;

		/*
		 * Empty strings are returned as length 0. Ensure
		 * we can cope with this.
		 */
		if (cnk.length == 0) {
			goto mismatch;
		}
		/*
		 * Values might be binary blobs. Don't use string
		 * search, but memory search instead.
		 */
		p = memmem((const void *)val.data,val.length,
			   (const void *)cnk.data, cnk.length);
		if (p == NULL) goto mismatch;

		/*
		 * At this point we know cnk.length <= val.length as
		 * otherwise there could be no match
		 */

		if ( (! tree->u.substring.chunks[c + 1]) && (! tree->u.substring.end_with_wildcard) ) {
			uint8_t *g;
			uint8_t *end = val.data + val.length;
			do { /* greedy */

				/*
				 * haystack is a valid pointer in val
				 * because the memmem() can only
				 * succeed if the needle (cnk.length)
				 * is <= haystacklen
				 *
				 * p will be a pointer at least
				 * cnk.length from the end of haystack
				 */
				uint8_t *haystack
					= p + cnk.length;
				size_t haystacklen
					= end - (haystack);

				g = memmem(haystack,
					   haystacklen,
					   (const uint8_t *)cnk.data,
					   cnk.length);
				if (g) {
					p = g;
				}
			} while(g);
		}
		val.length = val.length - (p - (uint8_t *)(val.data)) - cnk.length;
		val.data = (uint8_t *)(p + cnk.length);
		c++;
		talloc_free(cnk.data);
		cnk.data = NULL;
	}

	/* last chunk may not have reached end of string */
	if ( (! tree->u.substring.end_with_wildcard) && (*(val.data) != 0) ) goto mismatch;
	talloc_free(save_p);
	*matched = true;
	return LDB_SUCCESS;

mismatch:
	*matched = false;
	talloc_free(save_p);
	talloc_free(cnk.data);
	return LDB_SUCCESS;
}