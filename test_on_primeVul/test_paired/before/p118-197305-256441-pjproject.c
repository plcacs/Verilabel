PJ_DEF(pj_status_t) pjmedia_rtcp_fb_parse_rpsi(
					const void *buf,
					pj_size_t length,
					pjmedia_rtcp_fb_rpsi *rpsi)
{
    pjmedia_rtcp_common *hdr = (pjmedia_rtcp_common*) buf;
    pj_uint8_t *p;
    pj_uint8_t padlen;
    pj_size_t rpsi_len;

    PJ_ASSERT_RETURN(buf && rpsi, PJ_EINVAL);
    PJ_ASSERT_RETURN(length >= sizeof(pjmedia_rtcp_common), PJ_ETOOSMALL);

    /* RPSI uses pt==RTCP_PSFB and FMT==3 */
    if (hdr->pt != RTCP_PSFB || hdr->count != 3)
	return PJ_ENOTFOUND;

    rpsi_len = (pj_ntohs((pj_uint16_t)hdr->length)-2) * 4;
    if (length < rpsi_len + 12)
	return PJ_ETOOSMALL;

    p = (pj_uint8_t*)hdr + sizeof(*hdr);
    padlen = *p++;
    rpsi->pt = (*p++ & 0x7F);
    rpsi->rpsi_bit_len = rpsi_len*8 - 16 - padlen;
    pj_strset(&rpsi->rpsi, (char*)p, (rpsi->rpsi_bit_len + 7)/8);

    return PJ_SUCCESS;
}