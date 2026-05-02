static int php_plain_files_rename(php_stream_wrapper *wrapper, const char *url_from, const char *url_to, int options, php_stream_context *context)
{
	int ret;

	if (!url_from || !url_to) {
		return 0;
	}

#ifdef PHP_WIN32
	if (!php_win32_check_trailing_space(url_from, (int)strlen(url_from))) {
		php_win32_docref2_from_error(ERROR_INVALID_NAME, url_from, url_to);
		return 0;
	}
	if (!php_win32_check_trailing_space(url_to, (int)strlen(url_to))) {
		php_win32_docref2_from_error(ERROR_INVALID_NAME, url_from, url_to);
		return 0;
	}
#endif

	if (strncasecmp(url_from, "file://", sizeof("file://") - 1) == 0) {
		url_from += sizeof("file://") - 1;
	}

	if (strncasecmp(url_to, "file://", sizeof("file://") - 1) == 0) {
		url_to += sizeof("file://") - 1;
	}

	if (php_check_open_basedir(url_from) || php_check_open_basedir(url_to)) {
		return 0;
	}

	ret = VCWD_RENAME(url_from, url_to);

	if (ret == -1) {
#ifndef PHP_WIN32
# ifdef EXDEV
		if (errno == EXDEV) {
			zend_stat_t sb;
# if !defined(ZTS) && !defined(TSRM_WIN32) && !defined(NETWARE)
			/* not sure what to do in ZTS case, umask is not thread-safe */
			int oldmask = umask(077);
# endif
			int success = 0;
			if (php_copy_file(url_from, url_to) == SUCCESS) {
				if (VCWD_STAT(url_from, &sb) == 0) {
					success = 1;
#  if !defined(TSRM_WIN32) && !defined(NETWARE)
					/*
					 * Try to set user and permission info on the target.
					 * If we're not root, then some of these may fail.
					 * We try chown first, to set proper group info, relying
					 * on the system environment to have proper umask to not allow
					 * access to the file in the meantime.
					 */
					if (VCWD_CHOWN(url_to, sb.st_uid, sb.st_gid)) {
						php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
						if (errno != EPERM) {
							success = 0;
						}
					}

					if (success) {
						if (VCWD_CHMOD(url_to, sb.st_mode)) {
							php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
							if (errno != EPERM) {
								success = 0;
							}
						}
					}
#  endif
					if (success) {
						VCWD_UNLINK(url_from);
					}
				} else {
					php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
				}
			} else {
				php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
			}
#  if !defined(ZTS) && !defined(TSRM_WIN32) && !defined(NETWARE)
			umask(oldmask);
#  endif
			return success;
		}
# endif
#endif

#ifdef PHP_WIN32
		php_win32_docref2_from_error(GetLastError(), url_from, url_to);
#else
		php_error_docref2(NULL, url_from, url_to, E_WARNING, "%s", strerror(errno));
#endif
		return 0;
	}

	/* Clear stat cache (and realpath cache) */
	php_clear_stat_cache(1, NULL, 0);

	return 1;
}