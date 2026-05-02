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

    if (size < 4) {
      GST_ERROR_OBJECT (element, "Too small wavpack buffer");
      gst_buffer_unmap (*buf, &map);
      return GST_FLOW_ERROR;
    }

    gst_buffer_extract (*buf, 0, &tmp, sizeof (guint32));
    block_samples = GUINT32_FROM_LE (tmp);
    /* we need to reconstruct the header of the wavpack block */

    /* -20 because ck_size is the size of the wavpack block -8
     * and lace_size is the size of the wavpack block + 12
     * (the three guint32 of the header that already are in the buffer) */
    wvh.ck_size = size + WAVPACK4_HEADER_SIZE - 20;

    /* block_samples, flags and crc are already in the buffer */
    newbuf = gst_buffer_new_allocate (NULL, WAVPACK4_HEADER_SIZE - 12, NULL);

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
    gsize buf_size, size;
    guint32 block_samples, flags, crc, blocksize;
    GstAdapter *adapter;

    adapter = gst_adapter_new ();

    gst_buffer_map (*buf, &map, GST_MAP_READ);
    buf_data = map.data;
    buf_size = map.size;

    if (buf_size < 4) {
      GST_ERROR_OBJECT (element, "Too small wavpack buffer");
      gst_buffer_unmap (*buf, &map);
      g_object_unref (adapter);
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

      if (blocksize == 0 || size < blocksize) {
        GST_ERROR_OBJECT (element, "Too small wavpack buffer");
        gst_buffer_unmap (*buf, &map);
        g_object_unref (adapter);
        return GST_FLOW_ERROR;
      }

      g_assert (newbuf == NULL);

      newbuf =
          gst_buffer_new_allocate (NULL, WAVPACK4_HEADER_SIZE + blocksize,
          NULL);
      gst_buffer_map (newbuf, &outmap, GST_MAP_WRITE);
      outdata = outmap.data;

      outdata[0] = 'w';
      outdata[1] = 'v';
      outdata[2] = 'p';
      outdata[3] = 'k';
      outdata += 4;

      GST_WRITE_UINT32_LE (outdata, blocksize + WAVPACK4_HEADER_SIZE - 8);
      GST_WRITE_UINT16_LE (outdata + 4, wvh.version);
      GST_WRITE_UINT8 (outdata + 6, wvh.track_no);
      GST_WRITE_UINT8 (outdata + 7, wvh.index_no);
      GST_WRITE_UINT32_LE (outdata + 8, wvh.total_samples);
      GST_WRITE_UINT32_LE (outdata + 12, wvh.block_index);
      GST_WRITE_UINT32_LE (outdata + 16, block_samples);
      GST_WRITE_UINT32_LE (outdata + 20, flags);
      GST_WRITE_UINT32_LE (outdata + 24, crc);
      outdata += 28;

      memcpy (outdata, data, blocksize);

      gst_buffer_unmap (newbuf, &outmap);
      gst_adapter_push (adapter, newbuf);
      newbuf = NULL;

      data += blocksize;
      size -= blocksize;
    }
    gst_buffer_unmap (*buf, &map);

    newbuf = gst_adapter_take_buffer (adapter, gst_adapter_available (adapter));
    g_object_unref (adapter);

    gst_buffer_copy_into (newbuf, *buf,
        GST_BUFFER_COPY_TIMESTAMPS | GST_BUFFER_COPY_FLAGS, 0, -1);
    gst_buffer_unref (*buf);
    *buf = newbuf;

    audiocontext->wvpk_block_index += block_samples;
  }

  return GST_FLOW_OK;
}