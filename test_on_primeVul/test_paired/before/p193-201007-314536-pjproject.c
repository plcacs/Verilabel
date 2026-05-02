static int print_media_desc(const pjmedia_sdp_media *m, char *buf, pj_size_t len)
{
    char *p = buf;
    char *end = buf+len;
    unsigned i;
    int printed;

    /* check length for the "m=" line. */
    if (len < (pj_size_t)m->desc.media.slen+m->desc.transport.slen+12+24) {
	return -1;
    }
    *p++ = 'm';	    /* m= */
    *p++ = '=';
    pj_memcpy(p, m->desc.media.ptr, m->desc.media.slen);
    p += m->desc.media.slen;
    *p++ = ' ';
    printed = pj_utoa(m->desc.port, p);
    p += printed;
    if (m->desc.port_count > 1) {
	*p++ = '/';
	printed = pj_utoa(m->desc.port_count, p);
	p += printed;
    }
    *p++ = ' ';
    pj_memcpy(p, m->desc.transport.ptr, m->desc.transport.slen);
    p += m->desc.transport.slen;
    for (i=0; i<m->desc.fmt_count; ++i) {
	*p++ = ' ';
	pj_memcpy(p, m->desc.fmt[i].ptr, m->desc.fmt[i].slen);
	p += m->desc.fmt[i].slen;
    }
    *p++ = '\r';
    *p++ = '\n';

    /* print connection info, if present. */
    if (m->conn) {
	printed = print_connection_info(m->conn, p, (int)(end-p));
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }
    
    /* print optional bandwidth info. */
    for (i=0; i<m->bandw_count; ++i) {
	printed = (int)print_bandw(m->bandw[i], p, end-p);
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }

    /* print attributes. */
    for (i=0; i<m->attr_count; ++i) {
	printed = (int)print_attr(m->attr[i], p, end-p);
	if (printed < 0) {
	    return -1;
	}
	p += printed;
    }

    return (int)(p-buf);
}