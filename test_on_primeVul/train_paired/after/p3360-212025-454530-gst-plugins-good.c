gst_aac_parse_sink_setcaps (GstBaseParse * parse, GstCaps * caps)
{
  GstAacParse *aacparse;
  GstStructure *structure;
  gchar *caps_str;
  const GValue *value;

  aacparse = GST_AAC_PARSE (parse);
  structure = gst_caps_get_structure (caps, 0);
  caps_str = gst_caps_to_string (caps);

  GST_DEBUG_OBJECT (aacparse, "setcaps: %s", caps_str);
  g_free (caps_str);

  /* This is needed at least in case of RTP
   * Parses the codec_data information to get ObjectType,
   * number of channels and samplerate */
  value = gst_structure_get_value (structure, "codec_data");
  if (value) {
    GstBuffer *buf = gst_value_get_buffer (value);

    if (buf && gst_buffer_get_size (buf) >= 2) {
      GstMapInfo map;
      guint sr_idx;

      if (!gst_buffer_map (buf, &map, GST_MAP_READ))
        return FALSE;

      sr_idx = ((map.data[0] & 0x07) << 1) | ((map.data[1] & 0x80) >> 7);
      aacparse->object_type = (map.data[0] & 0xf8) >> 3;
      aacparse->sample_rate =
          gst_codec_utils_aac_get_sample_rate_from_index (sr_idx);
      aacparse->channels = (map.data[1] & 0x78) >> 3;
      if (aacparse->channels == 7)
        aacparse->channels = 8;
      else if (aacparse->channels == 11)
        aacparse->channels = 7;
      else if (aacparse->channels == 12 || aacparse->channels == 14)
        aacparse->channels = 8;
      aacparse->header_type = DSPAAC_HEADER_NONE;
      aacparse->mpegversion = 4;
      aacparse->frame_samples = (map.data[1] & 4) ? 960 : 1024;
      gst_buffer_unmap (buf, &map);

      GST_DEBUG ("codec_data: object_type=%d, sample_rate=%d, channels=%d, "
          "samples=%d", aacparse->object_type, aacparse->sample_rate,
          aacparse->channels, aacparse->frame_samples);

      /* arrange for metadata and get out of the way */
      gst_aac_parse_set_src_caps (aacparse, caps);
      if (aacparse->header_type == aacparse->output_header_type)
        gst_base_parse_set_passthrough (parse, TRUE);
    } else {
      return FALSE;
    }

    /* caps info overrides */
    gst_structure_get_int (structure, "rate", &aacparse->sample_rate);
    gst_structure_get_int (structure, "channels", &aacparse->channels);
  } else {
    const gchar *stream_format =
        gst_structure_get_string (structure, "stream-format");

    if (g_strcmp0 (stream_format, "raw") == 0) {
      GST_ERROR_OBJECT (parse, "Need codec_data for raw AAC");
      return FALSE;
    } else {
      aacparse->sample_rate = 0;
      aacparse->channels = 0;
      aacparse->header_type = DSPAAC_HEADER_NOT_PARSED;
      gst_base_parse_set_passthrough (parse, FALSE);
    }
  }
  return TRUE;
}