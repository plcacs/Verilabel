sanitize_filename (const char *file_name)
{
	size_t      prefix_len;
	char const *p;

	if (file_name == NULL)
		return NULL;

	prefix_len = 0;
	for (p = file_name; *p; ) {
		if (ISDOT (p[0]) && ISDOT (p[1]) && (ISSLASH (p[2]) || !p[2]))
			prefix_len = p + 2 - file_name;

		do {
			char c = *p++;
			if (ISSLASH (c))
				break;
		}
		while (*p);
	}

	p = file_name + prefix_len;
	while (ISSLASH (*p))
		p++;

	return p;
}