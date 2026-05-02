SWTPM_NVRAM_CheckHeader(unsigned char *data, uint32_t length,
                        uint32_t *dataoffset, uint16_t *hdrflags,
                        uint8_t *hdrversion, bool quiet)
{
    blobheader *bh = (blobheader *)data;
    uint16_t hdrsize;

    if (length < sizeof(bh)) {
        if (!quiet)
            logprintf(STDERR_FILENO,
                      "not enough bytes for header: %u\n", length);
        return TPM_BAD_PARAMETER;
    }

    if (ntohl(bh->totlen) != length) {
        if (!quiet)
            logprintf(STDERR_FILENO,
                      "broken header: bh->totlen %u != %u\n",
                      htonl(bh->totlen), length);
        return TPM_BAD_PARAMETER;
    }

    if (bh->min_version > BLOB_HEADER_VERSION) {
        if (!quiet)
            logprintf(STDERR_FILENO,
                      "Minimum required version for the blob is %d, we "
                      "only support version %d\n", bh->min_version,
                      BLOB_HEADER_VERSION);
        return TPM_BAD_VERSION;
    }

    hdrsize = ntohs(bh->hdrsize);
    if (hdrsize != sizeof(blobheader)) {
        logprintf(STDERR_FILENO,
                  "bad header size: %u != %zu\n",
                  hdrsize, sizeof(blobheader));
        return TPM_BAD_DATASIZE;
    }

    *hdrversion = bh->version;
    *dataoffset = hdrsize;
    *hdrflags = ntohs(bh->flags);

    return TPM_SUCCESS;
}