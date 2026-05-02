NEDMALLOCNOALIASATTR NEDMALLOCPTRATTR void * nedpcalloc(nedpool *p, size_t no, size_t size) THROWSPEC
{
	unsigned flags=NEDMALLOC_FORCERESERVE(p, 0, no*size);
	return nedpmalloc2(p, size*no, 0, M2_ZERO_MEMORY|flags);
}