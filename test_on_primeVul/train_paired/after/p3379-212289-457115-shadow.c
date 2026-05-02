commonio_sort (struct commonio_db *db, int (*cmp) (const void *, const void *))
{
	struct commonio_entry **entries, *ptr;
	size_t n = 0, i;
#if KEEP_NIS_AT_END
	struct commonio_entry *nis = NULL;
#endif

	for (ptr = db->head;
	        (NULL != ptr)
#if KEEP_NIS_AT_END
	     && ((NULL == ptr->line)
	         || (('+' != ptr->line[0])
	             && ('-' != ptr->line[0])))
#endif
	     ;
	     ptr = ptr->next) {
		n++;
	}
#if KEEP_NIS_AT_END
	if (NULL != ptr) {
		nis = ptr;
	}
#endif

	if (n <= 1) {
		return 0;
	}

	entries = malloc (n * sizeof (struct commonio_entry *));
	if (entries == NULL) {
		return -1;
	}

	n = 0;
	for (ptr = db->head;
#if KEEP_NIS_AT_END
	     nis != ptr;
#else
	     NULL != ptr;
#endif
/*@ -nullderef @*/
	     ptr = ptr->next
/*@ +nullderef @*/
	    ) {
		entries[n] = ptr;
		n++;
	}
	qsort (entries, n, sizeof (struct commonio_entry *), cmp);

	/* Take care of the head and tail separately */
	db->head = entries[0];
	n--;
#if KEEP_NIS_AT_END
	if (NULL == nis)
#endif
	{
		db->tail = entries[n];
	}
	db->head->prev = NULL;
	db->head->next = entries[1];
	entries[n]->prev = entries[n - 1];
#if KEEP_NIS_AT_END
	entries[n]->next = nis;
#else
	entries[n]->next = NULL;
#endif

	/* Now other elements have prev and next entries */
	for (i = 1; i < n; i++) {
		entries[i]->prev = entries[i - 1];
		entries[i]->next = entries[i + 1];
	}

	free (entries);
	db->changed = true;

	return 0;
}