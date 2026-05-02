void spl_filesystem_object_construct(INTERNAL_FUNCTION_PARAMETERS, zend_long ctor_flags) /* {{{ */
{
	spl_filesystem_object *intern;
	char *path;
	int parsed;
	size_t len;
	zend_long flags;
	zend_error_handling error_handling;

	zend_replace_error_handling(EH_THROW, spl_ce_UnexpectedValueException, &error_handling);

	if (SPL_HAS_FLAG(ctor_flags, DIT_CTOR_FLAGS)) {
		flags = SPL_FILE_DIR_KEY_AS_PATHNAME|SPL_FILE_DIR_CURRENT_AS_FILEINFO;
		parsed = zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &path, &len, &flags);
	} else {
		flags = SPL_FILE_DIR_KEY_AS_PATHNAME|SPL_FILE_DIR_CURRENT_AS_SELF;
		parsed = zend_parse_parameters(ZEND_NUM_ARGS(), "s", &path, &len);
	}
	if (SPL_HAS_FLAG(ctor_flags, SPL_FILE_DIR_SKIPDOTS)) {
		flags |= SPL_FILE_DIR_SKIPDOTS;
	}
	if (SPL_HAS_FLAG(ctor_flags, SPL_FILE_DIR_UNIXPATHS)) {
		flags |= SPL_FILE_DIR_UNIXPATHS;
	}
	if (parsed == FAILURE) {
		zend_restore_error_handling(&error_handling);
		return;
	}
	if (!len) {
		zend_throw_exception_ex(spl_ce_RuntimeException, 0, "Directory name must not be empty.");
		zend_restore_error_handling(&error_handling);
		return;
	}

	intern = Z_SPLFILESYSTEM_P(getThis());
	if (intern->_path) {
		/* object is alreay initialized */
		zend_restore_error_handling(&error_handling);
		php_error_docref(NULL, E_WARNING, "Directory object is already initialized");
		return;
	}
	intern->flags = flags;
#ifdef HAVE_GLOB
	if (SPL_HAS_FLAG(ctor_flags, DIT_CTOR_GLOB) && strstr(path, "glob://") != path) {
		spprintf(&path, 0, "glob://%s", path);
		spl_filesystem_dir_open(intern, path);
		efree(path);
	} else
#endif
	{
		spl_filesystem_dir_open(intern, path);

	}

	intern->u.dir.is_recursive = instanceof_function(intern->std.ce, spl_ce_RecursiveDirectoryIterator) ? 1 : 0;

	zend_restore_error_handling(&error_handling);
}