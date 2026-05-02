msg_t* MsgDup(msg_t* pOld)
{
	msg_t* pNew;
	rsRetVal localRet;

	assert(pOld != NULL);

	BEGINfunc
	if(msgConstructWithTime(&pNew, &pOld->tTIMESTAMP, pOld->ttGenTime) != RS_RET_OK) {
		return NULL;
	}

	/* now copy the message properties */
	pNew->iRefCount = 1;
	pNew->iSeverity = pOld->iSeverity;
	pNew->iFacility = pOld->iFacility;
	pNew->msgFlags = pOld->msgFlags;
	pNew->iProtocolVersion = pOld->iProtocolVersion;
	pNew->ttGenTime = pOld->ttGenTime;
	pNew->offMSG = pOld->offMSG;
	pNew->iLenRawMsg = pOld->iLenRawMsg;
	pNew->iLenMSG = pOld->iLenMSG;
	pNew->iLenTAG = pOld->iLenTAG;
	pNew->iLenHOSTNAME = pOld->iLenHOSTNAME;
	if((pOld->msgFlags & NEEDS_DNSRESOL)) {
			localRet = msgSetFromSockinfo(pNew, pOld->rcvFrom.pfrominet);
			if(localRet != RS_RET_OK) {
				/* if something fails, we accept loss of this property, it is
				 * better than losing the whole message.
				 */
				pNew->msgFlags &= ~NEEDS_DNSRESOL;
				pNew->rcvFrom.pRcvFrom = NULL; /* make sure no dangling values */
			}
	} else {
		if(pOld->rcvFrom.pRcvFrom != NULL) {
			pNew->rcvFrom.pRcvFrom = pOld->rcvFrom.pRcvFrom;
			prop.AddRef(pNew->rcvFrom.pRcvFrom);
		}
	}
	if(pOld->pRcvFromIP != NULL) {
		pNew->pRcvFromIP = pOld->pRcvFromIP;
		prop.AddRef(pNew->pRcvFromIP);
	}
	if(pOld->pInputName != NULL) {
		pNew->pInputName = pOld->pInputName;
		prop.AddRef(pNew->pInputName);
	}
	/* enable this, if someone actually uses UxTradMsg, delete after some time has
	 * passed and nobody complained -- rgerhards, 2009-06-16
	pNew->offAfterPRI = pOld->offAfterPRI;
	*/
	if(pOld->iLenTAG > 0) {
		if(pOld->iLenTAG < CONF_TAG_BUFSIZE) {
			memcpy(pNew->TAG.szBuf, pOld->TAG.szBuf, pOld->iLenTAG);
		} else {
			if((pNew->TAG.pszTAG = srUtilStrDup(pOld->TAG.pszTAG, pOld->iLenTAG)) == NULL) {
				msgDestruct(&pNew);
				return NULL;
			}
			pNew->iLenTAG = pOld->iLenTAG;
		}
	}
	if(pOld->iLenRawMsg < CONF_RAWMSG_BUFSIZE) {
		memcpy(pNew->szRawMsg, pOld->szRawMsg, pOld->iLenRawMsg + 1);
		pNew->pszRawMsg = pNew->szRawMsg;
	} else {
		tmpCOPYSZ(RawMsg);
	}
	if(pOld->iLenHOSTNAME < CONF_HOSTNAME_BUFSIZE) {
		memcpy(pNew->szHOSTNAME, pOld->szHOSTNAME, pOld->iLenHOSTNAME + 1);
		pNew->pszHOSTNAME = pNew->szHOSTNAME;
	} else {
		tmpCOPYSZ(HOSTNAME);
	}

	tmpCOPYCSTR(ProgName);
	tmpCOPYCSTR(StrucData);
	tmpCOPYCSTR(APPNAME);
	tmpCOPYCSTR(PROCID);
	tmpCOPYCSTR(MSGID);

	/* we do not copy all other cache properties, as we do not even know
	 * if they are needed once again. So we let them re-create if needed.
	 */

	ENDfunc
	return pNew;
}