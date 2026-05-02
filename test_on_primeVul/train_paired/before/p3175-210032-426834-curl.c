CURLcode Curl_ntlm_core_mk_nt_hash(struct Curl_easy *data,
                                   const char *password,
                                   unsigned char *ntbuffer /* 21 bytes */)
{
  size_t len = strlen(password);
  unsigned char *pw = len ? malloc(len * 2) : strdup("");
  CURLcode result;
  if(!pw)
    return CURLE_OUT_OF_MEMORY;

  ascii_to_unicode_le(pw, password, len);

  /*
   * The NT hashed password needs to be created using the password in the
   * network encoding not the host encoding.
   */
  result = Curl_convert_to_network(data, (char *)pw, len * 2);
  if(result)
    return result;

  {
    /* Create NT hashed password. */
#ifdef USE_OPENSSL
    MD4_CTX MD4pw;
    MD4_Init(&MD4pw);
    MD4_Update(&MD4pw, pw, 2 * len);
    MD4_Final(ntbuffer, &MD4pw);
#elif defined(USE_GNUTLS_NETTLE)
    struct md4_ctx MD4pw;
    md4_init(&MD4pw);
    md4_update(&MD4pw, (unsigned int)(2 * len), pw);
    md4_digest(&MD4pw, MD4_DIGEST_SIZE, ntbuffer);
#elif defined(USE_GNUTLS)
    gcry_md_hd_t MD4pw;
    gcry_md_open(&MD4pw, GCRY_MD_MD4, 0);
    gcry_md_write(MD4pw, pw, 2 * len);
    memcpy(ntbuffer, gcry_md_read(MD4pw, 0), MD4_DIGEST_LENGTH);
    gcry_md_close(MD4pw);
#elif defined(USE_NSS)
    Curl_md4it(ntbuffer, pw, 2 * len);
#elif defined(USE_MBEDTLS)
#if defined(MBEDTLS_MD4_C)
    mbedtls_md4(pw, 2 * len, ntbuffer);
#else
    Curl_md4it(ntbuffer, pw, 2 * len);
#endif
#elif defined(USE_DARWINSSL)
    (void)CC_MD4(pw, (CC_LONG)(2 * len), ntbuffer);
#elif defined(USE_OS400CRYPTO)
    Curl_md4it(ntbuffer, pw, 2 * len);
#elif defined(USE_WIN32_CRYPTO)
    HCRYPTPROV hprov;
    if(CryptAcquireContext(&hprov, NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT)) {
      HCRYPTHASH hhash;
      if(CryptCreateHash(hprov, CALG_MD4, 0, 0, &hhash)) {
        DWORD length = 16;
        CryptHashData(hhash, pw, (unsigned int)len * 2, 0);
        CryptGetHashParam(hhash, HP_HASHVAL, ntbuffer, &length, 0);
        CryptDestroyHash(hhash);
      }
      CryptReleaseContext(hprov, 0);
    }
#endif

    memset(ntbuffer + 16, 0, 21 - 16);
  }

  free(pw);

  return CURLE_OK;
}