static int unix_getpw(UNUSED void *instance, REQUEST *request,
		      VALUE_PAIR **vp_list)
{
	const char	*name;
	const char	*encrypted_pass;
#ifdef HAVE_GETSPNAM
	struct spwd	*spwd = NULL;
#endif
#ifdef OSFC2
	struct pr_passwd *pr_pw;
#else
	struct passwd	*pwd;
#endif
#ifdef HAVE_GETUSERSHELL
	char		*shell;
#endif
	VALUE_PAIR	*vp;

	/*
	 *	We can only authenticate user requests which HAVE
	 *	a User-Name attribute.
	 */
	if (!request->username) {
		return RLM_MODULE_NOOP;
	}

	name = (char *)request->username->vp_strvalue;
	encrypted_pass = NULL;

#ifdef OSFC2
	if ((pr_pw = getprpwnam(name)) == NULL)
		return RLM_MODULE_NOTFOUND;
	encrypted_pass = pr_pw->ufld.fd_encrypt;

	/*
	 *	Check if account is locked.
	 */
	if (pr_pw->uflg.fg_lock!=1) {
		radlog(L_AUTH, "rlm_unix: [%s]: account locked", name);
		return RLM_MODULE_USERLOCK;
	}
#else /* OSFC2 */
	if ((pwd = getpwnam(name)) == NULL) {
		return RLM_MODULE_NOTFOUND;
	}
	encrypted_pass = pwd->pw_passwd;
#endif /* OSFC2 */

#ifdef HAVE_GETSPNAM
	/*
	 *      See if there is a shadow password.
	 *
	 *	Only query the _system_ shadow file if the encrypted
	 *	password from the passwd file is < 10 characters (i.e.
	 *	a valid password would never crypt() to it).  This will
	 *	prevents users from using NULL password fields as things
	 *	stand right now.
	 */
	if ((encrypted_pass == NULL) || (strlen(encrypted_pass) < 10)) {
		if ((spwd = getspnam(name)) == NULL) {
			return RLM_MODULE_NOTFOUND;
		}
		encrypted_pass = spwd->sp_pwdp;
	}
#endif	/* HAVE_GETSPNAM */

/*
 *	These require 'pwd != NULL', which isn't true on OSFC2
 */
#ifndef OSFC2
#ifdef DENY_SHELL
	/*
	 *	Users with a particular shell are denied access
	 */
	if (strcmp(pwd->pw_shell, DENY_SHELL) == 0) {
		radlog_request(L_AUTH, 0, request,
			       "rlm_unix: [%s]: invalid shell", name);
		return RLM_MODULE_REJECT;
	}
#endif

#ifdef HAVE_GETUSERSHELL
	/*
	 *	Check /etc/shells for a valid shell. If that file
	 *	contains /RADIUSD/ANY/SHELL then any shell will do.
	 */
	while ((shell = getusershell()) != NULL) {
		if (strcmp(shell, pwd->pw_shell) == 0 ||
		    strcmp(shell, "/RADIUSD/ANY/SHELL") == 0) {
			break;
		}
	}
	endusershell();
	if (shell == NULL) {
		radlog_request(L_AUTH, 0, request, "[%s]: invalid shell [%s]",
		       name, pwd->pw_shell);
		return RLM_MODULE_REJECT;
	}
#endif
#endif /* OSFC2 */

#if defined(HAVE_GETSPNAM) && !defined(M_UNIX)
	/*
	 *      Check if password has expired.
	 */
	if (spwd && spwd->sp_expire > 0 &&
	    (request->timestamp / 86400) > spwd->sp_expire) {
		radlog_request(L_AUTH, 0, request, "[%s]: password has expired", name);
		return RLM_MODULE_REJECT;
	}
#endif

#if defined(__FreeBSD__) || defined(bsdi) || defined(_PWF_EXPIRE)
	/*
	 *	Check if password has expired.
	 */
	if ((pwd->pw_expire > 0) &&
	    (request->timestamp > pwd->pw_expire)) {
		radlog_request(L_AUTH, 0, request, "[%s]: password has expired", name);
		return RLM_MODULE_REJECT;
	}
#endif

	/*
	 *	We might have a passwordless account.
	 *
	 *	FIXME: Maybe add Auth-Type := Accept?
	 */
	if (encrypted_pass[0] == 0)
		return RLM_MODULE_NOOP;

	vp = pairmake("Crypt-Password", encrypted_pass, T_OP_SET);
	if (!vp) return RLM_MODULE_FAIL;

	pairmove(vp_list, &vp);
	pairfree(&vp);		/* might not be NULL; */

	return RLM_MODULE_UPDATED;
}