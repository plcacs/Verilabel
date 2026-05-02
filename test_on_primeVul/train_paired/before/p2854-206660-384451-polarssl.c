static int ssl_decrypt_buf( ssl_context *ssl )
{
    size_t i, padlen = 0, correct = 1;
    unsigned char tmp[POLARSSL_SSL_MAX_MAC_SIZE];

    SSL_DEBUG_MSG( 2, ( "=> decrypt buf" ) );

    if( ssl->in_msglen < ssl->transform_in->minlen )
    {
        SSL_DEBUG_MSG( 1, ( "in_msglen (%d) < minlen (%d)",
                       ssl->in_msglen, ssl->transform_in->minlen ) );
        return( POLARSSL_ERR_SSL_INVALID_MAC );
    }

    if( ssl->transform_in->ivlen == 0 )
    {
#if defined(POLARSSL_ARC4_C)
        if( ssl->session_in->ciphersuite == TLS_RSA_WITH_RC4_128_MD5 ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_RC4_128_SHA )
        {
            arc4_crypt( (arc4_context *) ssl->transform_in->ctx_dec,
                    ssl->in_msglen, ssl->in_msg,
                    ssl->in_msg );
        } else
#endif
#if defined(POLARSSL_CIPHER_NULL_CIPHER)
        if( ssl->session_in->ciphersuite == TLS_RSA_WITH_NULL_MD5 ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_NULL_SHA ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_NULL_SHA256 )
        {
        } else
#endif
        return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
    }
    else if( ssl->transform_in->ivlen == 12 )
    {
        unsigned char *dec_msg;
        unsigned char *dec_msg_result;
        size_t dec_msglen;
        unsigned char add_data[13];
        int ret = POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE;

#if defined(POLARSSL_AES_C) && defined(POLARSSL_GCM_C)
        if( ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_128_GCM_SHA256 ||
            ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_256_GCM_SHA384 ||
            ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_128_GCM_SHA256 ||
            ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_256_GCM_SHA384 )
        {
            dec_msglen = ssl->in_msglen - ( ssl->transform_in->ivlen -
                                            ssl->transform_in->fixed_ivlen );
            dec_msglen -= 16;
            dec_msg = ssl->in_msg + ( ssl->transform_in->ivlen -
                                      ssl->transform_in->fixed_ivlen );
            dec_msg_result = ssl->in_msg;
            ssl->in_msglen = dec_msglen;

            memcpy( add_data, ssl->in_ctr, 8 );
            add_data[8]  = ssl->in_msgtype;
            add_data[9]  = ssl->major_ver;
            add_data[10] = ssl->minor_ver;
            add_data[11] = ( ssl->in_msglen >> 8 ) & 0xFF;
            add_data[12] = ssl->in_msglen & 0xFF;

            SSL_DEBUG_BUF( 4, "additional data used for AEAD",
                           add_data, 13 );

            memcpy( ssl->transform_in->iv_dec + ssl->transform_in->fixed_ivlen,
                    ssl->in_msg,
                    ssl->transform_in->ivlen - ssl->transform_in->fixed_ivlen );

            SSL_DEBUG_BUF( 4, "IV used", ssl->transform_in->iv_dec,
                                         ssl->transform_in->ivlen );
            SSL_DEBUG_BUF( 4, "TAG used", dec_msg + dec_msglen, 16 );

            memcpy( ssl->transform_in->iv_dec + ssl->transform_in->fixed_ivlen,
                    ssl->in_msg,
                    ssl->transform_in->ivlen - ssl->transform_in->fixed_ivlen );

            ret = gcm_auth_decrypt( (gcm_context *) ssl->transform_in->ctx_dec,
                                     dec_msglen,
                                     ssl->transform_in->iv_dec,
                                     ssl->transform_in->ivlen,
                                     add_data, 13,
                                     dec_msg + dec_msglen, 16,
                                     dec_msg, dec_msg_result );
            
            if( ret != 0 )
            {
                SSL_DEBUG_MSG( 1, ( "AEAD decrypt failed on validation (ret = -0x%02x)",
                                    -ret ) );

                return( POLARSSL_ERR_SSL_INVALID_MAC );
            }
        } else
#endif
        return( ret );
    }
    else
    {
        /*
         * Decrypt and check the padding
         */
        unsigned char *dec_msg;
        unsigned char *dec_msg_result;
        size_t dec_msglen;
        size_t minlen = 0, fake_padlen;

        /*
         * Check immediate ciphertext sanity
         */
        if( ssl->in_msglen % ssl->transform_in->ivlen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "msglen (%d) %% ivlen (%d) != 0",
                           ssl->in_msglen, ssl->transform_in->ivlen ) );
            return( POLARSSL_ERR_SSL_INVALID_MAC );
        }

        if( ssl->minor_ver >= SSL_MINOR_VERSION_2 )
            minlen += ssl->transform_in->ivlen;

        if( ssl->in_msglen < minlen + ssl->transform_in->ivlen ||
            ssl->in_msglen < minlen + ssl->transform_in->maclen + 1 )
        {
            SSL_DEBUG_MSG( 1, ( "msglen (%d) < max( ivlen(%d), maclen (%d) + 1 ) ( + expl IV )",
                           ssl->in_msglen, ssl->transform_in->ivlen, ssl->transform_in->maclen ) );
            return( POLARSSL_ERR_SSL_INVALID_MAC );
        }

        dec_msglen = ssl->in_msglen;
        dec_msg = ssl->in_msg;
        dec_msg_result = ssl->in_msg;

        /*
         * Initialize for prepended IV for block cipher in TLS v1.1 and up
         */
        if( ssl->minor_ver >= SSL_MINOR_VERSION_2 )
        {
            dec_msg += ssl->transform_in->ivlen;
            dec_msglen -= ssl->transform_in->ivlen;
            ssl->in_msglen -= ssl->transform_in->ivlen;

            for( i = 0; i < ssl->transform_in->ivlen; i++ )
                ssl->transform_in->iv_dec[i] = ssl->in_msg[i];
        }

        switch( ssl->transform_in->ivlen )
        {
#if defined(POLARSSL_DES_C)
            case  8:
#if defined(POLARSSL_ENABLE_WEAK_CIPHERSUITES)
                if( ssl->session_in->ciphersuite == TLS_RSA_WITH_DES_CBC_SHA ||
                    ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_DES_CBC_SHA )
                {
                    des_crypt_cbc( (des_context *) ssl->transform_in->ctx_dec,
                                   DES_DECRYPT, dec_msglen,
                                   ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                }
                else
#endif
                    des3_crypt_cbc( (des3_context *) ssl->transform_in->ctx_dec,
                        DES_DECRYPT, dec_msglen,
                        ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                break;
#endif

            case 16:
#if defined(POLARSSL_AES_C)
        if ( ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_AES_256_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_AES_256_CBC_SHA256 )
        {
                    aes_crypt_cbc( (aes_context *) ssl->transform_in->ctx_dec,
                       AES_DECRYPT, dec_msglen,
                       ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                    break;
        }
#endif

#if defined(POLARSSL_CAMELLIA_C)
        if ( ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256 ||
             ssl->session_in->ciphersuite == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 )
        {
                    camellia_crypt_cbc( (camellia_context *) ssl->transform_in->ctx_dec,
                       CAMELLIA_DECRYPT, dec_msglen,
                       ssl->transform_in->iv_dec, dec_msg, dec_msg_result );
                    break;
        }
#endif

            default:
                return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }

        padlen = 1 + ssl->in_msg[ssl->in_msglen - 1];
        fake_padlen = 256 - padlen;

        if( ssl->in_msglen < ssl->transform_in->maclen + padlen )
        {
            SSL_DEBUG_MSG( 1, ( "msglen (%d) < maclen (%d) + padlen (%d)",
                        ssl->in_msglen, ssl->transform_in->maclen, padlen ) );

            padlen = 0;
            fake_padlen = 256;
            correct = 0;
        }

        if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
        {
            if( padlen > ssl->transform_in->ivlen )
            {
                SSL_DEBUG_MSG( 1, ( "bad padding length: is %d, "
                                    "should be no more than %d",
                               padlen, ssl->transform_in->ivlen ) );
                correct = 0;
            }
        }
        else
        {
            /*
             * TLSv1+: always check the padding up to the first failure
             * and fake check up to 256 bytes of padding
             */
            for( i = 1; i <= padlen; i++ )
            {
                if( ssl->in_msg[ssl->in_msglen - i] != padlen - 1 )
                {
                    correct = 0;
                    fake_padlen = 256 - i;
                    padlen = 0;
                }
            }
            for( i = 1; i <= fake_padlen; i++ )
            {
                if( ssl->in_msg[i + 1] != fake_padlen - 1 )
                    minlen = 0;
                else
                    minlen = 1;
            }
            if( padlen > 0 && correct == 0)
                SSL_DEBUG_MSG( 1, ( "bad padding byte detected" ) );
        }
    }

    SSL_DEBUG_BUF( 4, "raw buffer after decryption",
                   ssl->in_msg, ssl->in_msglen );

    /*
     * Always compute the MAC (RFC4346, CBCTIME).
     */
    ssl->in_msglen -= ( ssl->transform_in->maclen + padlen );

    ssl->in_hdr[3] = (unsigned char)( ssl->in_msglen >> 8 );
    ssl->in_hdr[4] = (unsigned char)( ssl->in_msglen      );

    memcpy( tmp, ssl->in_msg + ssl->in_msglen, ssl->transform_in->maclen );

    if( ssl->minor_ver == SSL_MINOR_VERSION_0 )
    {
        if( ssl->transform_in->maclen == 16 )
             ssl_mac_md5( ssl->transform_in->mac_dec,
                          ssl->in_msg, ssl->in_msglen,
                          ssl->in_ctr, ssl->in_msgtype );
        else if( ssl->transform_in->maclen == 20 )
            ssl_mac_sha1( ssl->transform_in->mac_dec,
                          ssl->in_msg, ssl->in_msglen,
                          ssl->in_ctr, ssl->in_msgtype );
        else if( ssl->transform_in->maclen == 32 )
            ssl_mac_sha2( ssl->transform_in->mac_dec,
                          ssl->in_msg, ssl->in_msglen,
                          ssl->in_ctr, ssl->in_msgtype );
        else if( ssl->transform_in->maclen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "invalid MAC len: %d",
                                ssl->transform_in->maclen ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }
    else
    {
        /*
         * Process MAC and always update for padlen afterwards to make
         * total time independent of padlen
         */
        if( ssl->transform_in->maclen == 16 )
             md5_hmac( ssl->transform_in->mac_dec, 16,
                       ssl->in_ctr,  ssl->in_msglen + 13,
                       ssl->in_msg + ssl->in_msglen );
        else if( ssl->transform_in->maclen == 20 )
            sha1_hmac( ssl->transform_in->mac_dec, 20,
                       ssl->in_ctr,  ssl->in_msglen + 13,
                       ssl->in_msg + ssl->in_msglen );
        else if( ssl->transform_in->maclen == 32 )
            sha2_hmac( ssl->transform_in->mac_dec, 32,
                       ssl->in_ctr,  ssl->in_msglen + 13,
                       ssl->in_msg + ssl->in_msglen, 0 );
        else if( ssl->transform_in->maclen != 0 )
        {
            SSL_DEBUG_MSG( 1, ( "invalid MAC len: %d",
                                ssl->transform_in->maclen ) );
            return( POLARSSL_ERR_SSL_FEATURE_UNAVAILABLE );
        }
    }

    SSL_DEBUG_BUF( 4, "message  mac", tmp, ssl->transform_in->maclen );
    SSL_DEBUG_BUF( 4, "computed mac", ssl->in_msg + ssl->in_msglen,
                   ssl->transform_in->maclen );

    if( memcmp( tmp, ssl->in_msg + ssl->in_msglen,
                     ssl->transform_in->maclen ) != 0 )
    {
        SSL_DEBUG_MSG( 1, ( "message mac does not match" ) );
        correct = 0;
    }

    /*
     * Finally check the correct flag
     */
    if( correct == 0 )
        return( POLARSSL_ERR_SSL_INVALID_MAC );

    if( ssl->in_msglen == 0 )
    {
        ssl->nb_zero++;

        /*
         * Three or more empty messages may be a DoS attack
         * (excessive CPU consumption).
         */
        if( ssl->nb_zero > 3 )
        {
            SSL_DEBUG_MSG( 1, ( "received four consecutive empty "
                                "messages, possible DoS attack" ) );
            return( POLARSSL_ERR_SSL_INVALID_MAC );
        }
    }
    else
        ssl->nb_zero = 0;
            
    for( i = 8; i > 0; i-- )
        if( ++ssl->in_ctr[i - 1] != 0 )
            break;

    SSL_DEBUG_MSG( 2, ( "<= decrypt buf" ) );

    return( 0 );
}