cdata_read (cdata_t *cd, guint8 res_data, gint comptype,
            GDataInputStream *in, GCancellable *cancellable, GError **error)

{
    gboolean success = FALSE;
    int ret, zret = Z_OK;
    gint compression = comptype & GCAB_COMPRESSION_MASK;
    guint8 *buf = compression == GCAB_COMPRESSION_NONE ? cd->out : cd->in;
    guint32 datacsum;
    guint32 checksum_tmp;
    guint8 sizecsum[4];
    guint16 nbytes_le;

    if (compression > GCAB_COMPRESSION_MSZIP &&
        compression != GCAB_COMPRESSION_LZX) {
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_NOT_SUPPORTED,
                     "unsupported compression method %d", compression);
        return FALSE;
    }

    R4 (cd->checksum);
    R2 (cd->ncbytes);
    R2 (cd->nubytes);
    RN (cd->reserved, res_data);
    RN (buf, cd->ncbytes);

    datacsum = compute_checksum(buf, cd->ncbytes, 0);
    nbytes_le = GUINT16_TO_LE (cd->ncbytes);
    memcpy (&sizecsum[0], &nbytes_le, 2);
    nbytes_le = GUINT16_TO_LE (cd->nubytes);
    memcpy (&sizecsum[2], &nbytes_le, 2);
    checksum_tmp = compute_checksum (sizecsum, sizeof(sizecsum), datacsum);
    if (cd->checksum != checksum_tmp) {
        if (_enforce_checksum ()) {
            g_set_error_literal (error, GCAB_ERROR, GCAB_ERROR_INVALID_DATA,
                                 "incorrect checksum detected");
            return FALSE;
        }
        if (g_getenv ("GCAB_DEBUG"))
            g_debug ("CDATA checksum 0x%08x", (guint) checksum_tmp);
    }

    if (g_getenv ("GCAB_DEBUG")) {
        g_debug ("CDATA");
        P4 (cd, checksum);
        P2 (cd, ncbytes);
        P2 (cd, nubytes);
        if (res_data)
            PN (cd, reserved, res_data);
        PND (cd, buf, 64);
    }

    if (compression == GCAB_COMPRESSION_LZX) {
        if (cd->fdi.alloc == NULL) {
            cd->fdi.alloc = g_malloc;
            cd->fdi.free = g_free;
            cd->decomp.fdi = &cd->fdi;
            cd->decomp.inbuf = cd->in;
            cd->decomp.outbuf = cd->out;
            cd->decomp.comptype = compression;

            ret = LZXfdi_init((comptype >> 8) & 0x1f, &cd->decomp);
            if (ret < 0)
                goto end;
        }

        ret = LZXfdi_decomp (cd->ncbytes, cd->nubytes, &cd->decomp);
        if (ret < 0)
            goto end;
    }

    if (compression == GCAB_COMPRESSION_MSZIP) {
        if (cd->in[0] != 'C' || cd->in[1] != 'K')
            goto end;

        cd->decomp.comptype = compression;
        z_stream *z = &cd->z;

        z->avail_in = cd->ncbytes - 2;
        z->next_in = cd->in + 2;
        z->avail_out = cd->nubytes;
        z->next_out = cd->out;
        z->total_out = 0;

        if (!z->opaque) {
            z->zalloc = zalloc;
            z->zfree = zfree;
            z->opaque = cd;

            zret = inflateInit2 (z, -MAX_WBITS);
            if (zret != Z_OK)
                goto end;
        }

        while (1) {
            zret = inflate (z, Z_BLOCK);
            if (zret == Z_STREAM_END)
                break;
            if (zret != Z_OK)
                goto end;
        }

        g_warn_if_fail (z->avail_in == 0);
        g_warn_if_fail (z->avail_out == 0);
        if (z->avail_in != 0 || z->avail_out != 0)
            goto end;

        zret = inflateReset (z);
        if (zret != Z_OK)
            goto end;

        zret = inflateSetDictionary (z, cd->out, cd->nubytes);
        if (zret != Z_OK)
            goto end;
    }

    success = TRUE;

end:
    if (zret != Z_OK)
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "zlib failed: %s", zError (zret));

    if (error != NULL && *error == NULL && !success)
        g_set_error (error, GCAB_ERROR, GCAB_ERROR_FAILED,
                     "Invalid cabinet chunk");

    return success;
}