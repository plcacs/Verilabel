soup_ntlm_parse_challenge (const char *challenge,
			   char      **nonce,
			   char      **default_domain,
			   gboolean   *ntlmv2_session,
			   gboolean   *negotiate_target,
			   char		**target_info,
			   size_t	*target_info_sz)
{
	gsize clen;
	NTLMString domain;
	NTLMString target;
	guchar *chall;
	guint32 flags;

	chall = g_base64_decode (challenge, &clen);
	if (clen < NTLM_CHALLENGE_DOMAIN_STRING_OFFSET ||
	    clen < NTLM_CHALLENGE_NONCE_OFFSET + NTLM_CHALLENGE_NONCE_LENGTH) {
		g_free (chall);
		return FALSE;
	}

	memcpy (&flags, chall + NTLM_CHALLENGE_FLAGS_OFFSET, sizeof(flags));
	flags = GUINT_FROM_LE (flags);
	*ntlmv2_session = (flags & NTLM_FLAGS_NEGOTIATE_NTLMV2) ? TRUE : FALSE;
	/* To know if NTLMv2 responses should be calculated */
	*negotiate_target = (flags & NTLM_FLAGS_NEGOTIATE_TARGET_INFORMATION ) ? TRUE : FALSE;

	if (default_domain) {
		memcpy (&domain, chall + NTLM_CHALLENGE_DOMAIN_STRING_OFFSET, sizeof (domain));
		domain.length = GUINT16_FROM_LE (domain.length);
		domain.offset = GUINT16_FROM_LE (domain.offset);

		if (clen < domain.length + domain.offset) {
			g_free (chall);
			return FALSE;
		}

		*default_domain = g_convert ((char *)chall + domain.offset,
					     domain.length, "UTF-8", "UCS-2LE",
					     NULL, NULL, NULL);
	}

	if (nonce) {
		*nonce = g_memdup (chall + NTLM_CHALLENGE_NONCE_OFFSET,
				   NTLM_CHALLENGE_NONCE_LENGTH);
	}
	/* For NTLMv2 response */
	if (*negotiate_target && target_info) {
		memcpy (&target, chall + NTLM_CHALLENGE_TARGET_INFORMATION_OFFSET, sizeof (target));
		target.length = GUINT16_FROM_LE (target.length);
		target.offset = GUINT16_FROM_LE (target.offset);

		if (clen < target.length + target.offset) {
			g_free (chall);
			return FALSE;
		}
		*target_info = g_memdup (chall + target.offset, target.length);
		*target_info_sz = target.length;
	}

	g_free (chall);
	return TRUE;
}