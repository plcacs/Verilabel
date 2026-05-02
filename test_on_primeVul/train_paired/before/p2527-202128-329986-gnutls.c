u32 cdk_pk_get_keyid(cdk_pubkey_t pk, u32 * keyid)
{
	u32 lowbits = 0;
	byte buf[24];

	if (pk && (!pk->keyid[0] || !pk->keyid[1])) {
		if (pk->version < 4 && is_RSA(pk->pubkey_algo)) {
			byte p[MAX_MPI_BYTES];
			size_t n;

			n = MAX_MPI_BYTES;
			_gnutls_mpi_print(pk->mpi[0], p, &n);
			pk->keyid[0] =
			    p[n - 8] << 24 | p[n - 7] << 16 | p[n -
								6] << 8 |
			    p[n - 5];
			pk->keyid[1] =
			    p[n - 4] << 24 | p[n - 3] << 16 | p[n -
								2] << 8 |
			    p[n - 1];
		} else if (pk->version == 4) {
			cdk_pk_get_fingerprint(pk, buf);
			pk->keyid[0] = _cdk_buftou32(buf + 12);
			pk->keyid[1] = _cdk_buftou32(buf + 16);
		}
	}
	lowbits = pk ? pk->keyid[1] : 0;
	if (keyid && pk) {
		keyid[0] = pk->keyid[0];
		keyid[1] = pk->keyid[1];
	}

	return lowbits;
}