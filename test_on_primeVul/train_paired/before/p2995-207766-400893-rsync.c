char *auth_server(int f_in, int f_out, int module, const char *host,
		  const char *addr, const char *leader)
{
	char *users = lp_auth_users(module);
	char challenge[MAX_DIGEST_LEN*2];
	char line[BIGPATHBUFLEN];
	char **auth_uid_groups = NULL;
	int auth_uid_groups_cnt = -1;
	const char *err = NULL;
	int group_match = -1;
	char *tok, *pass;
	char opt_ch = '\0';

	/* if no auth list then allow anyone in! */
	if (!users || !*users)
		return "";

	gen_challenge(addr, challenge);

	io_printf(f_out, "%s%s\n", leader, challenge);

	if (!read_line_old(f_in, line, sizeof line, 0)
	 || (pass = strchr(line, ' ')) == NULL) {
		rprintf(FLOG, "auth failed on module %s from %s (%s): "
			"invalid challenge response\n",
			lp_name(module), host, addr);
		return NULL;
	}
	*pass++ = '\0';

	if (!(users = strdup(users)))
		out_of_memory("auth_server");

	for (tok = strtok(users, " ,\t"); tok; tok = strtok(NULL, " ,\t")) {
		char *opts;
		/* See if the user appended :deny, :ro, or :rw. */
		if ((opts = strchr(tok, ':')) != NULL) {
			*opts++ = '\0';
			opt_ch = isUpper(opts) ? toLower(opts) : *opts;
			if (opt_ch == 'r') { /* handle ro and rw */
				opt_ch = isUpper(opts+1) ? toLower(opts+1) : opts[1];
				if (opt_ch == 'o')
					opt_ch = 'r';
				else if (opt_ch != 'w')
					opt_ch = '\0';
			} else if (opt_ch != 'd') /* if it's not deny, ignore it */
				opt_ch = '\0';
		} else
			opt_ch = '\0';
		if (*tok != '@') {
			/* Match the username */
			if (wildmatch(tok, line))
				break;
		} else {
#ifdef HAVE_GETGROUPLIST
			int j;
			/* See if authorizing user is a real user, and if so, see
			 * if it is in a group that matches tok+1 wildmat. */
			if (auth_uid_groups_cnt < 0) {
				item_list gid_list = EMPTY_ITEM_LIST;
				uid_t auth_uid;
				if (!user_to_uid(line, &auth_uid, False)
				 || getallgroups(auth_uid, &gid_list) != NULL)
					auth_uid_groups_cnt = 0;
				else {
					gid_t *gid_array = gid_list.items;
					auth_uid_groups_cnt = gid_list.count;
					if ((auth_uid_groups = new_array(char *, auth_uid_groups_cnt)) == NULL)
						out_of_memory("auth_server");
					for (j = 0; j < auth_uid_groups_cnt; j++)
						auth_uid_groups[j] = gid_to_group(gid_array[j]);
				}
			}
			for (j = 0; j < auth_uid_groups_cnt; j++) {
				if (auth_uid_groups[j] && wildmatch(tok+1, auth_uid_groups[j])) {
					group_match = j;
					break;
				}
			}
			if (group_match >= 0)
				break;
#else
			rprintf(FLOG, "your computer doesn't support getgrouplist(), so no @group authorization is possible.\n");
#endif
		}
	}

	free(users);

	if (!tok)
		err = "no matching rule";
	else if (opt_ch == 'd')
		err = "denied by rule";
	else {
		char *group = group_match >= 0 ? auth_uid_groups[group_match] : NULL;
		err = check_secret(module, line, group, challenge, pass);
	}

	memset(challenge, 0, sizeof challenge);
	memset(pass, 0, strlen(pass));

	if (auth_uid_groups) {
		int j;
		for (j = 0; j < auth_uid_groups_cnt; j++) {
			if (auth_uid_groups[j])
				free(auth_uid_groups[j]);
		}
		free(auth_uid_groups);
	}

	if (err) {
		rprintf(FLOG, "auth failed on module %s from %s (%s) for %s: %s\n",
			lp_name(module), host, addr, line, err);
		return NULL;
	}

	if (opt_ch == 'r')
		read_only = 1;
	else if (opt_ch == 'w')
		read_only = 0;

	return strdup(line);
}