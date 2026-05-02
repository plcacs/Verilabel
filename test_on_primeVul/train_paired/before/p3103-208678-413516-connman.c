static char *uncompress(int16_t field_count, char *start, char *end,
			char *ptr, char *uncompressed, int uncomp_len,
			char **uncompressed_ptr)
{
	char *uptr = *uncompressed_ptr; /* position in result buffer */

	debug("count %d ptr %p end %p uptr %p", field_count, ptr, end, uptr);

	while (field_count-- > 0 && ptr < end) {
		int dlen;		/* data field length */
		int ulen;		/* uncompress length */
		int pos;		/* position in compressed string */
		char name[NS_MAXLABEL]; /* tmp label */
		uint16_t dns_type, dns_class;
		int comp_pos;

		if (!convert_label(start, end, ptr, name, NS_MAXLABEL,
					&pos, &comp_pos))
			goto out;

		/*
		 * Copy the uncompressed resource record, type, class and \0 to
		 * tmp buffer.
		 */

		ulen = strlen(name);
		strncpy(uptr, name, uncomp_len - (uptr - uncompressed));

		debug("pos %d ulen %d left %d name %s", pos, ulen,
			(int)(uncomp_len - (uptr - uncompressed)), uptr);

		uptr += ulen;
		*uptr++ = '\0';

		ptr += pos;

		/*
		 * We copy also the fixed portion of the result (type, class,
		 * ttl, address length and the address)
		 */
		memcpy(uptr, ptr, NS_RRFIXEDSZ);

		dns_type = uptr[0] << 8 | uptr[1];
		dns_class = uptr[2] << 8 | uptr[3];

		if (dns_class != ns_c_in)
			goto out;

		ptr += NS_RRFIXEDSZ;
		uptr += NS_RRFIXEDSZ;

		/*
		 * Then the variable portion of the result (data length).
		 * Typically this portion is also compressed
		 * so we need to uncompress it also when necessary.
		 */
		if (dns_type == ns_t_cname) {
			if (!convert_label(start, end, ptr, uptr,
					uncomp_len - (uptr - uncompressed),
						&pos, &comp_pos))
				goto out;

			uptr[-2] = comp_pos << 8;
			uptr[-1] = comp_pos & 0xff;

			uptr += comp_pos;
			ptr += pos;

		} else if (dns_type == ns_t_a || dns_type == ns_t_aaaa) {
			dlen = uptr[-2] << 8 | uptr[-1];

			if (ptr + dlen > end) {
				debug("data len %d too long", dlen);
				goto out;
			}

			memcpy(uptr, ptr, dlen);
			uptr += dlen;
			ptr += dlen;

		} else if (dns_type == ns_t_soa) {
			int total_len = 0;
			char *len_ptr;

			/* Primary name server expansion */
			if (!convert_label(start, end, ptr, uptr,
					uncomp_len - (uptr - uncompressed),
						&pos, &comp_pos))
				goto out;

			total_len += comp_pos;
			len_ptr = &uptr[-2];
			ptr += pos;
			uptr += comp_pos;

			/* Responsible authority's mailbox */
			if (!convert_label(start, end, ptr, uptr,
					uncomp_len - (uptr - uncompressed),
						&pos, &comp_pos))
				goto out;

			total_len += comp_pos;
			ptr += pos;
			uptr += comp_pos;

			/*
			 * Copy rest of the soa fields (serial number,
			 * refresh interval, retry interval, expiration
			 * limit and minimum ttl). They are 20 bytes long.
			 */
			memcpy(uptr, ptr, 20);
			uptr += 20;
			ptr += 20;
			total_len += 20;

			/*
			 * Finally fix the length of the data part
			 */
			len_ptr[0] = total_len << 8;
			len_ptr[1] = total_len & 0xff;
		}

		*uncompressed_ptr = uptr;
	}

	return ptr;

out:
	return NULL;
}