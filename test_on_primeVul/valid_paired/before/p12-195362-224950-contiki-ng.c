snmp_ber_decode_string_len_buffer(unsigned char *buf, uint32_t *buff_len, const char **str, uint32_t *length)
{
  uint8_t type, i, length_bytes;

  buf = snmp_ber_decode_type(buf, buff_len, &type);

  if(buf == NULL || type != BER_DATA_TYPE_OCTET_STRING) {
    /*
     * Sanity check
     * Invalid type in buffer
     */
    return NULL;
  }

  if((*buf & 0x80) == 0) {
    *length = (uint32_t)*buf++;
    (*buff_len)--;
  } else {

    length_bytes = (uint8_t)(*buf++ & 0x7F);
    (*buff_len)--;
    if(length_bytes > 4) {
      /*
       * Sanity check
       * It will not fit in the uint32_t
       */
      return NULL;
    }

    *length = (uint32_t)*buf++;
    (*buff_len)--;
    for(i = 1; i < length_bytes; ++i) {
      *length <<= 8;
      *length |= *buf++;
      (*buff_len)--;
    }
  }

  *str = (const char *)buf;
  *buff_len -= *length;

  return buf + *length;
}