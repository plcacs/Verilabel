PJ_DEF(pj_status_t) pjmedia_rtcp_fb_parse_rpsi(
					const void *buf,
					pj_size_t length,
					pjmedia_rtcp_fb_rpsi *rpsi)
{
    pjmedia_rtcp_fb_common *hdr = (pjmedia_rtcp_fb_common*) buf;
    pj_uint8_t *p;
    pj_uint8_t padlen;
    pj_size_t rpsi_len;

    PJ_ASSERT_RETURN(buf && rpsi, PJ_EINVAL);
    PJ_ASSERT_RETURN(length >= sizeof(pjmedia_rtcp_fb_common), PJ_ETOOSMALL);

    /* RPSI uses pt==RTCP_PSFB and FMT==3 */
    if (hdr->rtcp_common.pt != RTCP_PSFB || hdr->rtcp_common.count != 3)
	return PJ_ENOTFOUND;

    if (hdr->rtcp_common.length < 3) {    
        PJ_PERROR(3, (THIS_FILE, PJ_ETOOSMALL,
                      "Failed parsing FB RPSI, invalid header length"));
	return PJ_ETOOSMALL;
    }

    rpsi_len = (pj_ntohs((pj_uint16_t)hdr->rtcp_common.length)-2) * 4;
    if (length < rpsi_len + 12)
	return PJ_ETOOSMALL;

    p = (pj_uint8_t*)hdr + sizeof(*hdr);
    padlen = *p++;

    if (padlen >= 32) {
        PJ_PERROR(3, (THIS_FILE, PJ_ETOOBIG,
                      "Failed parsing FB RPSI, invalid RPSI padding len"));
	return PJ_ETOOBIG;
    }

    if ((rpsi_len * 8) < (unsigned)(16 + padlen)) {
        PJ_PERROR(3, (THIS_FILE, PJ_ETOOSMALL,
                      "Failed parsing FB RPSI, invalid RPSI bit len"));
	return PJ_ETOOSMALL;
    }

    rpsi->pt = (*p++ & 0x7F);
    rpsi->rpsi_bit_len = rpsi_len*8 - 16 - padlen;
    pj_strset(&rpsi->rpsi, (char*)p, (rpsi->rpsi_bit_len + 7)/8);

    return PJ_SUCCESS;
}