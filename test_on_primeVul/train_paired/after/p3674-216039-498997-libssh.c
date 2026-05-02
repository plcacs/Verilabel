static void aes_ctr_cleanup(struct ssh_cipher_struct *cipher){
    if (cipher != NULL) {
        if (cipher->aes_key != NULL) {
            explicit_bzero(cipher->aes_key, sizeof(*cipher->aes_key));
        }
        SAFE_FREE(cipher->aes_key);
    }
}