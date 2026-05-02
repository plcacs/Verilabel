struct nfc_llcp_sdp_tlv *nfc_llcp_build_sdreq_tlv(u8 tid, char *uri,
						  size_t uri_len)
{
	struct nfc_llcp_sdp_tlv *sdreq;

	pr_debug("uri: %s, len: %zu\n", uri, uri_len);

	/* sdreq->tlv_len is u8, takes uri_len, + 3 for header, + 1 for NULL */
	if (WARN_ON_ONCE(uri_len > U8_MAX - 4))
		return NULL;

	sdreq = kzalloc(sizeof(struct nfc_llcp_sdp_tlv), GFP_KERNEL);
	if (sdreq == NULL)
		return NULL;

	sdreq->tlv_len = uri_len + 3;

	if (uri[uri_len - 1] == 0)
		sdreq->tlv_len--;

	sdreq->tlv = kzalloc(sdreq->tlv_len + 1, GFP_KERNEL);
	if (sdreq->tlv == NULL) {
		kfree(sdreq);
		return NULL;
	}

	sdreq->tlv[0] = LLCP_TLV_SDREQ;
	sdreq->tlv[1] = sdreq->tlv_len - 2;
	sdreq->tlv[2] = tid;

	sdreq->tid = tid;
	sdreq->uri = sdreq->tlv + 3;
	memcpy(sdreq->uri, uri, uri_len);

	sdreq->time = jiffies;

	INIT_HLIST_NODE(&sdreq->node);

	return sdreq;
}
