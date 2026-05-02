static int somfy_iohc_decode(r_device *decoder, bitbuffer_t *bitbuffer)
{
    uint8_t const preamble_pattern[] = {0x57, 0xfd, 0x99};

    uint8_t b[19 + 15]; // 19 byte + up 15 byte payload

    if (bitbuffer->num_rows != 1)
        return DECODE_ABORT_EARLY;

    int offset = bitbuffer_search(bitbuffer, 0, 0, preamble_pattern, 24) + 24;
    if (offset >= bitbuffer->bits_per_row[0] - 19 * 10)
        return DECODE_ABORT_EARLY;

    int num_bits = bitbuffer->bits_per_row[0] - offset;

    int len = extract_bytes_uart(bitbuffer->bb[0], offset, num_bits, b);
    if (len < 19)
        return DECODE_ABORT_LENGTH;

    if ((b[0] & 0xf0) != 0xf0)
        return DECODE_ABORT_EARLY;

    int msg_len = b[0] & 0xf; // should be 6 or 8
    if (len < 19 + msg_len)
        return DECODE_ABORT_LENGTH;

    // calculate and verify checksum
    if (crc16lsb(b, len, 0x8408, 0x0000) != 0) // unreflected poly 0x1021
        return DECODE_FAIL_MIC;
    bitrow_printf(b, len * 8, "%s: offset %d, num_bits %d, len %d, msg_len %d\n", __func__, offset, num_bits, len, msg_len);

    int msg_type = (b[0]);
    int dst_id   = ((unsigned)b[4] << 24) | (b[3] << 16) | (b[2] << 8) | (b[1]); // assume Little-Endian
    int src_id   = ((unsigned)b[8] << 24) | (b[7] << 16) | (b[6] << 8) | (b[5]); // assume Little-Endian
    int counter  = (b[len - 10] << 8) | (b[len - 9]);
    char msg_str[15 * 2 + 1];
    bitrow_snprint(&b[9], msg_len * 8, msg_str, 15 * 2 + 1);
    char mac_str[13];
    bitrow_snprint(&b[len - 8], 6 * 8, mac_str, 13);

    /* clang-format off */
    data_t *data = data_make(
            "model",            "",                 DATA_STRING, "Somfy-IOHC",
            "id",               "",                 DATA_FORMAT, "%08x", DATA_INT, src_id,
            "dst_id",           "Dest ID",          DATA_FORMAT, "%08x", DATA_INT, dst_id,
            "msg_type",         "Msg type",         DATA_FORMAT, "%02x", DATA_INT, msg_type,
            "msg",              "Message",          DATA_STRING, msg_str,
            "counter",          "Counter",          DATA_INT,    counter,
            "mac",              "MAC",              DATA_STRING, mac_str,
            "mic",              "Integrity",        DATA_STRING, "CRC",
            NULL);
    /* clang-format on */

    decoder_output_data(decoder, data);
    return 1;
}