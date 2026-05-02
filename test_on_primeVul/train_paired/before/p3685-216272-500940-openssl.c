int tls1_cbc_remove_padding(const SSL* s,
			    SSL3_RECORD *rec,
			    unsigned block_size,
			    unsigned mac_size)
	{
	unsigned padding_length, good, to_check, i;
	const char has_explicit_iv =
		s->version >= TLS1_1_VERSION || s->version == DTLS1_VERSION;
	const unsigned overhead = 1 /* padding length byte */ +
				  mac_size +
				  (has_explicit_iv ? block_size : 0);

	/* These lengths are all public so we can test them in non-constant
	 * time. */
	if (overhead > rec->length)
		return 0;

	/* We can always safely skip the explicit IV. We check at the beginning
	 * of this function that the record has at least enough space for the
	 * IV, MAC and padding length byte. (These can be checked in
	 * non-constant time because it's all public information.) So, if the
	 * padding was invalid, then we didn't change |rec->length| and this is
	 * safe. If the padding was valid then we know that we have at least
	 * overhead+padding_length bytes of space and so this is still safe
	 * because overhead accounts for the explicit IV. */
	if (has_explicit_iv)
		{
		rec->data += block_size;
		rec->input += block_size;
		rec->length -= block_size;
		}

	padding_length = rec->data[rec->length-1];

	/* NB: if compression is in operation the first packet may not be of
	 * even length so the padding bug check cannot be performed. This bug
	 * workaround has been around since SSLeay so hopefully it is either
	 * fixed now or no buggy implementation supports compression [steve]
	 */
	if ( (s->options&SSL_OP_TLS_BLOCK_PADDING_BUG) && !s->expand)
		{
		/* First packet is even in size, so check */
		if ((memcmp(s->s3->read_sequence, "\0\0\0\0\0\0\0\0",8) == 0) &&
		    !(padding_length & 1))
			{
			s->s3->flags|=TLS1_FLAGS_TLS_PADDING_BUG;
			}
		if ((s->s3->flags & TLS1_FLAGS_TLS_PADDING_BUG) &&
		    padding_length > 0)
			{
			padding_length--;
			}
		}

	if (EVP_CIPHER_flags(s->enc_read_ctx->cipher)&EVP_CIPH_FLAG_AEAD_CIPHER)
		{
		/* padding is already verified */
		rec->length -= padding_length;
		return 1;
		}

	good = constant_time_ge(rec->length, overhead+padding_length);
	/* The padding consists of a length byte at the end of the record and
	 * then that many bytes of padding, all with the same value as the
	 * length byte. Thus, with the length byte included, there are i+1
	 * bytes of padding.
	 *
	 * We can't check just |padding_length+1| bytes because that leaks
	 * decrypted information. Therefore we always have to check the maximum
	 * amount of padding possible. (Again, the length of the record is
	 * public information so we can use it.) */
	to_check = 255; /* maximum amount of padding. */
	if (to_check > rec->length-1)
		to_check = rec->length-1;

	for (i = 0; i < to_check; i++)
		{
		unsigned char mask = constant_time_ge(padding_length, i);
		unsigned char b = rec->data[rec->length-1-i];
		/* The final |padding_length+1| bytes should all have the value
		 * |padding_length|. Therefore the XOR should be zero. */
		good &= ~(mask&(padding_length ^ b));
		}

	/* If any of the final |padding_length+1| bytes had the wrong value,
	 * one or more of the lower eight bits of |good| will be cleared. We
	 * AND the bottom 8 bits together and duplicate the result to all the
	 * bits. */
	good &= good >> 4;
	good &= good >> 2;
	good &= good >> 1;
	good <<= sizeof(good)*8-1;
	good = DUPLICATE_MSB_TO_ALL(good);

	padding_length = good & (padding_length+1);
	rec->length -= padding_length;
	rec->type |= padding_length<<8;	/* kludge: pass padding length */

	return (int)((good & 1) | (~good & -1));
	}