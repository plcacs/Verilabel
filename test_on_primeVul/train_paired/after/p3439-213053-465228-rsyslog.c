rsCStrExtendBuf(cstr_t *pThis, size_t iMinNeeded)
{
	uchar *pNewBuf;
	size_t iNewSize;
	DEFiRet;

	/* first compute the new size needed */
	if(iMinNeeded > RS_STRINGBUF_ALLOC_INCREMENT) {
		/* we allocate "n" ALLOC_INCREMENTs. Usually, that should
		 * leave some room after the absolutely needed one. It also
		 * reduces memory fragmentation. Note that all of this are
		 * integer operations (very important to understand what is
		 * going on)! Parenthesis are for better readibility.
		 */
		iNewSize = (iMinNeeded / RS_STRINGBUF_ALLOC_INCREMENT + 1) * RS_STRINGBUF_ALLOC_INCREMENT;
	} else {
		iNewSize = pThis->iBufSize + RS_STRINGBUF_ALLOC_INCREMENT;
	}
	iNewSize += pThis->iBufSize; /* add current size */

	/* DEV debugging only: dbgprintf("extending string buffer, old %d, new %d\n", pThis->iBufSize, iNewSize); */
	CHKmalloc(pNewBuf = (uchar*) realloc(pThis->pBuf, iNewSize * sizeof(uchar)));
	pThis->iBufSize = iNewSize;
	pThis->pBuf = pNewBuf;

finalize_it:
	RETiRet;
}