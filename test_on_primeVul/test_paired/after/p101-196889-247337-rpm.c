int pgpPrtParams(const uint8_t * pkts, size_t pktlen, unsigned int pkttype,
		 pgpDigParams * ret)
{
    const uint8_t *p = pkts;
    const uint8_t *pend = pkts + pktlen;
    pgpDigParams digp = NULL;
    pgpDigParams selfsig = NULL;
    int i = 0;
    int alloced = 16; /* plenty for normal cases */
    struct pgpPkt *all = xmalloc(alloced * sizeof(*all));
    int rc = -1; /* assume failure */
    int expect = 0;
    int prevtag = 0;

    while (p < pend) {
	struct pgpPkt *pkt = &all[i];
	if (decodePkt(p, (pend - p), pkt))
	    break;

	if (digp == NULL) {
	    if (pkttype && pkt->tag != pkttype) {
		break;
	    } else {
		digp = pgpDigParamsNew(pkt->tag);
	    }
	}

	if (expect) {
	    if (pkt->tag != expect)
		break;
	    selfsig = pgpDigParamsNew(pkt->tag);
	}

	if (pgpPrtPkt(pkt, selfsig ? selfsig : digp))
	    break;

	if (selfsig) {
	    /* subkeys must be followed by binding signature */
	    if (prevtag == PGPTAG_PUBLIC_SUBKEY) {
		if (selfsig->sigtype != PGPSIGTYPE_SUBKEY_BINDING)
		    break;
	    }

	    int xx = pgpVerifySelf(digp, selfsig, all, i);

	    selfsig = pgpDigParamsFree(selfsig);
	    if (xx)
		break;
	    expect = 0;
	}

	if (pkt->tag == PGPTAG_PUBLIC_SUBKEY)
	    expect = PGPTAG_SIGNATURE;
	prevtag = pkt->tag;

	i++;
	p += (pkt->body - pkt->head) + pkt->blen;
	if (pkttype == PGPTAG_SIGNATURE)
	    break;

	if (alloced <= i) {
	    alloced *= 2;
	    all = xrealloc(all, alloced * sizeof(*all));
	}
    }

    rc = (digp && (p == pend) && expect == 0) ? 0 : -1;

    free(all);
    if (ret && rc == 0) {
	*ret = digp;
    } else {
	pgpDigParamsFree(digp);
    }
    return rc;
}