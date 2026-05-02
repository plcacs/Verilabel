snmp_ber_decode_string_len_buffer(snmp_packet_t *snmp_packet, const char **str, uint32_t *length)
{
  uint8_t type, i, length_bytes;

  if(!snmp_ber_decode_type(snmp_packet, &type)) {
    return 0;
  }

  if(type != BER_DATA_TYPE_OCTET_STRING) {
    /*
     * Sanity check
     * Invalid type in buffer
     */
    return 0;
  }

  if((*snmp_packet->in & 0x80) == 0) {

    if(snmp_packet->used == 0) {
      return 0;
    }

    *length = (uint32_t)*snmp_packet->in++;
    snmp_packet->used--;
  } else {

    if(snmp_packet->used == 0) {
      return 0;
    }

    length_bytes = (uint8_t)(*snmp_packet->in++ & 0x7F);
    snmp_packet->used--;

    if(length_bytes > 4) {
      /*
       * Sanity check
       * It will not fit in the uint32_t
       */
      return 0;
    }

    if(snmp_packet->used == 0) {
      return 0;
    }

    *length = (uint32_t)*snmp_packet->in++;
    snmp_packet->used--;

    for(i = 1; i < length_bytes; ++i) {
      *length <<= 8;

      if(snmp_packet->used == 0) {
        return 0;
      }

      *length |= *snmp_packet->in++;
      snmp_packet->used--;
    }
  }

  *str = (const char *)snmp_packet->in;

  if(snmp_packet->used == 0 || snmp_packet->used - *length <= 0) {
    return 0;
  }

  snmp_packet->used -= *length;
  snmp_packet->in += *length;

  return 1;
}