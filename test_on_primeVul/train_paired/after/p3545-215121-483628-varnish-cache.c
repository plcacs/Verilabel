http1_dissect_hdrs(struct http *hp, char *p, struct http_conn *htc,
    unsigned maxhdr)
{
	char *q, *r, *s;
	int i;

	assert(p > htc->rxbuf_b);
	assert(p <= htc->rxbuf_e);
	hp->nhd = HTTP_HDR_FIRST;
	r = NULL;		/* For FlexeLint */
	for (; p < htc->rxbuf_e; p = r) {

		/* Find end of next header */
		q = r = p;
		if (vct_iscrlf(p, htc->rxbuf_e))
			break;
		while (r < htc->rxbuf_e) {
			if (!vct_isctl(*r) || vct_issp(*r)) {
				r++;
				continue;
			}
			i = vct_iscrlf(r, htc->rxbuf_e);
			if (i == 0) {
				VSLb(hp->vsl, SLT_BogoHeader,
				    "Header has ctrl char 0x%02x", *r);
				return (400);
			}
			q = r;
			r += i;
			assert(r <= htc->rxbuf_e);
			if (r == htc->rxbuf_e)
				break;
			if (vct_iscrlf(r, htc->rxbuf_e))
				break;
			/* If line does not continue: got it. */
			if (!vct_issp(*r))
				break;

			/* Clear line continuation LWS to spaces */
			while (q < r)
				*q++ = ' ';
			while (q < htc->rxbuf_e && vct_issp(*q))
				*q++ = ' ';
		}

		/* Empty header = end of headers */
		if (p == q)
			break;

		if (q - p > maxhdr) {
			VSLb(hp->vsl, SLT_BogoHeader, "Header too long: %.*s",
			    (int)(q - p > 20 ? 20 : q - p), p);
			return (400);
		}

		if (vct_islws(*p)) {
			VSLb(hp->vsl, SLT_BogoHeader,
			    "1st header has white space: %.*s",
			    (int)(q - p > 20 ? 20 : q - p), p);
			return (400);
		}

		if (*p == ':') {
			VSLb(hp->vsl, SLT_BogoHeader,
			    "Missing header name: %.*s",
			    (int)(q - p > 20 ? 20 : q - p), p);
			return (400);
		}

		while (q > p && vct_issp(q[-1]))
			q--;
		*q = '\0';

		for (s = p; *s != ':' && s < q; s++) {
			if (!vct_istchar(*s)) {
				VSLb(hp->vsl, SLT_BogoHeader,
				    "Illegal char 0x%02x in header name", *s);
				return (400);
			}
		}
		if (*s != ':') {
			VSLb(hp->vsl, SLT_BogoHeader, "Header without ':' %.*s",
			    (int)(q - p > 20 ? 20 : q - p), p);
			return (400);
		}

		if (hp->nhd < hp->shd) {
			hp->hdf[hp->nhd] = 0;
			hp->hd[hp->nhd].b = p;
			hp->hd[hp->nhd].e = q;
			hp->nhd++;
		} else {
			VSLb(hp->vsl, SLT_BogoHeader, "Too many headers: %.*s",
			    (int)(q - p > 20 ? 20 : q - p), p);
			return (400);
		}
	}
	i = vct_iscrlf(p, htc->rxbuf_e);
	assert(i > 0);		/* HTTP1_Complete guarantees this */
	p += i;
	HTC_RxPipeline(htc, p);
	htc->rxbuf_e = p;
	return (0);
}