static Bigint * Balloc(int k)
{
	int x;
	Bigint *rv;

	_THREAD_PRIVATE_MUTEX_LOCK(dtoa_mutex);
	if ((rv = freelist[k])) {
		freelist[k] = rv->next;
	} else {
		x = 1 << k;
		rv = (Bigint *)MALLOC(sizeof(Bigint) + (x-1)*sizeof(Long));
		rv->k = k;
		rv->maxwds = x;
	}
	_THREAD_PRIVATE_MUTEX_UNLOCK(dtoa_mutex);
	rv->sign = rv->wds = 0;
	return rv;
}