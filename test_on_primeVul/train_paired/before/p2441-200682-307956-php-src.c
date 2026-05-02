static int phar_extract_file(zend_bool overwrite, phar_entry_info *entry, char *dest, int dest_len, char **error TSRMLS_DC) /* {{{ */
{
	php_stream_statbuf ssb;
	int len;
	php_stream *fp;
	char *fullpath;
	const char *slash;
	mode_t mode;
	cwd_state new_state;
	char *filename;
	size_t filename_len;

	if (entry->is_mounted) {
		/* silently ignore mounted entries */
		return SUCCESS;
	}

	if (entry->filename_len >= sizeof(".phar")-1 && !memcmp(entry->filename, ".phar", sizeof(".phar")-1)) {
		return SUCCESS;
	}
	/* strip .. from path and restrict it to be under dest directory */
	new_state.cwd = (char*)emalloc(2);
	new_state.cwd[0] = DEFAULT_SLASH;
	new_state.cwd[1] = '\0';
	new_state.cwd_length = 1;
	if (virtual_file_ex(&new_state, entry->filename, NULL, CWD_EXPAND TSRMLS_CC) != 0 ||
			new_state.cwd_length <= 1) {
		if (EINVAL == errno && entry->filename_len > 50) {
			char *tmp = estrndup(entry->filename, 50);
			spprintf(error, 4096, "Cannot extract \"%s...\" to \"%s...\", extracted filename is too long for filesystem", tmp, dest);
			efree(tmp);
		} else {
			spprintf(error, 4096, "Cannot extract \"%s\", internal error", entry->filename);
		}
		efree(new_state.cwd);
		return FAILURE;
	}
	filename = new_state.cwd + 1;
	filename_len = new_state.cwd_length - 1;
#ifdef PHP_WIN32
	/* unixify the path back, otherwise non zip formats might be broken */
	{
		int cnt = filename_len;

		do {
			if ('\\' == filename[cnt]) {
				filename[cnt] = '/';
			}
		} while (cnt-- >= 0);
	}
#endif

	len = spprintf(&fullpath, 0, "%s/%s", dest, filename);

	if (len >= MAXPATHLEN) {
		char *tmp;
		/* truncate for error message */
		fullpath[50] = '\0';
		if (entry->filename_len > 50) {
			tmp = estrndup(entry->filename, 50);
			spprintf(error, 4096, "Cannot extract \"%s...\" to \"%s...\", extracted filename is too long for filesystem", tmp, fullpath);
			efree(tmp);
		} else {
			spprintf(error, 4096, "Cannot extract \"%s\" to \"%s...\", extracted filename is too long for filesystem", entry->filename, fullpath);
		}
		efree(fullpath);
		efree(new_state.cwd);
		return FAILURE;
	}

	if (!len) {
		spprintf(error, 4096, "Cannot extract \"%s\", internal error", entry->filename);
		efree(fullpath);
		efree(new_state.cwd);
		return FAILURE;
	}

	if (PHAR_OPENBASEDIR_CHECKPATH(fullpath)) {
		spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", openbasedir/safe mode restrictions in effect", entry->filename, fullpath);
		efree(fullpath);
		efree(new_state.cwd);
		return FAILURE;
	}

	/* let see if the path already exists */
	if (!overwrite && SUCCESS == php_stream_stat_path(fullpath, &ssb)) {
		spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", path already exists", entry->filename, fullpath);
		efree(fullpath);
		efree(new_state.cwd);
		return FAILURE;
	}

	/* perform dirname */
	slash = zend_memrchr(filename, '/', filename_len);

	if (slash) {
		fullpath[dest_len + (slash - filename) + 1] = '\0';
	} else {
		fullpath[dest_len] = '\0';
	}

	if (FAILURE == php_stream_stat_path(fullpath, &ssb)) {
		if (entry->is_dir) {
			if (!php_stream_mkdir(fullpath, entry->flags & PHAR_ENT_PERM_MASK,  PHP_STREAM_MKDIR_RECURSIVE, NULL)) {
				spprintf(error, 4096, "Cannot extract \"%s\", could not create directory \"%s\"", entry->filename, fullpath);
				efree(fullpath);
				free(new_state.cwd);
				return FAILURE;
			}
		} else {
			if (!php_stream_mkdir(fullpath, 0777,  PHP_STREAM_MKDIR_RECURSIVE, NULL)) {
				spprintf(error, 4096, "Cannot extract \"%s\", could not create directory \"%s\"", entry->filename, fullpath);
				efree(fullpath);
				free(new_state.cwd);
				return FAILURE;
			}
		}
	}

	if (slash) {
		fullpath[dest_len + (slash - filename) + 1] = '/';
	} else {
		fullpath[dest_len] = '/';
	}

	filename = NULL;
	efree(new_state.cwd);
	/* it is a standalone directory, job done */
	if (entry->is_dir) {
		efree(fullpath);
		return SUCCESS;
	}

#if PHP_API_VERSION < 20100412
	fp = php_stream_open_wrapper(fullpath, "w+b", REPORT_ERRORS|ENFORCE_SAFE_MODE, NULL);
#else
	fp = php_stream_open_wrapper(fullpath, "w+b", REPORT_ERRORS, NULL);
#endif

	if (!fp) {
		spprintf(error, 4096, "Cannot extract \"%s\", could not open for writing \"%s\"", entry->filename, fullpath);
		efree(fullpath);
		return FAILURE;
	}

	if (!phar_get_efp(entry, 0 TSRMLS_CC)) {
		if (FAILURE == phar_open_entry_fp(entry, error, 1 TSRMLS_CC)) {
			if (error) {
				spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", unable to open internal file pointer: %s", entry->filename, fullpath, *error);
			} else {
				spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", unable to open internal file pointer", entry->filename, fullpath);
			}
			efree(fullpath);
			php_stream_close(fp);
			return FAILURE;
		}
	}

	if (FAILURE == phar_seek_efp(entry, 0, SEEK_SET, 0, 0 TSRMLS_CC)) {
		spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", unable to seek internal file pointer", entry->filename, fullpath);
		efree(fullpath);
		php_stream_close(fp);
		return FAILURE;
	}

	if (SUCCESS != php_stream_copy_to_stream_ex(phar_get_efp(entry, 0 TSRMLS_CC), fp, entry->uncompressed_filesize, NULL)) {
		spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", copying contents failed", entry->filename, fullpath);
		efree(fullpath);
		php_stream_close(fp);
		return FAILURE;
	}

	php_stream_close(fp);
	mode = (mode_t) entry->flags & PHAR_ENT_PERM_MASK;

	if (FAILURE == VCWD_CHMOD(fullpath, mode)) {
		spprintf(error, 4096, "Cannot extract \"%s\" to \"%s\", setting file permissions failed", entry->filename, fullpath);
		efree(fullpath);
		return FAILURE;
	}

	efree(fullpath);
	return SUCCESS;
}