BOOL security_fips_decrypt(BYTE* data, size_t length, rdpRdp* rdp)
{
	size_t olen;

	if (!rdp || !rdp->fips_decrypt)
		return FALSE;

	if (!winpr_Cipher_Update(rdp->fips_decrypt, data, length, data, &olen))
		return FALSE;

	return TRUE;
}