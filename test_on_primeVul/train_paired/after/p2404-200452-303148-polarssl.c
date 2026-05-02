int x509_get_name( unsigned char **p, const unsigned char *end,
                   x509_name *cur )
{
    int ret;
    size_t set_len;
    const unsigned char *end_set;

    /*
     * parse first SET, restricted to 1 element
     */
    if( ( ret = asn1_get_tag( p, end, &set_len,
            ASN1_CONSTRUCTED | ASN1_SET ) ) != 0 )
        return( POLARSSL_ERR_X509_INVALID_NAME + ret );

    end_set  = *p + set_len;

    if( ( ret = x509_get_attr_type_value( p, end_set, cur ) ) != 0 )
        return( ret );

    if( *p != end_set )
        return( POLARSSL_ERR_X509_FEATURE_UNAVAILABLE );

    /*
     * recurse until end of SEQUENCE is reached
     */
    if( *p == end )
        return( 0 );

    cur->next = (x509_name *) polarssl_malloc( sizeof( x509_name ) );

    if( cur->next == NULL )
        return( POLARSSL_ERR_X509_MALLOC_FAILED );

    memset( cur->next, 0, sizeof( x509_name ) );

    return( x509_get_name( p, end, cur->next ) );
}