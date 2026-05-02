call_hook(struct module_qstate* qstate, struct ipsecmod_qstate* iq,
	struct ipsecmod_env* ATTR_UNUSED(ie))
{
	size_t slen, tempdata_len, tempstring_len, i;
	char str[65535], *s, *tempstring;
	int w = 0, w_temp, qtype;
	struct ub_packed_rrset_key* rrset_key;
	struct packed_rrset_data* rrset_data;
	uint8_t *tempdata;

	/* Check if a shell is available */
	if(system(NULL) == 0) {
		log_err("ipsecmod: no shell available for ipsecmod-hook");
		return 0;
	}

	/* Zero the buffer. */
	s = str;
	slen = sizeof(str);
	memset(s, 0, slen);

	/* Copy the hook into the buffer. */
	w += sldns_str_print(&s, &slen, "%s", qstate->env->cfg->ipsecmod_hook);
	/* Put space into the buffer. */
	w += sldns_str_print(&s, &slen, " ");
	/* Copy the qname into the buffer. */
	tempstring = sldns_wire2str_dname(qstate->qinfo.qname,
		qstate->qinfo.qname_len);
	if(!tempstring) {
		log_err("ipsecmod: out of memory when calling the hook");
		return 0;
	}
	if(!domainname_has_safe_characters(tempstring, strlen(tempstring))) {
		log_err("ipsecmod: qname has unsafe characters");
		free(tempstring);
		return 0;
	}
	w += sldns_str_print(&s, &slen, "\"%s\"", tempstring);
	free(tempstring);
	/* Put space into the buffer. */
	w += sldns_str_print(&s, &slen, " ");
	/* Copy the IPSECKEY TTL into the buffer. */
	rrset_data = (struct packed_rrset_data*)iq->ipseckey_rrset->entry.data;
	w += sldns_str_print(&s, &slen, "\"%ld\"", (long)rrset_data->ttl);
	/* Put space into the buffer. */
	w += sldns_str_print(&s, &slen, " ");
	rrset_key = reply_find_answer_rrset(&qstate->return_msg->qinfo,
		qstate->return_msg->rep);
	/* Double check that the records are indeed A/AAAA.
	 * This should never happen as this function is only executed for A/AAAA
	 * queries but make sure we don't pass anything other than A/AAAA to the
	 * shell. */
	qtype = ntohs(rrset_key->rk.type);
	if(qtype != LDNS_RR_TYPE_AAAA && qtype != LDNS_RR_TYPE_A) {
		log_err("ipsecmod: Answer is not of A or AAAA type");
		return 0;
	}
	rrset_data = (struct packed_rrset_data*)rrset_key->entry.data;
	/* Copy the A/AAAA record(s) into the buffer. Start and end this section
	 * with a double quote. */
	w += sldns_str_print(&s, &slen, "\"");
	for(i=0; i<rrset_data->count; i++) {
		if(i > 0) {
			/* Put space into the buffer. */
			w += sldns_str_print(&s, &slen, " ");
		}
		/* Ignore the first two bytes, they are the rr_data len. */
		w_temp = sldns_wire2str_rdata_buf(rrset_data->rr_data[i] + 2,
			rrset_data->rr_len[i] - 2, s, slen, qstate->qinfo.qtype);
		if(w_temp < 0) {
			/* Error in printout. */
			log_err("ipsecmod: Error in printing IP address");
			return 0;
		} else if((size_t)w_temp >= slen) {
			s = NULL; /* We do not want str to point outside of buffer. */
			slen = 0;
			log_err("ipsecmod: shell command too long");
			return 0;
		} else {
			s += w_temp;
			slen -= w_temp;
			w += w_temp;
		}
	}
	w += sldns_str_print(&s, &slen, "\"");
	/* Put space into the buffer. */
	w += sldns_str_print(&s, &slen, " ");
	/* Copy the IPSECKEY record(s) into the buffer. Start and end this section
	 * with a double quote. */
	w += sldns_str_print(&s, &slen, "\"");
	rrset_data = (struct packed_rrset_data*)iq->ipseckey_rrset->entry.data;
	for(i=0; i<rrset_data->count; i++) {
		if(i > 0) {
			/* Put space into the buffer. */
			w += sldns_str_print(&s, &slen, " ");
		}
		/* Ignore the first two bytes, they are the rr_data len. */
		tempdata = rrset_data->rr_data[i] + 2;
		tempdata_len = rrset_data->rr_len[i] - 2;
		/* Save the buffer pointers. */
		tempstring = s; tempstring_len = slen;
		w_temp = sldns_wire2str_ipseckey_scan(&tempdata, &tempdata_len, &s,
			&slen, NULL, 0);
		/* There was an error when parsing the IPSECKEY; reset the buffer
		 * pointers to their previous values. */
		if(w_temp == -1) {
			s = tempstring; slen = tempstring_len;
		} else if(w_temp > 0) {
			if(!ipseckey_has_safe_characters(
					tempstring, tempstring_len - slen)) {
				log_err("ipsecmod: ipseckey has unsafe characters");
				return 0;
			}
			w += w_temp;
		}
	}
	w += sldns_str_print(&s, &slen, "\"");
	if(w >= (int)sizeof(str)) {
		log_err("ipsecmod: shell command too long");
		return 0;
	}
	verbose(VERB_ALGO, "ipsecmod: shell command: '%s'", str);
	/* ipsecmod-hook should return 0 on success. */
	if(system(str) != 0)
		return 0;
	return 1;
}