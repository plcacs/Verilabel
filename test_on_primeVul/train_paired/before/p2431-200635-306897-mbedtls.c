int ssl_set_hostname( ssl_context *ssl, const char *hostname )
{
    if( hostname == NULL )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    ssl->hostname_len = strlen( hostname );

    if( ssl->hostname_len + 1 == 0 )
        return( POLARSSL_ERR_SSL_BAD_INPUT_DATA );

    ssl->hostname = polarssl_malloc( ssl->hostname_len + 1 );

    if( ssl->hostname == NULL )
        return( POLARSSL_ERR_SSL_MALLOC_FAILED );

    memcpy( ssl->hostname, (const unsigned char *) hostname,
            ssl->hostname_len );

    ssl->hostname[ssl->hostname_len] = '\0';

    return( 0 );
}