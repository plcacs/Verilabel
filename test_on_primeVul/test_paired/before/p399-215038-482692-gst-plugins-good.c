gst_flxdec_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstCaps *caps;
  guint avail;
  GstFlowReturn res = GST_FLOW_OK;

  GstFlxDec *flxdec;
  FlxHeader *flxh;

  g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);
  flxdec = (GstFlxDec *) parent;
  g_return_val_if_fail (flxdec != NULL, GST_FLOW_ERROR);

  gst_adapter_push (flxdec->adapter, buf);
  avail = gst_adapter_available (flxdec->adapter);

  if (flxdec->state == GST_FLXDEC_READ_HEADER) {
    if (avail >= FlxHeaderSize) {
      const guint8 *data = gst_adapter_map (flxdec->adapter, FlxHeaderSize);
      GstCaps *templ;

      memcpy ((gchar *) & flxdec->hdr, data, FlxHeaderSize);
      FLX_HDR_FIX_ENDIANNESS (&(flxdec->hdr));
      gst_adapter_unmap (flxdec->adapter);
      gst_adapter_flush (flxdec->adapter, FlxHeaderSize);

      flxh = &flxdec->hdr;

      /* check header */
      if (flxh->type != FLX_MAGICHDR_FLI &&
          flxh->type != FLX_MAGICHDR_FLC && flxh->type != FLX_MAGICHDR_FLX)
        goto wrong_type;

      GST_LOG ("size      :  %d", flxh->size);
      GST_LOG ("frames    :  %d", flxh->frames);
      GST_LOG ("width     :  %d", flxh->width);
      GST_LOG ("height    :  %d", flxh->height);
      GST_LOG ("depth     :  %d", flxh->depth);
      GST_LOG ("speed     :  %d", flxh->speed);

      flxdec->next_time = 0;

      if (flxh->type == FLX_MAGICHDR_FLI) {
        flxdec->frame_time = JIFFIE * flxh->speed;
      } else if (flxh->speed == 0) {
        flxdec->frame_time = GST_SECOND / 70;
      } else {
        flxdec->frame_time = flxh->speed * GST_MSECOND;
      }

      flxdec->duration = flxh->frames * flxdec->frame_time;
      GST_LOG ("duration   :  %" GST_TIME_FORMAT,
          GST_TIME_ARGS (flxdec->duration));

      templ = gst_pad_get_pad_template_caps (flxdec->srcpad);
      caps = gst_caps_copy (templ);
      gst_caps_unref (templ);
      gst_caps_set_simple (caps,
          "width", G_TYPE_INT, flxh->width,
          "height", G_TYPE_INT, flxh->height,
          "framerate", GST_TYPE_FRACTION, (gint) GST_MSECOND,
          (gint) flxdec->frame_time / 1000, NULL);

      gst_pad_set_caps (flxdec->srcpad, caps);
      gst_caps_unref (caps);

      if (flxh->depth <= 8)
        flxdec->converter =
            flx_colorspace_converter_new (flxh->width, flxh->height);

      if (flxh->type == FLX_MAGICHDR_FLC || flxh->type == FLX_MAGICHDR_FLX) {
        GST_LOG ("(FLC) aspect_dx :  %d", flxh->aspect_dx);
        GST_LOG ("(FLC) aspect_dy :  %d", flxh->aspect_dy);
        GST_LOG ("(FLC) oframe1   :  0x%08x", flxh->oframe1);
        GST_LOG ("(FLC) oframe2   :  0x%08x", flxh->oframe2);
      }

      flxdec->size = ((guint) flxh->width * (guint) flxh->height);

      /* create delta and output frame */
      flxdec->frame_data = g_malloc (flxdec->size);
      flxdec->delta_data = g_malloc (flxdec->size);

      flxdec->state = GST_FLXDEC_PLAYING;
    }
  } else if (flxdec->state == GST_FLXDEC_PLAYING) {
    GstBuffer *out;

    /* while we have enough data in the adapter */
    while (avail >= FlxFrameChunkSize && res == GST_FLOW_OK) {
      FlxFrameChunk flxfh;
      guchar *chunk;
      const guint8 *data;
      GstMapInfo map;

      chunk = NULL;
      data = gst_adapter_map (flxdec->adapter, FlxFrameChunkSize);
      memcpy (&flxfh, data, FlxFrameChunkSize);
      FLX_FRAME_CHUNK_FIX_ENDIANNESS (&flxfh);
      gst_adapter_unmap (flxdec->adapter);

      switch (flxfh.id) {
        case FLX_FRAME_TYPE:
          /* check if we have the complete frame */
          if (avail < flxfh.size)
            goto need_more_data;

          /* flush header */
          gst_adapter_flush (flxdec->adapter, FlxFrameChunkSize);

          chunk = gst_adapter_take (flxdec->adapter,
              flxfh.size - FlxFrameChunkSize);
          FLX_FRAME_TYPE_FIX_ENDIANNESS ((FlxFrameType *) chunk);
          if (((FlxFrameType *) chunk)->chunks == 0)
            break;

          /* create 32 bits output frame */
//          res = gst_pad_alloc_buffer_and_set_caps (flxdec->srcpad,
//              GST_BUFFER_OFFSET_NONE,
//              flxdec->size * 4, GST_PAD_CAPS (flxdec->srcpad), &out);
//          if (res != GST_FLOW_OK)
//            break;

          out = gst_buffer_new_and_alloc (flxdec->size * 4);

          /* decode chunks */
          if (!flx_decode_chunks (flxdec,
                  ((FlxFrameType *) chunk)->chunks,
                  chunk + FlxFrameTypeSize, flxdec->frame_data)) {
            GST_ELEMENT_ERROR (flxdec, STREAM, DECODE,
                ("%s", "Could not decode chunk"), NULL);
            return GST_FLOW_ERROR;
          }

          /* save copy of the current frame for possible delta. */
          memcpy (flxdec->delta_data, flxdec->frame_data, flxdec->size);

          gst_buffer_map (out, &map, GST_MAP_WRITE);
          /* convert current frame. */
          flx_colorspace_convert (flxdec->converter, flxdec->frame_data,
              map.data);
          gst_buffer_unmap (out, &map);

          GST_BUFFER_TIMESTAMP (out) = flxdec->next_time;
          flxdec->next_time += flxdec->frame_time;

          res = gst_pad_push (flxdec->srcpad, out);
          break;
        default:
          /* check if we have the complete frame */
          if (avail < flxfh.size)
            goto need_more_data;

          gst_adapter_flush (flxdec->adapter, flxfh.size);
          break;
      }

      g_free (chunk);

      avail = gst_adapter_available (flxdec->adapter);
    }
  }
need_more_data:
  return res;

  /* ERRORS */
wrong_type:
  {
    GST_ELEMENT_ERROR (flxdec, STREAM, WRONG_TYPE, (NULL),
        ("not a flx file (type %x)", flxh->type));
    gst_object_unref (flxdec);
    return GST_FLOW_ERROR;
  }
}