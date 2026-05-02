GIT_INLINE(bool) only_spaces_and_dots(const char *path)
{
	const char *c = path;

	for (;; c++) {
		if (*c == '\0' || *c == ':')
			return true;
		if (*c != ' ' && *c != '.')
			return false;
	}

	return true;
}