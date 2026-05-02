PJ_DEF(pj_status_t) pjstun_parse_msg( void *buf, pj_size_t buf_len, 
				      pjstun_msg *msg)
{
    pj_uint16_t msg_type, msg_len;
    char *p_attr;

    PJ_CHECK_STACK();

    msg->hdr = (pjstun_msg_hdr*)buf;
    msg_type = pj_ntohs(msg->hdr->type);

    switch (msg_type) {
    case PJSTUN_BINDING_REQUEST:
    case PJSTUN_BINDING_RESPONSE:
    case PJSTUN_BINDING_ERROR_RESPONSE:
    case PJSTUN_SHARED_SECRET_REQUEST:
    case PJSTUN_SHARED_SECRET_RESPONSE:
    case PJSTUN_SHARED_SECRET_ERROR_RESPONSE:
	break;
    default:
	PJ_LOG(4,(THIS_FILE, "Error: unknown msg type %d", msg_type));
	return PJLIB_UTIL_ESTUNINMSGTYPE;
    }

    msg_len = pj_ntohs(msg->hdr->length);
    if (msg_len != buf_len - sizeof(pjstun_msg_hdr)) {
	PJ_LOG(4,(THIS_FILE, "Error: invalid msg_len %d (expecting %d)", 
			     msg_len, buf_len - sizeof(pjstun_msg_hdr)));
	return PJLIB_UTIL_ESTUNINMSGLEN;
    }

    msg->attr_count = 0;
    p_attr = (char*)buf + sizeof(pjstun_msg_hdr);

    while (msg_len > 0) {
	pjstun_attr_hdr **attr = &msg->attr[msg->attr_count];
	pj_uint32_t len;
	pj_uint16_t attr_type;

	*attr = (pjstun_attr_hdr*)p_attr;
	len = pj_ntohs((pj_uint16_t) ((*attr)->length)) + sizeof(pjstun_attr_hdr);
	len = (len + 3) & ~3;

	if (msg_len < len) {
	    PJ_LOG(4,(THIS_FILE, "Error: length mismatch in attr %d", 
				 msg->attr_count));
	    return PJLIB_UTIL_ESTUNINATTRLEN;
	}

	attr_type = pj_ntohs((*attr)->type);
	if (attr_type > PJSTUN_ATTR_REFLECTED_FROM &&
	    attr_type != PJSTUN_ATTR_XOR_MAPPED_ADDR)
	{
	    PJ_LOG(5,(THIS_FILE, "Warning: unknown attr type %x in attr %d. "
				 "Attribute was ignored.",
				 attr_type, msg->attr_count));
	}

	msg_len = (pj_uint16_t)(msg_len - len);
	p_attr += len;
	++msg->attr_count;
    }

    return PJ_SUCCESS;
}