writeDataError(instanceData *pData, cJSON **pReplyRoot, uchar *reqmsg)
{
	char *rendered = NULL;
	cJSON *errRoot;
	cJSON *req;
	cJSON *replyRoot = *pReplyRoot;
	size_t toWrite;
	ssize_t wrRet;
	char errStr[1024];
	DEFiRet;

	if(pData->errorFile == NULL) {
		DBGPRINTF("omelasticsearch: no local error logger defined - "
		          "ignoring ES error information\n");
		FINALIZE;
	}

	if(pData->fdErrFile == -1) {
		pData->fdErrFile = open((char*)pData->errorFile,
					O_WRONLY|O_CREAT|O_APPEND|O_LARGEFILE|O_CLOEXEC,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if(pData->fdErrFile == -1) {
			rs_strerror_r(errno, errStr, sizeof(errStr));
			DBGPRINTF("omelasticsearch: error opening error file: %s\n", errStr);
			ABORT_FINALIZE(RS_RET_ERR);
		}
	}
	if((req=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	cJSON_AddItemToObject(req, "url", cJSON_CreateString((char*)pData->restURL));
	cJSON_AddItemToObject(req, "postdata", cJSON_CreateString((char*)reqmsg));

	if((errRoot=cJSON_CreateObject()) == NULL) ABORT_FINALIZE(RS_RET_ERR);
	cJSON_AddItemToObject(errRoot, "request", req);
	cJSON_AddItemToObject(errRoot, "reply", replyRoot);
	rendered = cJSON_Print(errRoot);
	/* we do not do real error-handling on the err file, as this finally complicates
	 * things way to much.
	 */
	DBGPRINTF("omelasticsearch: error record: '%s'\n", rendered);
	toWrite = strlen(rendered);
	wrRet = write(pData->fdErrFile, rendered, toWrite);
	if(wrRet != (ssize_t) toWrite) {
		DBGPRINTF("omelasticsearch: error %d writing error file, write returns %lld\n",
			  errno, (long long) wrRet);
	}
	free(rendered);
	cJSON_Delete(errRoot);
	*pReplyRoot = NULL; /* tell caller not to delete once again! */

finalize_it:
	if(rendered != NULL)
		free(rendered);
	RETiRet;
}