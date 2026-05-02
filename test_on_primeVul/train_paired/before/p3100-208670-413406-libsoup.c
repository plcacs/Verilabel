soup_ntlm_parse_challenge (const char *challenge,
			   char      **nonce,
			   char      **default_domain,
			   gboolean   *ntlmv2_session)
{
	gsize clen;
	NTLMString domain;
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

	g_free (chall);
	return TRUE;
}