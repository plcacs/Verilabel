gpg_ctx_add_recipient (struct _GpgCtx *gpg,
                       const gchar *keyid)
{
	if (gpg->mode != GPG_CTX_MODE_ENCRYPT && gpg->mode != GPG_CTX_MODE_EXPORT)
		return;

	if (!gpg->recipients)
		gpg->recipients = g_ptr_array_new ();

	g_ptr_array_add (gpg->recipients, g_strdup (keyid));
}