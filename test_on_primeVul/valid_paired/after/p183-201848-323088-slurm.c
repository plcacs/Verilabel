extern int x11_delete_xauth(char *xauthority, char *host, uint16_t display)
{
	int i=0, status;
	char *result;
	char **xauth_argv;

	xauth_argv = xmalloc(sizeof(char *) * 10);
	xauth_argv[i++] = xstrdup("xauth");
	xauth_argv[i++] = xstrdup("-v");
	xauth_argv[i++] = xstrdup("-f");
	xauth_argv[i++] = xstrdup(xauthority);
	xauth_argv[i++] = xstrdup("remove");
	xauth_argv[i++] = xstrdup_printf("%s/unix:%u", host, display);
	xauth_argv[i++] = NULL;
	xassert(i < 10);

	result = run_command("xauth", XAUTH_PATH, xauth_argv, 10000, 0,
			     &status);

	free_command_argv(xauth_argv);

	debug2("%s: result from xauth: %s", __func__, result);
	xfree(result);

	return status;
}