static Bigint * Balloc(int k)
{
	int x;
	Bigint *rv;

	if (k > Kmax) {
		zend_error(E_ERROR, "Balloc() allocation exceeds list boundary");
	}

	_THREAD_PRIVATE_MUTEX_LOCK(dtoa_mutex);
	if ((rv = freelist[k])) {
		freelist[k] = rv->next;
	} else {
		x = 1 << k;
		rv = (Bigint *)MALLOC(sizeof(Bigint) + (x-1)*sizeof(Long));
		if (!rv) {
			_THREAD_PRIVATE_MUTEX_UNLOCK(dtoa_mutex);
			zend_error(E_ERROR, "Balloc() failed to allocate memory");
		}
		rv->k = k;
		rv->maxwds = x;
	}
	_THREAD_PRIVATE_MUTEX_UNLOCK(dtoa_mutex);
	rv->sign = rv->wds = 0;
	return rv;
}