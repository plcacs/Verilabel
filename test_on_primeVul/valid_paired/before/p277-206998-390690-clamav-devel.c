static int xar_hash_check(int hash, const void * result, const void * expected)
{
    int len;

    if (!result || !expected)
        return 1;
    switch (hash) {
    case XAR_CKSUM_SHA1:
        len = SHA1_HASH_SIZE;
        break;
    case XAR_CKSUM_MD5:
        len = CLI_HASH_MD5;
        break;
    case XAR_CKSUM_OTHER:
    case XAR_CKSUM_NONE:
    default:
        return 1;
    }
    return memcmp(result, expected, len);
}