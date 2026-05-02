_TIFFrealloc(void* p, tmsize_t s)
{
	return (realloc(p, (size_t) s));
}
