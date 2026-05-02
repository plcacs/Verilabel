int pgpPrtParams(const uint8_t * pkts, size_t pktlen, unsigned int pkttype,
		 pgpDigParams * ret)
{
    const uint8_t *p = pkts;
    const uint8_t *pend = pkts + pktlen;
    pgpDigParams digp = NULL;
    struct pgpPkt pkt;
    int rc = -1; /* assume failure */

    while (p < pend) {
	if (decodePkt(p, (pend - p), &pkt))
	    break;

	if (digp == NULL) {
	    if (pkttype && pkt.tag != pkttype) {
		break;
	    } else {
		digp = pgpDigParamsNew(pkt.tag);
	    }
	}

	if (pgpPrtPkt(&pkt, digp))
	    break;

	p += (pkt.body - pkt.head) + pkt.blen;
	if (pkttype == PGPTAG_SIGNATURE)
	    break;
    }

    rc = (digp && (p == pend)) ? 0 : -1;

    if (ret && rc == 0) {
	*ret = digp;
    } else {
	pgpDigParamsFree(digp);
    }
    return rc;
}