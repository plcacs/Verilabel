static void build_dirs(char *src, char *dst, size_t src_prefix_len, size_t dst_prefix_len) {
	char *p = src + src_prefix_len + 1;
	char *q = dst + dst_prefix_len + 1;
	char *r = dst + dst_prefix_len;
	struct stat s;
	bool last = false;
	*r = '\0';
	for (; !last; p++, q++) {
		if (*p == '\0') {
			last = true;
		}
		if (*p == '\0' || (*p == '/' && *(p - 1) != '/')) {
			// We found a new component of our src path.
			// Null-terminate it temporarily here so that we can work
			// with it.
			*p = '\0';
			if (stat(src, &s) == 0 && S_ISDIR(s.st_mode)) {
				// Null-terminate the dst path and undo its previous
				// termination.
				*q = '\0';
				*r = '/';
				r = q;
				mkdir_attr(dst, s.st_mode, 0, 0);
			}
			if (!last) {
				// If we're not at the final terminating null, restore
				// the slash so that we can continue our traversal.
				*p = '/';
			}
		}
	}
}