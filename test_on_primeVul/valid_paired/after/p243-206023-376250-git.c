int url_is_local_not_ssh(const char *url)
{
	const char *colon = strchr(url, ':');
	const char *slash = strchr(url, '/');
	return !colon || (slash && slash < colon) ||
		(has_dos_drive_prefix(url) && is_valid_path(url));
}