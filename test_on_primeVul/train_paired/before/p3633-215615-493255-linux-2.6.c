static struct kmem_cache *ccid_kmem_cache_create(int obj_size, char *slab_name_fmt, const char *fmt,...)
{
	struct kmem_cache *slab;
	va_list args;

	va_start(args, fmt);
	vsnprintf(slab_name_fmt, sizeof(slab_name_fmt), fmt, args);
	va_end(args);

	slab = kmem_cache_create(slab_name_fmt, sizeof(struct ccid) + obj_size, 0,
				 SLAB_HWCACHE_ALIGN, NULL);
	return slab;
}