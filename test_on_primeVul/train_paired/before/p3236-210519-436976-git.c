int is_ntfs_dotgit(const char *name)
{
	int len;

	for (len = 0; ; len++)
		if (!name[len] || name[len] == '\\' || is_dir_sep(name[len])) {
			if (only_spaces_and_periods(name, len, 4) &&
					!strncasecmp(name, ".git", 4))
				return 1;
			if (only_spaces_and_periods(name, len, 5) &&
					!strncasecmp(name, "git~1", 5))
				return 1;
			if (name[len] != '\\')
				return 0;
			name += len + 1;
			len = -1;
		}
}