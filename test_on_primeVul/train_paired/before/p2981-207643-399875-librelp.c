relpTcpChkPeerName(relpTcp_t *pThis, gnutls_x509_crt_t cert)
{
	int r = 0;
	int ret;
	unsigned int status = 0;
	char cnBuf[1024]; /* this is sufficient for the DNSNAME... */
	char szAltName[1024]; /* this is sufficient for the DNSNAME... */
	int iAltName;
	char allNames[32*1024]; /* for error-reporting */
	int iAllNames;
	size_t szAltNameLen;
	int bFoundPositiveMatch;
	int gnuRet;

	ret = gnutls_certificate_verify_peers2(pThis->session, &status);
	if(ret < 0) {
		callOnAuthErr(pThis, "", "certificate validation failed",
			RELP_RET_AUTH_CERT_INVL);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
	if(status != 0) { /* Certificate is not trusted */
		callOnAuthErr(pThis, "", "certificate validation failed",
			RELP_RET_AUTH_CERT_INVL);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}

	bFoundPositiveMatch = 0;
	iAllNames = 0;

	/* first search through the dNSName subject alt names */
	iAltName = 0;
	while(!bFoundPositiveMatch) { /* loop broken below */
		szAltNameLen = sizeof(szAltName);
		gnuRet = gnutls_x509_crt_get_subject_alt_name(cert, iAltName,
				szAltName, &szAltNameLen, NULL);
		if(gnuRet < 0)
			break;
		else if(gnuRet == GNUTLS_SAN_DNSNAME) {
			pThis->pEngine->dbgprint("librelp: subject alt dnsName: '%s'\n", szAltName);
			iAllNames += snprintf(allNames+iAllNames, sizeof(allNames)-iAllNames,
					      "DNSname: %s; ", szAltName);
			relpTcpChkOnePeerName(pThis, szAltName, &bFoundPositiveMatch);
			/* do NOT break, because there may be multiple dNSName's! */
		}
		++iAltName;
	}

	if(!bFoundPositiveMatch) {
		/* if we did not succeed so far, we try the CN part of the DN... */
		if(relpTcpGetCN(pThis, cert, cnBuf, sizeof(cnBuf)) == 0) {
			pThis->pEngine->dbgprint("librelp: relpTcp now checking auth for CN '%s'\n", cnBuf);
			iAllNames += snprintf(allNames+iAllNames, sizeof(allNames)-iAllNames,
					      "CN: %s; ", cnBuf);
			relpTcpChkOnePeerName(pThis, cnBuf, &bFoundPositiveMatch);
		}
	}

	if(!bFoundPositiveMatch) {
		callOnAuthErr(pThis, allNames, "no permited name found", RELP_RET_AUTH_ERR_NAME);
		r = GNUTLS_E_CERTIFICATE_ERROR; goto done;
	}
	r = 0;
done:
	return r;
}