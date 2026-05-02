ciphertext_to_compressed (gnutls_session_t session,
                          gnutls_datum_t *ciphertext, 
                          gnutls_datum_t * compressed,
                          uint8_t type, record_parameters_st * params, 
                          uint64* sequence)
{
  uint8_t tag[MAX_HASH_SIZE];
  unsigned int pad = 0, i;
  int length, length_to_decrypt;
  uint16_t blocksize;
  int ret;
  unsigned int tmp_pad_failed = 0;
  unsigned int pad_failed = 0;
  uint8_t preamble[MAX_PREAMBLE_SIZE];
  unsigned int preamble_size;
  unsigned int ver = gnutls_protocol_get_version (session);
  unsigned int tag_size = _gnutls_auth_cipher_tag_len (&params->read.cipher_state);
  unsigned int explicit_iv = _gnutls_version_has_explicit_iv (session->security_parameters.version);

  blocksize = gnutls_cipher_get_block_size (params->cipher_algorithm);


  /* actual decryption (inplace)
   */
  switch (_gnutls_cipher_is_block (params->cipher_algorithm))
    {
    case CIPHER_STREAM:
      /* The way AEAD ciphers are defined in RFC5246, it allows
       * only stream ciphers.
       */
      if (explicit_iv && _gnutls_auth_cipher_is_aead(&params->read.cipher_state))
        {
          uint8_t nonce[blocksize];
          /* Values in AEAD are pretty fixed in TLS 1.2 for 128-bit block
           */
          if (params->read.IV.data == NULL || params->read.IV.size != 4)
            return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
          
          if (ciphertext->size < tag_size+AEAD_EXPLICIT_DATA_SIZE)
            return gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);

          memcpy(nonce, params->read.IV.data, AEAD_IMPLICIT_DATA_SIZE);
          memcpy(&nonce[AEAD_IMPLICIT_DATA_SIZE], ciphertext->data, AEAD_EXPLICIT_DATA_SIZE);
          
          _gnutls_auth_cipher_setiv(&params->read.cipher_state, nonce, AEAD_EXPLICIT_DATA_SIZE+AEAD_IMPLICIT_DATA_SIZE);

          ciphertext->data += AEAD_EXPLICIT_DATA_SIZE;
          ciphertext->size -= AEAD_EXPLICIT_DATA_SIZE;
          
          length_to_decrypt = ciphertext->size - tag_size;
        }
      else
        {
          if (ciphertext->size < tag_size)
            return gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);
  
          length_to_decrypt = ciphertext->size;
        }

      length = ciphertext->size - tag_size;

      /* Pass the type, version, length and compressed through
       * MAC.
       */
      preamble_size =
        make_preamble (UINT64DATA(*sequence), type,
                       length, ver, preamble);

      ret = _gnutls_auth_cipher_add_auth (&params->read.cipher_state, preamble, preamble_size);
      if (ret < 0)
        return gnutls_assert_val(ret);

      if ((ret =
           _gnutls_auth_cipher_decrypt2 (&params->read.cipher_state,
             ciphertext->data, length_to_decrypt,
             ciphertext->data, ciphertext->size)) < 0)
        return gnutls_assert_val(ret);

      break;
    case CIPHER_BLOCK:
      if (ciphertext->size < blocksize || (ciphertext->size % blocksize != 0))
        return gnutls_assert_val(GNUTLS_E_UNEXPECTED_PACKET_LENGTH);

      /* ignore the IV in TLS 1.1+
       */
      if (explicit_iv)
        {
          _gnutls_auth_cipher_setiv(&params->read.cipher_state,
            ciphertext->data, blocksize);

          ciphertext->size -= blocksize;
          ciphertext->data += blocksize;
        }

      if (ciphertext->size < tag_size+1)
        return gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);

      /* we don't use the auth_cipher interface here, since
       * TLS with block ciphers is impossible to be used under such
       * an API. (the length of plaintext is required to calculate
       * auth_data, but it is not available before decryption).
       */
      if ((ret =
           _gnutls_cipher_decrypt (&params->read.cipher_state.cipher,
             ciphertext->data, ciphertext->size)) < 0)
        return gnutls_assert_val(ret);

      pad = ciphertext->data[ciphertext->size - 1];   /* pad */

      /* Check the pading bytes (TLS 1.x). 
       * Note that we access all 256 bytes of ciphertext for padding check
       * because there is a timing channel in that memory access (in certain CPUs).
       */
      if (ver != GNUTLS_SSL3)
        for (i = 2; i <= MIN(256, ciphertext->size); i++)
          {
            tmp_pad_failed |= (ciphertext->data[ciphertext->size - i] != pad);
            pad_failed |= ((i<= (1+pad)) & (tmp_pad_failed));
          }

      if (pad_failed != 0 || (1+pad > ((int) ciphertext->size - tag_size)))
        {
          /* We do not fail here. We check below for the
           * the pad_failed. If zero means success.
           */
          pad_failed = 1;
          pad = 0;
        }

      length = ciphertext->size - tag_size - pad - 1;

      /* Pass the type, version, length and compressed through
       * MAC.
       */
      preamble_size =
        make_preamble (UINT64DATA(*sequence), type,
                       length, ver, preamble);
      ret = _gnutls_auth_cipher_add_auth (&params->read.cipher_state, preamble, preamble_size);
      if (ret < 0)
        return gnutls_assert_val(ret);

      ret = _gnutls_auth_cipher_add_auth (&params->read.cipher_state, ciphertext->data, length);
      if (ret < 0)
        return gnutls_assert_val(ret);

      break;
    default:
      return gnutls_assert_val(GNUTLS_E_INTERNAL_ERROR);
    }

  ret = _gnutls_auth_cipher_tag(&params->read.cipher_state, tag, tag_size);
  if (ret < 0)
    return gnutls_assert_val(ret);

  if (memcmp (tag, &ciphertext->data[length], tag_size) != 0 || pad_failed != 0)
    {
      /* HMAC was not the same. */
      dummy_wait(params, compressed, pad_failed, pad, length+preamble_size);

      return gnutls_assert_val(GNUTLS_E_DECRYPTION_FAILED);
    }

  /* copy the decrypted stuff to compressed_data.
   */
  if (compressed->size < (unsigned)length)
    return gnutls_assert_val(GNUTLS_E_DECOMPRESSION_FAILED);

  memcpy (compressed->data, ciphertext->data, length);

  return length;
}