processDataRcvd(ptcpsess_t *const __restrict__ pThis,
	char **buff,
	const int buffLen,
	struct syslogTime *stTime,
	const time_t ttGenTime,
	multi_submit_t *pMultiSub,
	unsigned *const __restrict__ pnMsgs)
{
	DEFiRet;
	char c = **buff;
	int octatesToCopy, octatesToDiscard;

	if(pThis->inputState == eAtStrtFram) {
		if(pThis->bSuppOctetFram && isdigit((int) c)) {
			pThis->inputState = eInOctetCnt;
			pThis->iOctetsRemain = 0;
			pThis->eFraming = TCP_FRAMING_OCTET_COUNTING;
		} else if(pThis->bSPFramingFix && c == ' ') {
			/* Cisco very occasionally sends a SP after a LF, which
			 * thrashes framing if not taken special care of. Here,
			 * we permit space *in front of the next frame* and
			 * ignore it.
			 */
			 FINALIZE;
		} else {
			pThis->inputState = eInMsg;
			pThis->eFraming = TCP_FRAMING_OCTET_STUFFING;
		}
	}

	if(pThis->inputState == eInOctetCnt) {
		if(isdigit(c)) {
			pThis->iOctetsRemain = pThis->iOctetsRemain * 10 + c - '0';
		} else { /* done with the octet count, so this must be the SP terminator */
			DBGPRINTF("TCP Message with octet-counter, size %d.\n", pThis->iOctetsRemain);
			if(c != ' ') {
				errmsg.LogError(0, NO_ERRCODE, "Framing Error in received TCP message: "
					    "delimiter is not SP but has ASCII value %d.", c);
			}
			if(pThis->iOctetsRemain < 1) {
				/* TODO: handle the case where the octet count is 0! */
				DBGPRINTF("Framing Error: invalid octet count\n");
				errmsg.LogError(0, NO_ERRCODE, "Framing Error in received TCP message: "
					    "invalid octet count %d.", pThis->iOctetsRemain);
			} else if(pThis->iOctetsRemain > iMaxLine) {
				/* while we can not do anything against it, we can at least log an indication
				 * that something went wrong) -- rgerhards, 2008-03-14
				 */
				DBGPRINTF("truncating message with %d octets - max msg size is %d\n",
					  pThis->iOctetsRemain, iMaxLine);
				errmsg.LogError(0, NO_ERRCODE, "received oversize message: size is %d bytes, "
					        "max msg size is %d, truncating...", pThis->iOctetsRemain, iMaxLine);
			}
			pThis->inputState = eInMsg;
		}
	} else {
		assert(pThis->inputState == eInMsg);

		if (pThis->eFraming == TCP_FRAMING_OCTET_STUFFING) {
			if(pThis->iMsg >= iMaxLine) {
				/* emergency, we now need to flush, no matter if we are at end of message or not... */
				int i = 1;
				char currBuffChar;
				while(i < buffLen && ((currBuffChar = (*buff)[i]) != '\n'
					&& (pThis->pLstn->pSrv->iAddtlFrameDelim == TCPSRV_NO_ADDTL_DELIMITER
						|| currBuffChar != pThis->pLstn->pSrv->iAddtlFrameDelim))) {
					i++;
				}
				LogError(0, NO_ERRCODE, "error: message received is at least %d byte larger than max msg"
					" size; message will be split starting at: \"%.*s\"\n", i, (i < 32) ? i : 32, *buff);
				doSubmitMsg(pThis, stTime, ttGenTime, pMultiSub);
				++(*pnMsgs);
				/* we might think if it is better to ignore the rest of the
				 * message than to treat it as a new one. Maybe this is a good
				 * candidate for a configuration parameter...
				 * rgerhards, 2006-12-04
				 */
			}

			if ((c == '\n')
				   || ((pThis->pLstn->pSrv->iAddtlFrameDelim != TCPSRV_NO_ADDTL_DELIMITER)
					   && (c == pThis->pLstn->pSrv->iAddtlFrameDelim))
				   ) { /* record delimiter? */
				doSubmitMsg(pThis, stTime, ttGenTime, pMultiSub);
				++(*pnMsgs);
				pThis->inputState = eAtStrtFram;
			} else {
				/* IMPORTANT: here we copy the actual frame content to the message - for BOTH framing modes!
				 * If we have a message that is larger than the max msg size, we truncate it. This is the best
				 * we can do in light of what the engine supports. -- rgerhards, 2008-03-14
				 */
				if(pThis->iMsg < iMaxLine) {
					*(pThis->pMsg + pThis->iMsg++) = c;
				}
			}
		} else {
			assert(pThis->eFraming == TCP_FRAMING_OCTET_COUNTING);
			octatesToCopy = pThis->iOctetsRemain;
			octatesToDiscard = 0;
			if (buffLen < octatesToCopy) {
				octatesToCopy = buffLen;
			}
			if (octatesToCopy + pThis->iMsg > iMaxLine) {
				octatesToDiscard = octatesToCopy - (iMaxLine - pThis->iMsg);
				octatesToCopy = iMaxLine - pThis->iMsg;
			}

			memcpy(pThis->pMsg + pThis->iMsg, *buff, octatesToCopy);
			pThis->iMsg += octatesToCopy;
			pThis->iOctetsRemain -= (octatesToCopy + octatesToDiscard);
			*buff += (octatesToCopy + octatesToDiscard - 1);
			if (pThis->iOctetsRemain == 0) {
				/* we have end of frame! */
				doSubmitMsg(pThis, stTime, ttGenTime, pMultiSub);
				++(*pnMsgs);
				pThis->inputState = eAtStrtFram;
			}
		}

	}

finalize_it:
	RETiRet;
}