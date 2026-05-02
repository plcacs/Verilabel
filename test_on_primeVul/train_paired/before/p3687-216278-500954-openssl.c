void ssl3_cbc_copy_mac(unsigned char* out,
		       const SSL3_RECORD *rec,
		       unsigned md_size,unsigned orig_len)
	{
#if defined(CBC_MAC_ROTATE_IN_PLACE)
	unsigned char rotated_mac_buf[EVP_MAX_MD_SIZE*2];
	unsigned char *rotated_mac;
#else
	unsigned char rotated_mac[EVP_MAX_MD_SIZE];
#endif

	/* mac_end is the index of |rec->data| just after the end of the MAC. */
	unsigned mac_end = rec->length;
	unsigned mac_start = mac_end - md_size;
	/* scan_start contains the number of bytes that we can ignore because
	 * the MAC's position can only vary by 255 bytes. */
	unsigned scan_start = 0;
	unsigned i, j;
	unsigned div_spoiler;
	unsigned rotate_offset;

	OPENSSL_assert(orig_len >= md_size);
	OPENSSL_assert(md_size <= EVP_MAX_MD_SIZE);

#if defined(CBC_MAC_ROTATE_IN_PLACE)
	rotated_mac = (unsigned char*) (((intptr_t)(rotated_mac_buf + 64)) & ~63);
#endif

	/* This information is public so it's safe to branch based on it. */
	if (orig_len > md_size + 255 + 1)
		scan_start = orig_len - (md_size + 255 + 1);
	/* div_spoiler contains a multiple of md_size that is used to cause the
	 * modulo operation to be constant time. Without this, the time varies
	 * based on the amount of padding when running on Intel chips at least.
	 *
	 * The aim of right-shifting md_size is so that the compiler doesn't
	 * figure out that it can remove div_spoiler as that would require it
	 * to prove that md_size is always even, which I hope is beyond it. */
	div_spoiler = md_size >> 1;
	div_spoiler <<= (sizeof(div_spoiler)-1)*8;
	rotate_offset = (div_spoiler + mac_start - scan_start) % md_size;

	memset(rotated_mac, 0, md_size);
	for (i = scan_start, j = 0; i < orig_len; i++)
		{
		unsigned char mac_started = constant_time_ge(i, mac_start);
		unsigned char mac_ended = constant_time_ge(i, mac_end);
		unsigned char b = rec->data[i];
		rotated_mac[j++] |= b & mac_started & ~mac_ended;
		j &= constant_time_lt(j,md_size);
		}

	/* Now rotate the MAC */
#if defined(CBC_MAC_ROTATE_IN_PLACE)
	j = 0;
	for (i = 0; i < md_size; i++)
		{
		out[j++] = rotated_mac[rotate_offset++];
		rotate_offset &= constant_time_lt(rotate_offset,md_size);
		}
#else
	memset(out, 0, md_size);
	rotate_offset = md_size - rotate_offset;
	rotate_offset &= constant_time_lt(rotate_offset,md_size);
	for (i = 0; i < md_size; i++)
		{
		for (j = 0; j < md_size; j++)
			out[j] |= rotated_mac[i] & constant_time_eq_8(j, rotate_offset);
		rotate_offset++;
		rotate_offset &= constant_time_lt(rotate_offset,md_size);
		}
#endif
	}