static char *make_filename_safe(const char *filename TSRMLS_DC)
{
	if (*filename && memcmp(filename, ":memory:", sizeof(":memory:"))) {
		char *fullpath = expand_filepath(filename, NULL TSRMLS_CC);

		if (!fullpath) {
			return NULL;
		}

		if (PG(safe_mode) && (!php_checkuid(fullpath, NULL, CHECKUID_CHECK_FILE_AND_DIR))) {
			efree(fullpath);
			return NULL;
		}

		if (php_check_open_basedir(fullpath TSRMLS_CC)) {
			efree(fullpath);
			return NULL;
		}
		return fullpath;
	}
	return estrdup(filename);
}