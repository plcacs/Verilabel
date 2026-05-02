static int tls1_check_sig_alg(SSL *s, X509 *x, int default_nid)
{
    int sig_nid, use_pc_sigalgs = 0;
    size_t i;
    const SIGALG_LOOKUP *sigalg;
    size_t sigalgslen;
    if (default_nid == -1)
        return 1;
    sig_nid = X509_get_signature_nid(x);
    if (default_nid)
        return sig_nid == default_nid ? 1 : 0;

    if (SSL_IS_TLS13(s) && s->s3.tmp.peer_cert_sigalgs != NULL) {
        /*
         * If we're in TLSv1.3 then we only get here if we're checking the
         * chain. If the peer has specified peer_cert_sigalgs then we use them
         * otherwise we default to normal sigalgs.
         */
        sigalgslen = s->s3.tmp.peer_cert_sigalgslen;
        use_pc_sigalgs = 1;
    } else {
        sigalgslen = s->shared_sigalgslen;
    }
    for (i = 0; i < sigalgslen; i++) {
        sigalg = use_pc_sigalgs
                 ? tls1_lookup_sigalg(s->s3.tmp.peer_cert_sigalgs[i])
                 : s->shared_sigalgs[i];
        if (sig_nid == sigalg->sigandhash)
            return 1;
    }
    return 0;
}