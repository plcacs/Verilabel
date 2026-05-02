static int is_fuse_usermount(struct libmnt_context *cxt, int *errsv)
{
	struct libmnt_ns *ns_old;
	const char *type = mnt_fs_get_fstype(cxt->fs);
	const char *optstr;
	char *user_id = NULL;
	size_t sz;
	uid_t uid;
	char uidstr[sizeof(stringify_value(ULONG_MAX))];

	*errsv = 0;

	if (!type)
		return 0;

	if (strcmp(type, "fuse") != 0 &&
	    strcmp(type, "fuseblk") != 0 &&
	    strncmp(type, "fuse.", 5) != 0 &&
	    strncmp(type, "fuseblk.", 8) != 0)
		return 0;

	/* get user_id= from mount table */
	optstr = mnt_fs_get_fs_options(cxt->fs);
	if (!optstr)
		return 0;

	if (mnt_optstr_get_option(optstr, "user_id", &user_id, &sz) != 0)
		return 0;

	if (sz == 0 || user_id == NULL)
		return 0;

	/* get current user */
	ns_old = mnt_context_switch_origin_ns(cxt);
	if (!ns_old) {
		*errsv = -MNT_ERR_NAMESPACE;
		return 0;
	}

	uid = getuid();

	if (!mnt_context_switch_ns(cxt, ns_old)) {
		*errsv = -MNT_ERR_NAMESPACE;
		return 0;
	}

	snprintf(uidstr, sizeof(uidstr), "%lu", (unsigned long) uid);
	return strncmp(user_id, uidstr, sz) == 0;
}