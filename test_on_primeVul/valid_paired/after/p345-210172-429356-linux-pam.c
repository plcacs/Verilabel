get_ruser(pam_handle_t *pamh, char *ruserbuf, size_t ruserbuflen)
{
	const void *ruser;
	struct passwd *pwd;

	if (ruserbuf == NULL || ruserbuflen < 1)
		return -2;
	/* Get the name of the source user. */
	if (pam_get_item(pamh, PAM_RUSER, &ruser) != PAM_SUCCESS) {
		ruser = NULL;
	}
	if ((ruser == NULL) || (strlen(ruser) == 0)) {
		/* Barring that, use the current RUID. */
		pwd = pam_modutil_getpwuid(pamh, getuid());
		if (pwd != NULL) {
			ruser = pwd->pw_name;
		}
	} else {
		/*
		 * This ruser is used by format_timestamp_name as a component
		 * of constructed timestamp pathname, so ".", "..", and '/'
		 * are disallowed to avoid potential path traversal issues.
		 */
		if (!strcmp(ruser, ".") ||
		    !strcmp(ruser, "..") ||
		    strchr(ruser, '/')) {
			ruser = NULL;
		}
	}
	if (ruser == NULL || strlen(ruser) >= ruserbuflen) {
		*ruserbuf = '\0';
		return -1;
	}
	strcpy(ruserbuf, ruser);
	return 0;
}