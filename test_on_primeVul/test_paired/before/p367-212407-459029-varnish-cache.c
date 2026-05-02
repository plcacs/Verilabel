http_isfiltered(const struct http *fm, unsigned u, unsigned how)
{
	const char *e;
	const struct http_hdrflg *f;

	if (fm->hdf[u] & HDF_FILTER)
		return (1);
	e = strchr(fm->hd[u].b, ':');
	if (e == NULL)
		return (0);
	f = http_hdr_flags(fm->hd[u].b, e);
	return (f != NULL && f->flag & how);
}