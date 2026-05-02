check_tty(const char *tty)
{
	/* Check that we're not being set up to take a fall. */
	if ((tty == NULL) || (strlen(tty) == 0)) {
		return NULL;
	}
	/* Pull out the meaningful part of the tty's name. */
	if (strchr(tty, '/') != NULL) {
		if (strncmp(tty, "/dev/", 5) != 0) {
			/* Make sure the device node is actually in /dev/,
			 * noted by Michal Zalewski. */
			return NULL;
		}
		tty = strrchr(tty, '/') + 1;
	}
	/* Make sure the tty wasn't actually a directory (no basename). */
	if (strlen(tty) == 0) {
		return NULL;
	}
	return tty;
}