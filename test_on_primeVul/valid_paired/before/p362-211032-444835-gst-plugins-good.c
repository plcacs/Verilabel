gst_matroska_demux_add_wvpk_header (GstElement * element,
    GstMatroskaTrackContext * stream, GstBuffer ** buf)
{
  GstMatroskaTrackAudioContext *audiocontext =
      (GstMatroskaTrackAudioContext *) stream;
  GstBuffer *newbuf = NULL;
  GstMapInfo map, outmap;
  guint8 *buf_data, *data;
  Wavpack4Header wvh;

  wvh.ck_id[0] = 'w';
  wvh.ck_id[1] = 'v';
  wvh.ck_id[2] = 'p';
  wvh.ck_id[3] = 'k';

  wvh.version = GST_READ_UINT16_LE (stream->codec_priv);
  wvh.track_no = 0;
  wvh.index_no = 0;
  wvh.total_samples = -1;
  wvh.block_index = audiocontext->wvpk_block_index;

  if (audiocontext->channels <= 2) {
    guint32 block_samples, tmp;
    gsize size = gst_buffer_get_size (*buf);

    gst_buffer_extract (*buf, 0, &tmp, sizeof (guint32));
    block_samples = GUINT32_FROM_LE (tmp);
    /* we need to reconstruct the header of the wavpack block */

    /* -20 because ck_size is the size of the wavpack block -8
     * and lace_size is the size of the wavpack block + 12
     * (the three guint32 of the header that already are in the buffer) */
    wvh.ck_size = size + sizeof (Wavpack4Header) - 20;

    /* block_samples, flags and crc are already in the buffer */
    newbuf = gst_buffer_new_allocate (NULL, sizeof (Wavpack4Header) - 12, NULL);

    gst_buffer_map (newbuf, &outmap, GST_MAP_WRITE);
    data = outmap.data;
    data[0] = 'w';
    data[1] = 'v';
    data[2] = 'p';
    data[3] = 'k';
    GST_WRITE_UINT32_LE (data + 4, wvh.ck_size);
    GST_WRITE_UINT16_LE (data + 8, wvh.version);
    GST_WRITE_UINT8 (data + 10, wvh.track_no);
    GST_WRITE_UINT8 (data + 11, wvh.index_no);
    GST_WRITE_UINT32_LE (data + 12, wvh.total_samples);
    GST_WRITE_UINT32_LE (data + 16, wvh.block_index);
    gst_buffer_unmap (newbuf, &outmap);

    /* Append data from buf: */
    gst_buffer_copy_into (newbuf, *buf, GST_BUFFER_COPY_TIMESTAMPS |
        GST_BUFFER_COPY_FLAGS | GST_BUFFER_COPY_MEMORY, 0, size);

    gst_buffer_unref (*buf);
    *buf = newbuf;
    audiocontext->wvpk_block_index += block_samples;
  } else {
    guint8 *outdata = NULL;
    guint outpos = 0;
    gsize buf_size, size, out_size = 0;
    guint32 block_samples, flags, crc, blocksize;

    gst_buffer_map (*buf, &map, GST_MAP_READ);
    buf_data = map.data;
    buf_size = map.size;

    if (buf_size < 4) {
      GST_ERROR_OBJECT (element, "Too small wavpack buffer");
      gst_buffer_unmap (*buf, &map);
      return GST_FLOW_ERROR;
    }

    data = buf_data;
    size = buf_size;

    block_samples = GST_READ_UINT32_LE (data);
    data += 4;
    size -= 4;

    while (size > 12) {
      flags = GST_READ_UINT32_LE (data);
      data += 4;
      size -= 4;
      crc = GST_READ_UINT32_LE (data);
      data += 4;
      size -= 4;
      blocksize = GST_READ_UINT32_LE (data);
      data += 4;
      size -= 4;

      if (blocksize == 0 || size < blocksize)
        break;

      g_assert ((newbuf == NULL) == (outdata == NULL));

      if (newbuf == NULL) {
        out_size = sizeof (Wavpack4Header) + blocksize;
        newbuf = gst_buffer_new_allocate (NULL, out_size, NULL);

        gst_buffer_copy_into (newbuf, *buf,
            GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_FLAGS, 0, -1);

        outpos = 0;
        gst_buffer_map (newbuf, &outmap, GST_MAP_WRITE);
        outdata = outmap.data;
      } else {
        gst_buffer_unmap (newbuf, &outmap);
        out_size += sizeof (Wavpack4Header) + blocksize;
        gst_buffer_set_size (newbuf, out_size);
        gst_buffer_map (newbuf, &outmap, GST_MAP_WRITE);
        outdata = outmap.data;
      }

      outdata[outpos] = 'w';
      outdata[outpos + 1] = 'v';
      outdata[outpos + 2] = 'p';
      outdata[outpos + 3] = 'k';
      outpos += 4;

      GST_WRITE_UINT32_LE (outdata + outpos,
          blocksize + sizeof (Wavpack4Header) - 8);
      GST_WRITE_UINT16_LE (outdata + outpos + 4, wvh.version);
      GST_WRITE_UINT8 (outdata + outpos + 6, wvh.track_no);
      GST_WRITE_UINT8 (outdata + outpos + 7, wvh.index_no);
      GST_WRITE_UINT32_LE (outdata + outpos + 8, wvh.total_samples);
      GST_WRITE_UINT32_LE (outdata + outpos + 12, wvh.block_index);
      GST_WRITE_UINT32_LE (outdata + outpos + 16, block_samples);
      GST_WRITE_UINT32_LE (outdata + outpos + 20, flags);
      GST_WRITE_UINT32_LE (outdata + outpos + 24, crc);
      outpos += 28;

      memmove (outdata + outpos, data, blocksize);
      outpos += blocksize;
      data += blocksize;
      size -= blocksize;
    }
    gst_buffer_unmap (*buf, &map);
    gst_buffer_unref (*buf);

    if (newbuf)
      gst_buffer_unmap (newbuf, &outmap);

    *buf = newbuf;
    audiocontext->wvpk_block_index += block_samples;
  }

  return GST_FLOW_OK;
}