static INLINE BOOL ensure_capacity(const BYTE* start, const BYTE* end, size_t size, size_t base)
{
	const size_t available = (uintptr_t)end - (uintptr_t)start;
	const BOOL rc = available >= size * base;
	return rc;
}