dnsc_load_local_data(struct dnsc_env* dnscenv, struct config_file *cfg)
{
    size_t i, j;
	// Insert 'local-zone: "2.dnscrypt-cert.example.com" deny'
    if(!cfg_str2list_insert(&cfg->local_zones,
                            strdup(dnscenv->provider_name),
                            strdup("deny"))) {
        log_err("Could not load dnscrypt local-zone: %s deny",
                dnscenv->provider_name);
        return -1;
    }

    // Add local data entry of type:
    // 2.dnscrypt-cert.example.com 86400 IN TXT "DNSC......"
    for(i=0; i<dnscenv->signed_certs_count; i++) {
        const char *ttl_class_type = " 86400 IN TXT \"";
        int rotated_cert = 0;
	uint32_t serial;
	uint16_t rrlen;
	char* rr;
        struct SignedCert *cert = dnscenv->signed_certs + i;
		// Check if the certificate is being rotated and should not be published
        for(j=0; j<dnscenv->rotated_certs_count; j++){
            if(cert == dnscenv->rotated_certs[j]) {
                rotated_cert = 1;
                break;
            }
        }
		memcpy(&serial, cert->serial, sizeof serial);
		serial = htonl(serial);
        if(rotated_cert) {
            verbose(VERB_OPS,
                "DNSCrypt: not adding cert with serial #%"
                PRIu32
                " to local-data as it is rotated",
                serial
            );
            continue;
        }
	if((unsigned)strlen(dnscenv->provider_name) >= (unsigned)0xffff0000) {
		/* guard against integer overflow in rrlen calculation */
		verbose(VERB_OPS, "cert #%" PRIu32 " is too long", serial);
		continue
	}
        rrlen = strlen(dnscenv->provider_name) +
                         strlen(ttl_class_type) +
                         4 * sizeof(struct SignedCert) + // worst case scenario
                         1 + // trailing double quote
                         1;
        rr = malloc(rrlen);
        if(!rr) {
            log_err("Could not allocate memory");
            return -2;
        }
        snprintf(rr, rrlen - 1, "%s 86400 IN TXT \"", dnscenv->provider_name);
        for(j=0; j<sizeof(struct SignedCert); j++) {
			int c = (int)*((const uint8_t *) cert + j);
            if (isprint(c) && c != '"' && c != '\\') {
                snprintf(rr + strlen(rr), rrlen - 1 - strlen(rr), "%c", c);
            } else {
                snprintf(rr + strlen(rr), rrlen - 1 - strlen(rr), "\\%03d", c);
            }
        }
        verbose(VERB_OPS,
			"DNSCrypt: adding cert with serial #%"
			PRIu32
			" to local-data to config: %s",
			serial, rr
		);
        snprintf(rr + strlen(rr), rrlen - 1 - strlen(rr), "\"");
        cfg_strlist_insert(&cfg->local_data, strdup(rr));
        free(rr);
    }
    return dnscenv->signed_certs_count;
}