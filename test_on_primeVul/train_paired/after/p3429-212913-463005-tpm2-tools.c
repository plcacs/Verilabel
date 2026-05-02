static tool_rc key_import(ESYS_CONTEXT *ectx, TPM2B_PUBLIC *parent_pub,
        TPM2B_SENSITIVE *privkey, TPM2B_PUBLIC *pubkey,
        TPM2B_ENCRYPTED_SECRET *encrypted_seed,
        TPM2B_PRIVATE **imported_private) {

    TPMI_ALG_HASH name_alg = pubkey->publicArea.nameAlg;

    TPM2B_DIGEST *seed = &privkey->sensitiveArea.seedValue;

    /*
     * Create the protection encryption key that gets encrypted with the parents public key.
     */
    TPM2B_DATA enc_sensitive_key = {
        .size = parent_pub->publicArea.parameters.rsaDetail.symmetric.keyBits.sym / 8
    };

    if(enc_sensitive_key.size < 16) {
        LOG_ERR("Calculated wrapping keysize is less than 16 bytes, got: %u", enc_sensitive_key.size);
        return tool_rc_general_error;
    }

    int ossl_rc = RAND_bytes(enc_sensitive_key.buffer, enc_sensitive_key.size);
    if (ossl_rc != 1) {
        LOG_ERR("RAND_bytes failed: %s", ERR_error_string(ERR_get_error(), NULL));
        return tool_rc_general_error;
    }

    /*
     * Calculate the object name.
     */
    TPM2B_NAME pubname = TPM2B_TYPE_INIT(TPM2B_NAME, name);
    bool res = tpm2_identity_create_name(pubkey, &pubname);
    if (!res) {
        return false;
    }

    TPM2B_MAX_BUFFER hmac_key;
    TPM2B_MAX_BUFFER enc_key;
    tpm2_identity_util_calc_outer_integrity_hmac_key_and_dupsensitive_enc_key(
            parent_pub, &pubname, seed, &hmac_key, &enc_key);

    TPM2B_MAX_BUFFER encrypted_inner_integrity = TPM2B_EMPTY_INIT;
    tpm2_identity_util_calculate_inner_integrity(name_alg, privkey, &pubname,
            &enc_sensitive_key,
            &parent_pub->publicArea.parameters.rsaDetail.symmetric,
            &encrypted_inner_integrity);

    TPM2B_DIGEST outer_hmac = TPM2B_EMPTY_INIT;
    TPM2B_MAX_BUFFER encrypted_duplicate_sensitive = TPM2B_EMPTY_INIT;
    tpm2_identity_util_calculate_outer_integrity(parent_pub->publicArea.nameAlg,
            &pubname, &encrypted_inner_integrity, &hmac_key, &enc_key,
            &parent_pub->publicArea.parameters.rsaDetail.symmetric,
            &encrypted_duplicate_sensitive, &outer_hmac);

    TPM2B_PRIVATE private = TPM2B_EMPTY_INIT;
    res = create_import_key_private_data(&private, parent_pub->publicArea.nameAlg,
            &encrypted_duplicate_sensitive, &outer_hmac);
    if (!res) {
        return tool_rc_general_error;
    }

    TPMT_SYM_DEF_OBJECT *sym_alg =
            &parent_pub->publicArea.parameters.rsaDetail.symmetric;

    if (!ctx.cp_hash_path) {
        return tpm2_import(ectx, &ctx.parent.object, &enc_sensitive_key, pubkey,
            &private, encrypted_seed, sym_alg, imported_private, NULL);
    }

    TPM2B_DIGEST cp_hash = { .size = 0 };
    tool_rc rc = tpm2_import(ectx, &ctx.parent.object, &enc_sensitive_key, pubkey,
            &private, encrypted_seed, sym_alg, imported_private, &cp_hash);
    if (rc != tool_rc_success) {
        return rc;
    }

    bool result = files_save_digest(&cp_hash, ctx.cp_hash_path);
    if (!result) {
        rc = tool_rc_general_error;
    }
    return rc;
}