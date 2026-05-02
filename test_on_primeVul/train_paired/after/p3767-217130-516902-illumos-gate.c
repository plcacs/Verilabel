parse_user_name(char *user_input, char **ret_username)
{
	register char *ptr;
	register int index = 0;
	char username[PAM_MAX_RESP_SIZE];

	/* Set the default value for *ret_username */
	*ret_username = NULL;

	/*
	 * Set the initial value for username - this is a buffer holds
	 * the user name.
	 */
	bzero((void *)username, PAM_MAX_RESP_SIZE);

	/*
	 * The user_input is guaranteed to be terminated by a null character.
	 */
	ptr = user_input;

	/* Skip all the leading whitespaces if there are any. */
	while ((*ptr == ' ') || (*ptr == '\t'))
		ptr++;

	if (*ptr == '\0') {
		/*
		 * We should never get here since the user_input we got
		 * in pam_get_user() is not all whitespaces nor just "\0".
		 */
		return (PAM_BUF_ERR);
	}

	/*
	 * username will be the first string we get from user_input
	 * - we skip leading whitespaces and ignore trailing whitespaces
	 */
	while (*ptr != '\0') {
		if ((*ptr == ' ') || (*ptr == '\t') ||
		    (index >= PAM_MAX_RESP_SIZE)) {
			break;
		} else {
			username[index] = *ptr;
			index++;
			ptr++;
		}
	}

	/* ret_username will be freed in pam_get_user(). */
	if (index >= PAM_MAX_RESP_SIZE ||
	    (*ret_username = strdup(username)) == NULL)
		return (PAM_BUF_ERR);
	return (PAM_SUCCESS);
}