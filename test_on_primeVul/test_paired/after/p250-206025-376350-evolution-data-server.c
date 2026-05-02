gpg_ctx_add_recipient (struct _GpgCtx *gpg,
                       const gchar *keyid)
{
	gchar *safe_keyid;

	if (gpg->mode != GPG_CTX_MODE_ENCRYPT && gpg->mode != GPG_CTX_MODE_EXPORT)
		return;

	if (!gpg->recipients)
		gpg->recipients = g_ptr_array_new ();

	g_return_if_fail (keyid != NULL);

	/* If the recipient looks like an email address,
	 * enclose it in brackets to ensure an exact match. */
	if (strchr (keyid, '@') != NULL) {
		safe_keyid = g_strdup_printf ("<%s>", keyid);
	} else {
		safe_keyid = g_strdup (keyid);
	}

	g_ptr_array_add (gpg->recipients, safe_keyid);
}