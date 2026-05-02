NEDMALLOCNOALIASATTR NEDMALLOCPTRATTR void * nedpmalloc(nedpool *p, size_t size) THROWSPEC
{
	unsigned flags=NEDMALLOC_FORCERESERVE(p, 0, size);
	return nedpmalloc2(p, size, 0, flags);
}