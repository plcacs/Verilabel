otError Commissioner::GeneratePskc(const char *              aPassPhrase,
                                   const char *              aNetworkName,
                                   const Mac::ExtendedPanId &aExtPanId,
                                   Pskc &                    aPskc)
{
    otError    error        = OT_ERROR_NONE;
    const char saltPrefix[] = "Thread";
    uint8_t    salt[OT_PBKDF2_SALT_MAX_LEN];
    uint16_t   saltLen = 0;
    uint16_t   passphraseLen;
    uint8_t    networkNameLen;

    passphraseLen  = static_cast<uint16_t>(strnlen(aPassPhrase, OT_COMMISSIONING_PASSPHRASE_MAX_SIZE + 1));
    networkNameLen = static_cast<uint8_t>(strnlen(aNetworkName, OT_NETWORK_NAME_MAX_SIZE + 1));

    VerifyOrExit((passphraseLen >= OT_COMMISSIONING_PASSPHRASE_MIN_SIZE) &&
                     (passphraseLen <= OT_COMMISSIONING_PASSPHRASE_MAX_SIZE) &&
                     (networkNameLen <= OT_NETWORK_NAME_MAX_SIZE),
                 error = OT_ERROR_INVALID_ARGS);

    memset(salt, 0, sizeof(salt));
    memcpy(salt, saltPrefix, sizeof(saltPrefix) - 1);
    saltLen += static_cast<uint16_t>(sizeof(saltPrefix) - 1);

    memcpy(salt + saltLen, aExtPanId.m8, sizeof(aExtPanId));
    saltLen += OT_EXT_PAN_ID_SIZE;

    memcpy(salt + saltLen, aNetworkName, networkNameLen);
    saltLen += networkNameLen;

    otPbkdf2Cmac(reinterpret_cast<const uint8_t *>(aPassPhrase), passphraseLen, reinterpret_cast<const uint8_t *>(salt),
                 saltLen, 16384, OT_PSKC_MAX_SIZE, aPskc.m8);

exit:
    return error;
}