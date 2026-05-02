static int parse_part_sign_sha256(sockent_t *se, /* {{{ */
                                  void **ret_buffer, size_t *ret_buffer_len,
                                  int flags) {
  static c_complain_t complain_no_users = C_COMPLAIN_INIT_STATIC;

  char *buffer;
  size_t buffer_len;
  size_t buffer_offset;

  size_t username_len;
  char *secret;

  part_signature_sha256_t pss;
  uint16_t pss_head_length;
  char hash[sizeof(pss.hash)];

  gcry_md_hd_t hd;
  gcry_error_t err;
  unsigned char *hash_ptr;

  buffer = *ret_buffer;
  buffer_len = *ret_buffer_len;
  buffer_offset = 0;

  if (se->data.server.userdb == NULL) {
    c_complain(
        LOG_NOTICE, &complain_no_users,
        "network plugin: Received signed network packet but can't verify it "
        "because no user DB has been configured. Will accept it.");
    return (0);
  }

  /* Check if the buffer has enough data for this structure. */
  if (buffer_len <= PART_SIGNATURE_SHA256_SIZE)
    return (-ENOMEM);

  /* Read type and length header */
  BUFFER_READ(&pss.head.type, sizeof(pss.head.type));
  BUFFER_READ(&pss.head.length, sizeof(pss.head.length));
  pss_head_length = ntohs(pss.head.length);

  /* Check if the `pss_head_length' is within bounds. */
  if ((pss_head_length <= PART_SIGNATURE_SHA256_SIZE) ||
      (pss_head_length > buffer_len)) {
    ERROR("network plugin: HMAC-SHA-256 with invalid length received.");
    return (-1);
  }

  /* Copy the hash. */
  BUFFER_READ(pss.hash, sizeof(pss.hash));

  /* Calculate username length (without null byte) and allocate memory */
  username_len = pss_head_length - PART_SIGNATURE_SHA256_SIZE;
  pss.username = malloc(username_len + 1);
  if (pss.username == NULL)
    return (-ENOMEM);

  /* Read the username */
  BUFFER_READ(pss.username, username_len);
  pss.username[username_len] = 0;

  assert(buffer_offset == pss_head_length);

  /* Query the password */
  secret = fbh_get(se->data.server.userdb, pss.username);
  if (secret == NULL) {
    ERROR("network plugin: Unknown user: %s", pss.username);
    sfree(pss.username);
    return (-ENOENT);
  }

  /* Create a hash device and check the HMAC */
  hd = NULL;
  err = gcry_md_open(&hd, GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC);
  if (err != 0) {
    ERROR("network plugin: Creating HMAC-SHA-256 object failed: %s",
          gcry_strerror(err));
    sfree(secret);
    sfree(pss.username);
    return (-1);
  }

  err = gcry_md_setkey(hd, secret, strlen(secret));
  if (err != 0) {
    ERROR("network plugin: gcry_md_setkey failed: %s", gcry_strerror(err));
    gcry_md_close(hd);
    sfree(secret);
    sfree(pss.username);
    return (-1);
  }

  gcry_md_write(hd, buffer + PART_SIGNATURE_SHA256_SIZE,
                buffer_len - PART_SIGNATURE_SHA256_SIZE);
  hash_ptr = gcry_md_read(hd, GCRY_MD_SHA256);
  if (hash_ptr == NULL) {
    ERROR("network plugin: gcry_md_read failed.");
    gcry_md_close(hd);
    sfree(secret);
    sfree(pss.username);
    return (-1);
  }
  memcpy(hash, hash_ptr, sizeof(hash));

  /* Clean up */
  gcry_md_close(hd);
  hd = NULL;

  if (memcmp(pss.hash, hash, sizeof(pss.hash)) != 0) {
    WARNING("network plugin: Verifying HMAC-SHA-256 signature failed: "
            "Hash mismatch. Username: %s",
            pss.username);
  } else {
    parse_packet(se, buffer + buffer_offset, buffer_len - buffer_offset,
                 flags | PP_SIGNED, pss.username);
  }

  sfree(secret);
  sfree(pss.username);

  *ret_buffer = buffer + buffer_len;
  *ret_buffer_len = 0;

  return (0);
} /* }}} int parse_part_sign_sha256 */