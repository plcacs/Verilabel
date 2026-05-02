static void aes_ctr_cleanup(struct ssh_cipher_struct *cipher){
    explicit_bzero(cipher->aes_key, sizeof(*cipher->aes_key));
    SAFE_FREE(cipher->aes_key);
}