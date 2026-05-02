rdata_copy(sldns_buffer* pkt, struct packed_rrset_data* data, uint8_t* to, 
	struct rr_parse* rr, time_t* rr_ttl, uint16_t type,
	sldns_pkt_section section)
{
	uint16_t pkt_len;
	const sldns_rr_descriptor* desc;

	*rr_ttl = sldns_read_uint32(rr->ttl_data);
	/* RFC 2181 Section 8. if msb of ttl is set treat as if zero. */
	if(*rr_ttl & 0x80000000U)
		*rr_ttl = 0;
	if(type == LDNS_RR_TYPE_SOA && section == LDNS_SECTION_AUTHORITY) {
		/* negative response. see if TTL of SOA record larger than the
		 * minimum-ttl in the rdata of the SOA record */
		if(*rr_ttl > soa_find_minttl(rr))
			*rr_ttl = soa_find_minttl(rr);
		if(*rr_ttl > MAX_NEG_TTL)
			*rr_ttl = MAX_NEG_TTL;
	}
	if(*rr_ttl < MIN_TTL)
		*rr_ttl = MIN_TTL;
	if(*rr_ttl > MAX_TTL)
		*rr_ttl = MAX_TTL;
	if(*rr_ttl < data->ttl)
		data->ttl = *rr_ttl;

	if(rr->outside_packet) {
		/* uncompressed already, only needs copy */
		memmove(to, rr->ttl_data+sizeof(uint32_t), rr->size);
		return 1;
	}

	sldns_buffer_set_position(pkt, (size_t)
		(rr->ttl_data - sldns_buffer_begin(pkt) + sizeof(uint32_t)));
	/* insert decompressed size into rdata len stored in memory */
	/* -2 because rdatalen bytes are not included. */
	pkt_len = htons(rr->size - 2);
	memmove(to, &pkt_len, sizeof(uint16_t));
	to += 2;
	/* read packet rdata len */
	pkt_len = sldns_buffer_read_u16(pkt);
	if(sldns_buffer_remaining(pkt) < pkt_len)
		return 0;
	desc = sldns_rr_descript(type);
	if(pkt_len > 0 && desc && desc->_dname_count > 0) {
		int count = (int)desc->_dname_count;
		int rdf = 0;
		size_t len;
		size_t oldpos;
		/* decompress dnames. */
		while(pkt_len > 0 && count) {
			switch(desc->_wireformat[rdf]) {
			case LDNS_RDF_TYPE_DNAME:
				oldpos = sldns_buffer_position(pkt);
				dname_pkt_copy(pkt, to, 
					sldns_buffer_current(pkt));
				to += pkt_dname_len(pkt);
				pkt_len -= sldns_buffer_position(pkt)-oldpos;
				count--;
				len = 0;
				break;
			case LDNS_RDF_TYPE_STR:
				len = sldns_buffer_current(pkt)[0] + 1;
				break;
			default:
				len = get_rdf_size(desc->_wireformat[rdf]);
				break;
			}
			if(len) {
				memmove(to, sldns_buffer_current(pkt), len);
				to += len;
				sldns_buffer_skip(pkt, (ssize_t)len);
				log_assert(len <= pkt_len);
				pkt_len -= len;
			}
			rdf++;
		}
	}
	/* copy remaining rdata */
	if(pkt_len >  0)
		memmove(to, sldns_buffer_current(pkt), pkt_len);
	
	return 1;
}