gst_mpegts_section_new (guint16 pid, guint8 * data, gsize data_size)
{
  GstMpegtsSection *res = NULL;
  guint8 tmp;
  guint8 table_id;
  guint16 section_length = 0;

  /* The smallest section ever is 3 bytes */
  if (G_UNLIKELY (data_size < 3))
    goto short_packet;

  /* Check for length */
  section_length = GST_READ_UINT16_BE (data + 1) & 0x0FFF;
  if (G_UNLIKELY (data_size < section_length + 3))
    goto short_packet;

  GST_LOG ("data_size:%" G_GSIZE_FORMAT " section_length:%u",
      data_size, section_length);

  /* Table id is in first byte */
  table_id = *data;

  res = _gst_mpegts_section_init (pid, table_id);

  res->data = data;
  /* table_id (already parsed)       : 8  bit */
  data++;
  /* section_syntax_indicator        : 1  bit
   * other_fields (reserved)         : 3  bit*/
  res->short_section = (*data & 0x80) == 0x00;
  /* section_length (already parsed) : 12 bit */
  res->section_length = section_length + 3;
  if (!res->short_section) {
    /* A long packet needs to be at least 11 bytes long
     * _ 3 for the bytes above
     * _ 5 for the bytes below
     * _ 4 for the CRC */
    if (G_UNLIKELY (data_size < 11))
      goto bad_long_packet;

    /* CRC is after section_length (-4 for the size of the CRC) */
    res->crc = GST_READ_UINT32_BE (res->data + res->section_length - 4);
    /* Skip to after section_length */
    data += 2;
    /* subtable extension            : 16 bit */
    res->subtable_extension = GST_READ_UINT16_BE (data);
    data += 2;
    /* reserved                      : 2  bit
     * version_number                : 5  bit
     * current_next_indicator        : 1  bit */
    tmp = *data++;
    res->version_number = tmp >> 1 & 0x1f;
    res->current_next_indicator = tmp & 0x01;
    /* section_number                : 8  bit */
    res->section_number = *data++;
    /* last_section_number                : 8  bit */
    res->last_section_number = *data;
  }

  return res;

short_packet:
  {
    GST_WARNING
        ("PID 0x%04x section extends past provided data (got:%" G_GSIZE_FORMAT
        ", need:%d)", pid, data_size, section_length + 3);
    g_free (data);
    return NULL;
  }
bad_long_packet:
  {
    GST_WARNING ("PID 0x%04x long section is too short (%" G_GSIZE_FORMAT
        " bytes, need at least 11)", pid, data_size);
    gst_mpegts_section_unref (res);
    return NULL;
  }
}