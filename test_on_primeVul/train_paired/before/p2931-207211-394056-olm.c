size_t olm_pk_decrypt(
    OlmPkDecryption * decryption,
    void const * ephemeral_key, size_t ephemeral_key_length,
    void const * mac, size_t mac_length,
    void * ciphertext, size_t ciphertext_length,
    void * plaintext, size_t max_plaintext_length
) {
    if (max_plaintext_length
            < olm_pk_max_plaintext_length(decryption, ciphertext_length)) {
        decryption->last_error =
            OlmErrorCode::OLM_OUTPUT_BUFFER_TOO_SMALL;
        return std::size_t(-1);
    }

    struct _olm_curve25519_public_key ephemeral;
    olm::decode_base64(
        (const uint8_t*)ephemeral_key, ephemeral_key_length,
        (uint8_t *)ephemeral.public_key
    );
    olm::SharedKey secret;
    _olm_crypto_curve25519_shared_secret(&decryption->key_pair, &ephemeral, secret);
    uint8_t raw_mac[MAC_LENGTH];
    olm::decode_base64((const uint8_t*)mac, olm::encode_base64_length(MAC_LENGTH), raw_mac);
    size_t raw_ciphertext_length = olm::decode_base64_length(ciphertext_length);
    olm::decode_base64((const uint8_t *)ciphertext, ciphertext_length, (uint8_t *)ciphertext);
    size_t result = _olm_cipher_aes_sha_256_ops.decrypt(
        olm_pk_cipher,
        secret, sizeof(secret),
        (uint8_t *) raw_mac, MAC_LENGTH,
        (const uint8_t *) ciphertext, raw_ciphertext_length,
        (uint8_t *) plaintext, max_plaintext_length
    );
    if (result == std::size_t(-1)) {
        // we already checked the buffer sizes, so the only error that decrypt
        // will return is if the MAC is incorrect
        decryption->last_error =
            OlmErrorCode::OLM_BAD_MESSAGE_MAC;
        return std::size_t(-1);
    } else {
        return result;
    }
}