static char *parsetree_to_sql(struct ldb_module *module,
			      void *mem_ctx,
			      const struct ldb_parse_tree *t)
{
	struct ldb_context *ldb;
	const struct ldb_schema_attribute *a;
	struct ldb_val value, subval;
	char *wild_card_string;
	char *child, *tmp;
	char *ret = NULL;
	char *attr;
	unsigned int i;

	ldb = ldb_module_get_ctx(module);

	switch(t->operation) {
	case LDB_OP_AND:

		tmp = parsetree_to_sql(module, mem_ctx, t->u.list.elements[0]);
		if (tmp == NULL) return NULL;

		for (i = 1; i < t->u.list.num_elements; i++) {

			child = parsetree_to_sql(module, mem_ctx, t->u.list.elements[i]);
			if (child == NULL) return NULL;

			tmp = talloc_asprintf_append(tmp, " INTERSECT %s ", child);
			if (tmp == NULL) return NULL;
		}

		ret = talloc_asprintf(mem_ctx, "SELECT * FROM ( %s )\n", tmp);

		return ret;

	case LDB_OP_OR:

		tmp = parsetree_to_sql(module, mem_ctx, t->u.list.elements[0]);
		if (tmp == NULL) return NULL;

		for (i = 1; i < t->u.list.num_elements; i++) {

			child = parsetree_to_sql(module, mem_ctx, t->u.list.elements[i]);
			if (child == NULL) return NULL;

			tmp = talloc_asprintf_append(tmp, " UNION %s ", child);
			if (tmp == NULL) return NULL;
		}

		return talloc_asprintf(mem_ctx, "SELECT * FROM ( %s ) ", tmp);

	case LDB_OP_NOT:

		child = parsetree_to_sql(module, mem_ctx, t->u.isnot.child);
		if (child == NULL) return NULL;

		return talloc_asprintf(mem_ctx,
					"SELECT eid FROM ldb_entry "
					"WHERE eid NOT IN ( %s ) ", child);

	case LDB_OP_EQUALITY:
		/*
		 * For simple searches, we want to retrieve the list of EIDs that
		 * match the criteria.
		*/
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		if (strcasecmp(t->u.equality.attr, "dn") == 0) {
			/* DN query is a special ldb case */
		 	const char *cdn = ldb_dn_get_casefold(
						ldb_dn_new(mem_ctx, ldb,
							      (const char *)value.data));

			return lsqlite3_tprintf(mem_ctx,
						"SELECT eid FROM ldb_entry "
						"WHERE norm_dn = '%q'", cdn);

		} else {
			/* A normal query. */
			return lsqlite3_tprintf(mem_ctx,
						"SELECT eid FROM ldb_attribute_values "
						"WHERE norm_attr_name = '%q' "
						"AND norm_attr_value = '%q'",
						attr,
						value.data);

		}

	case LDB_OP_SUBSTRING:

		wild_card_string = talloc_strdup(mem_ctx,
					(t->u.substring.start_with_wildcard)?"*":"");
		if (wild_card_string == NULL) return NULL;

		for (i = 0; t->u.substring.chunks[i]; i++) {
			wild_card_string = talloc_asprintf_append(wild_card_string, "%s*",
							t->u.substring.chunks[i]->data);
			if (wild_card_string == NULL) return NULL;
		}

		if ( ! t->u.substring.end_with_wildcard ) {
			/* remove last wildcard */
			wild_card_string[strlen(wild_card_string) - 1] = '\0';
		}

		attr = ldb_attr_casefold(mem_ctx, t->u.substring.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		subval.data = (void *)wild_card_string;
		subval.length = strlen(wild_card_string) + 1;

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(subval), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND norm_attr_value GLOB '%q'",
					attr,
					value.data);

	case LDB_OP_GREATER:
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND ldap_compare(norm_attr_value, '>=', '%q', '%q') ",
					attr,
					value.data,
					attr);

	case LDB_OP_LESS:
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND ldap_compare(norm_attr_value, '<=', '%q', '%q') ",
					attr,
					value.data,
					attr);

	case LDB_OP_PRESENT:
		if (strcasecmp(t->u.present.attr, "dn") == 0) {
			return talloc_strdup(mem_ctx, "SELECT eid FROM ldb_entry");
		}

		attr = ldb_attr_casefold(mem_ctx, t->u.present.attr);
		if (attr == NULL) return NULL;

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' ",
					attr);

	case LDB_OP_APPROX:
		attr = ldb_attr_casefold(mem_ctx, t->u.equality.attr);
		if (attr == NULL) return NULL;
		a = ldb_schema_attribute_by_name(ldb, attr);

		/* Get a canonicalised copy of the data */
		a->syntax->canonicalise_fn(ldb, mem_ctx, &(t->u.equality.value), &value);
		if (value.data == NULL) {
			return NULL;
		}

		return lsqlite3_tprintf(mem_ctx,
					"SELECT eid FROM ldb_attribute_values "
					"WHERE norm_attr_name = '%q' "
					"AND ldap_compare(norm_attr_value, '~%', 'q', '%q') ",
					attr,
					value.data,
					attr);

	case LDB_OP_EXTENDED:
#warning  "work out how to handle bitops"
		return NULL;

	default:
		break;
	};

	/* should never occur */
	abort();
	return NULL;
}