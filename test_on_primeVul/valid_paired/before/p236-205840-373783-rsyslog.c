static rsRetVal qAddDirect(qqueue_t *pThis, void* pUsr)
{
	batch_t singleBatch;
	batch_obj_t batchObj;
	DEFiRet;

	//TODO: init batchObj (states _OK and new fields -- CHECK)
	ASSERT(pThis != NULL);

	/* calling the consumer is quite different here than it is from a worker thread */
	/* we need to provide the consumer's return value back to the caller because in direct
	 * mode the consumer probably has a lot to convey (which get's lost in the other modes
	 * because they are asynchronous. But direct mode is deliberately synchronous.
	 * rgerhards, 2008-02-12
	 * We use our knowledge about the batch_t structure below, but without that, we
	 * pay a too-large performance toll... -- rgerhards, 2009-04-22
	 */
	memset(&batchObj, 0, sizeof(batch_obj_t));
	memset(&singleBatch, 0, sizeof(batch_t));
	batchObj.state = BATCH_STATE_RDY;
	batchObj.pUsrp = (obj_t*) pUsr;
	batchObj.bFilterOK = 1;
	singleBatch.nElem = 1; /* there always is only one in direct mode */
	singleBatch.pElem = &batchObj;
	iRet = pThis->pConsumer(pThis->pUsr, &singleBatch, &pThis->bShutdownImmediate);
	objDestruct(pUsr);

	RETiRet;
}