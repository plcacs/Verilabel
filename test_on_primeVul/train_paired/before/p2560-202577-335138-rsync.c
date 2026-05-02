void receive_xattr(int f, struct file_struct *file)
{
	static item_list temp_xattr = EMPTY_ITEM_LIST;
	int count, num;
#ifdef HAVE_LINUX_XATTRS
	int need_sort = 0;
#else
	int need_sort = 1;
#endif
	int ndx = read_varint(f);

	if (ndx < 0 || (size_t)ndx > rsync_xal_l.count) {
		rprintf(FERROR, "receive_xattr: xa index %d out of"
			" range for %s\n", ndx, f_name(file, NULL));
		exit_cleanup(RERR_STREAMIO);
	}

	if (ndx != 0) {
		F_XATTR(file) = ndx - 1;
		return;
	}

	if ((count = read_varint(f)) != 0) {
		(void)EXPAND_ITEM_LIST(&temp_xattr, rsync_xa, count);
		temp_xattr.count = 0;
	}

	for (num = 1; num <= count; num++) {
		char *ptr, *name;
		rsync_xa *rxa;
		size_t name_len = read_varint(f);
		size_t datum_len = read_varint(f);
		size_t dget_len = datum_len > MAX_FULL_DATUM ? 1 + MAX_DIGEST_LEN : datum_len;
		size_t extra_len = MIGHT_NEED_RPRE ? RPRE_LEN : 0;
		if ((dget_len + extra_len < dget_len)
		 || (dget_len + extra_len + name_len < dget_len + extra_len))
			overflow_exit("receive_xattr");
		ptr = new_array(char, dget_len + extra_len + name_len);
		if (!ptr)
			out_of_memory("receive_xattr");
		name = ptr + dget_len + extra_len;
		read_buf(f, name, name_len);
		if (dget_len == datum_len)
			read_buf(f, ptr, dget_len);
		else {
			*ptr = XSTATE_ABBREV;
			read_buf(f, ptr + 1, MAX_DIGEST_LEN);
		}

		if (saw_xattr_filter) {
			if (name_is_excluded(name, NAME_IS_XATTR, ALL_FILTERS)) {
				free(ptr);
				continue;
			}
		}
#ifdef HAVE_LINUX_XATTRS
		/* Non-root can only save the user namespace. */
		if (am_root <= 0 && !HAS_PREFIX(name, USER_PREFIX)) {
			if (!am_root && !saw_xattr_filter) {
				free(ptr);
				continue;
			}
			name -= RPRE_LEN;
			name_len += RPRE_LEN;
			memcpy(name, RSYNC_PREFIX, RPRE_LEN);
			need_sort = 1;
		}
#else
		/* This OS only has a user namespace, so we either
		 * strip the user prefix, or we put a non-user
		 * namespace inside our rsync hierarchy. */
		if (HAS_PREFIX(name, USER_PREFIX)) {
			name += UPRE_LEN;
			name_len -= UPRE_LEN;
		} else if (am_root) {
			name -= RPRE_LEN;
			name_len += RPRE_LEN;
			memcpy(name, RSYNC_PREFIX, RPRE_LEN);
		} else {
			free(ptr);
			continue;
		}
#endif
		/* No rsync.%FOO attributes are copied w/o 2 -X options. */
		if (preserve_xattrs < 2 && name_len > RPRE_LEN
		 && name[RPRE_LEN] == '%' && HAS_PREFIX(name, RSYNC_PREFIX)) {
			free(ptr);
			continue;
		}

		rxa = EXPAND_ITEM_LIST(&temp_xattr, rsync_xa, 1);
		rxa->name = name;
		rxa->datum = ptr;
		rxa->name_len = name_len;
		rxa->datum_len = datum_len;
		rxa->num = num;
	}

	if (need_sort && count > 1)
		qsort(temp_xattr.items, count, sizeof (rsync_xa), rsync_xal_compare_names);

	ndx = rsync_xal_store(&temp_xattr); /* adds item to rsync_xal_l */

	F_XATTR(file) = ndx;
}