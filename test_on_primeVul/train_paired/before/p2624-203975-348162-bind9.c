dns_message_parse(dns_message_t *msg, isc_buffer_t *source,
		  unsigned int options)
{
	isc_region_t r;
	dns_decompress_t dctx;
	isc_result_t ret;
	uint16_t tmpflags;
	isc_buffer_t origsource;
	bool seen_problem;
	bool ignore_tc;

	REQUIRE(DNS_MESSAGE_VALID(msg));
	REQUIRE(source != NULL);
	REQUIRE(msg->from_to_wire == DNS_MESSAGE_INTENTPARSE);

	seen_problem = false;
	ignore_tc = ((options & DNS_MESSAGEPARSE_IGNORETRUNCATION) != 0);

	origsource = *source;

	msg->header_ok = 0;
	msg->question_ok = 0;

	isc_buffer_remainingregion(source, &r);
	if (r.length < DNS_MESSAGE_HEADERLEN)
		return (ISC_R_UNEXPECTEDEND);

	msg->id = isc_buffer_getuint16(source);
	tmpflags = isc_buffer_getuint16(source);
	msg->opcode = ((tmpflags & DNS_MESSAGE_OPCODE_MASK)
		       >> DNS_MESSAGE_OPCODE_SHIFT);
	msg->rcode = (dns_rcode_t)(tmpflags & DNS_MESSAGE_RCODE_MASK);
	msg->flags = (tmpflags & DNS_MESSAGE_FLAG_MASK);
	msg->counts[DNS_SECTION_QUESTION] = isc_buffer_getuint16(source);
	msg->counts[DNS_SECTION_ANSWER] = isc_buffer_getuint16(source);
	msg->counts[DNS_SECTION_AUTHORITY] = isc_buffer_getuint16(source);
	msg->counts[DNS_SECTION_ADDITIONAL] = isc_buffer_getuint16(source);

	msg->header_ok = 1;
	msg->state = DNS_SECTION_QUESTION;

	/*
	 * -1 means no EDNS.
	 */
	dns_decompress_init(&dctx, -1, DNS_DECOMPRESS_ANY);

	dns_decompress_setmethods(&dctx, DNS_COMPRESS_GLOBAL14);

	ret = getquestions(source, msg, &dctx, options);
	if (ret == ISC_R_UNEXPECTEDEND && ignore_tc)
		goto truncated;
	if (ret == DNS_R_RECOVERABLE) {
		seen_problem = true;
		ret = ISC_R_SUCCESS;
	}
	if (ret != ISC_R_SUCCESS)
		return (ret);
	msg->question_ok = 1;

	ret = getsection(source, msg, &dctx, DNS_SECTION_ANSWER, options);
	if (ret == ISC_R_UNEXPECTEDEND && ignore_tc)
		goto truncated;
	if (ret == DNS_R_RECOVERABLE) {
		seen_problem = true;
		ret = ISC_R_SUCCESS;
	}
	if (ret != ISC_R_SUCCESS)
		return (ret);

	ret = getsection(source, msg, &dctx, DNS_SECTION_AUTHORITY, options);
	if (ret == ISC_R_UNEXPECTEDEND && ignore_tc)
		goto truncated;
	if (ret == DNS_R_RECOVERABLE) {
		seen_problem = true;
		ret = ISC_R_SUCCESS;
	}
	if (ret != ISC_R_SUCCESS)
		return (ret);

	ret = getsection(source, msg, &dctx, DNS_SECTION_ADDITIONAL, options);
	if (ret == ISC_R_UNEXPECTEDEND && ignore_tc)
		goto truncated;
	if (ret == DNS_R_RECOVERABLE) {
		seen_problem = true;
		ret = ISC_R_SUCCESS;
	}
	if (ret != ISC_R_SUCCESS)
		return (ret);

	isc_buffer_remainingregion(source, &r);
	if (r.length != 0) {
		isc_log_write(dns_lctx, ISC_LOGCATEGORY_GENERAL,
			      DNS_LOGMODULE_MESSAGE, ISC_LOG_DEBUG(3),
			      "message has %u byte(s) of trailing garbage",
			      r.length);
	}

 truncated:
	if ((options & DNS_MESSAGEPARSE_CLONEBUFFER) == 0)
		isc_buffer_usedregion(&origsource, &msg->saved);
	else {
		msg->saved.length = isc_buffer_usedlength(&origsource);
		msg->saved.base = isc_mem_get(msg->mctx, msg->saved.length);
		if (msg->saved.base == NULL)
			return (ISC_R_NOMEMORY);
		memmove(msg->saved.base, isc_buffer_base(&origsource),
			msg->saved.length);
		msg->free_saved = 1;
	}

	if (ret == ISC_R_UNEXPECTEDEND && ignore_tc)
		return (DNS_R_RECOVERABLE);
	if (seen_problem == true)
		return (DNS_R_RECOVERABLE);
	return (ISC_R_SUCCESS);
}