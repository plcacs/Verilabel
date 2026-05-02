gst_riff_create_audio_caps (guint16 codec_id,
    gst_riff_strh * strh, gst_riff_strf_auds * strf,
    GstBuffer * strf_data, GstBuffer * strd_data, char **codec_name,
    gint channel_reorder_map[18])
{
  gboolean block_align = FALSE, rate_chan = TRUE;
  GstCaps *caps = NULL;
  gint i;

  if (channel_reorder_map)
    for (i = 0; i < 18; i++)
      channel_reorder_map[i] = -1;

  switch (codec_id) {
    case GST_RIFF_WAVE_FORMAT_PCM:     /* PCM */
      if (strf != NULL) {
        gint ba = strf->blockalign;
        gint ch = strf->channels;
        gint wd, ws;
        GstAudioFormat format;

        if (ba > (32 / 8) * ch) {
          GST_WARNING ("Invalid block align: %d > %d", ba, (32 / 8) * ch);
          wd = GST_ROUND_UP_8 (strf->bits_per_sample);
        } else if (ba != 0) {
          /* If we have an empty blockalign, we take the width contained in 
           * strf->bits_per_sample */
          wd = ba * 8 / ch;
        } else {
          wd = GST_ROUND_UP_8 (strf->bits_per_sample);
        }

        if (strf->bits_per_sample > 32) {
          GST_WARNING ("invalid depth (%d) of pcm audio, overwriting.",
              strf->bits_per_sample);
          strf->bits_per_sample = wd;
        }

        /* in riff, the depth is stored in the size field but it just means that
         * the _least_ significant bits are cleared. We can therefore just play
         * the sample as if it had a depth == width */
        /* For reference, the actual depth is in strf->bits_per_sample */
        ws = wd;

        format =
            gst_audio_format_build_integer (wd != 8, G_LITTLE_ENDIAN, wd, ws);
        if (format == GST_AUDIO_FORMAT_UNKNOWN) {
          GST_WARNING ("Unsupported raw audio format with width %d", wd);
          return NULL;
        }

        caps = gst_caps_new_simple ("audio/x-raw",
            "format", G_TYPE_STRING, gst_audio_format_to_string (format),
            "layout", G_TYPE_STRING, "interleaved",
            "channels", G_TYPE_INT, ch, NULL);

        /* Add default channel layout. We know no default layout for more than
         * 8 channels. */
        if (ch > 8)
          GST_WARNING ("don't know default layout for %d channels", ch);
        else if (gst_riff_wave_add_default_channel_mask (caps, ch,
                channel_reorder_map))
          GST_DEBUG ("using default channel layout for %d channels", ch);
        else
          GST_WARNING ("failed to add channel layout");
      } else {
        /* FIXME: this is pretty useless - we need fixed caps */
        caps = gst_caps_from_string ("audio/x-raw, "
            "format = (string) { S8, U8, S16LE, U16LE, S24LE, "
            "U24LE, S32LE, U32LE }, " "layout = (string) interleaved");
      }
      if (codec_name && strf)
        *codec_name = g_strdup_printf ("Uncompressed %d-bit PCM audio",
            strf->bits_per_sample);
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM:
      if (strf != NULL) {
        /* Many encoding tools create a wrong bitrate information in the header,
         * so either we calculate the bitrate or mark it as invalid as this
         * would probably confuse timing */
        strf->av_bps = 0;
        if (strf->channels != 0 && strf->rate != 0 && strf->blockalign != 0) {
          int spb = ((strf->blockalign - strf->channels * 7) / 2) * 2;
          strf->av_bps =
              gst_util_uint64_scale_int (strf->rate, strf->blockalign, spb);
          GST_DEBUG ("fixing av_bps to calculated value %d of MS ADPCM",
              strf->av_bps);
        }
      }
      caps = gst_caps_new_simple ("audio/x-adpcm",
          "layout", G_TYPE_STRING, "microsoft", NULL);
      if (codec_name)
        *codec_name = g_strdup ("ADPCM audio");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_IEEE_FLOAT:
      if (strf != NULL) {
        gint ba = strf->blockalign;
        gint ch = strf->channels;
        gint wd = ba * 8 / ch;

        caps = gst_caps_new_simple ("audio/x-raw",
            "format", G_TYPE_STRING, wd == 64 ? "F64LE" : "F32LE",
            "layout", G_TYPE_STRING, "interleaved",
            "channels", G_TYPE_INT, ch, NULL);

        /* Add default channel layout. We know no default layout for more than
         * 8 channels. */
        if (ch > 8)
          GST_WARNING ("don't know default layout for %d channels", ch);
        else if (gst_riff_wave_add_default_channel_mask (caps, ch,
                channel_reorder_map))
          GST_DEBUG ("using default channel layout for %d channels", ch);
        else
          GST_WARNING ("failed to add channel layout");
      } else {
        /* FIXME: this is pretty useless - we need fixed caps */
        caps = gst_caps_from_string ("audio/x-raw, "
            "format = (string) { F32LE, F64LE }, "
            "layout = (string) interleaved");
      }
      if (codec_name && strf)
        *codec_name = g_strdup_printf ("Uncompressed %d-bit IEEE float audio",
            strf->bits_per_sample);
      break;

    case GST_RIFF_WAVE_FORMAT_IBM_CVSD:
      goto unknown;

    case GST_RIFF_WAVE_FORMAT_ALAW:
      if (strf != NULL) {
        if (strf->bits_per_sample != 8) {
          GST_WARNING ("invalid depth (%d) of alaw audio, overwriting.",
              strf->bits_per_sample);
          strf->bits_per_sample = 8;
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
        if (strf->av_bps == 0 || strf->blockalign == 0) {
          GST_WARNING ("fixing av_bps (%d) and blockalign (%d) of alaw audio",
              strf->av_bps, strf->blockalign);
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
      }
      caps = gst_caps_new_empty_simple ("audio/x-alaw");
      if (codec_name)
        *codec_name = g_strdup ("A-law audio");
      break;

    case GST_RIFF_WAVE_FORMAT_WMS:
      caps = gst_caps_new_empty_simple ("audio/x-wms");
      if (strf != NULL) {
        gst_caps_set_simple (caps,
            "bitrate", G_TYPE_INT, strf->av_bps * 8,
            "width", G_TYPE_INT, strf->bits_per_sample,
            "depth", G_TYPE_INT, strf->bits_per_sample, NULL);
      } else {
        gst_caps_set_simple (caps,
            "bitrate", GST_TYPE_INT_RANGE, 0, G_MAXINT, NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("Windows Media Audio Speech");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_MULAW:
      if (strf != NULL) {
        if (strf->bits_per_sample != 8) {
          GST_WARNING ("invalid depth (%d) of mulaw audio, overwriting.",
              strf->bits_per_sample);
          strf->bits_per_sample = 8;
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
        if (strf->av_bps == 0 || strf->blockalign == 0) {
          GST_WARNING ("fixing av_bps (%d) and blockalign (%d) of mulaw audio",
              strf->av_bps, strf->blockalign);
          strf->blockalign = (strf->bits_per_sample * strf->channels) / 8;
          strf->av_bps = strf->blockalign * strf->rate;
        }
      }
      caps = gst_caps_new_empty_simple ("audio/x-mulaw");
      if (codec_name)
        *codec_name = g_strdup ("Mu-law audio");
      break;

    case GST_RIFF_WAVE_FORMAT_OKI_ADPCM:
      goto unknown;

    case GST_RIFF_WAVE_FORMAT_DVI_ADPCM:
      if (strf != NULL) {
        /* Many encoding tools create a wrong bitrate information in the
         * header, so either we calculate the bitrate or mark it as invalid
         * as this would probably confuse timing */
        strf->av_bps = 0;
        if (strf->channels != 0 && strf->rate != 0 && strf->blockalign != 0) {
          int spb = ((strf->blockalign - strf->channels * 4) / 2) * 2;
          strf->av_bps =
              gst_util_uint64_scale_int (strf->rate, strf->blockalign, spb);
          GST_DEBUG ("fixing av_bps to calculated value %d of IMA DVI ADPCM",
              strf->av_bps);
        }
      }
      caps = gst_caps_new_simple ("audio/x-adpcm",
          "layout", G_TYPE_STRING, "dvi", NULL);
      if (codec_name)
        *codec_name = g_strdup ("DVI ADPCM audio");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM_G722:
      caps = gst_caps_new_empty_simple ("audio/G722");
      if (codec_name)
        *codec_name = g_strdup ("G722 audio");
      break;

    case GST_RIFF_WAVE_FORMAT_ITU_G726_ADPCM:
      if (strf != NULL) {
        gint bitrate;
        bitrate = 0;
        if (strf->av_bps == 2000 || strf->av_bps == 3000 || strf->av_bps == 4000
            || strf->av_bps == 5000) {
          strf->blockalign = strf->av_bps / 1000;
          bitrate = strf->av_bps * 8;
        } else if (strf->blockalign >= 2 && strf->blockalign <= 5) {
          bitrate = strf->blockalign * 8000;
        }
        if (bitrate > 0) {
          caps = gst_caps_new_simple ("audio/x-adpcm",
              "layout", G_TYPE_STRING, "g726", "bitrate", G_TYPE_INT, bitrate,
              NULL);
        } else {
          caps = gst_caps_new_simple ("audio/x-adpcm",
              "layout", G_TYPE_STRING, "g726", NULL);
        }
      } else {
        caps = gst_caps_new_simple ("audio/x-adpcm",
            "layout", G_TYPE_STRING, "g726", NULL);
      }
      if (codec_name)
        *codec_name = g_strdup ("G726 ADPCM audio");
      block_align = TRUE;
      break;

    case GST_RIFF_WAVE_FORMAT_DSP_TRUESPEECH:
      caps = gst_caps_new_empty_simple ("audio/x-truespeech");
      if (codec_name)
        *codec_name = g_strdup ("DSP Group TrueSpeech");
      break;

    case GST_RIFF_WAVE_FORMAT_GSM610:
    case GST_RIFF_WAVE_FORMAT_MSN:
      caps = gst_caps_new_empty_simple ("audio/ms-gsm");
      if (codec_name)
        *codec_name = g_strdup ("MS GSM audio");
      break;

    case GST_RIFF_WAVE_FORMAT_MPEGL12: /* mp1 or mp2 */
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 2, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-1 layer 2");
      break;

    case GST_RIFF_WAVE_FORMAT_MPEGL3:  /* mp3 */
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 1, "layer", G_TYPE_INT, 3, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-1 layer 3");
      break;

    case GST_RIFF_WAVE_FORMAT_AMR_NB:  /* amr-nb */
      caps = gst_caps_new_empty_simple ("audio/AMR");
      if (codec_name)
        *codec_name = g_strdup ("AMR Narrow Band (NB)");
      break;

    case GST_RIFF_WAVE_FORMAT_AMR_WB:  /* amr-wb */
      caps = gst_caps_new_empty_simple ("audio/AMR-WB");
      if (codec_name)
        *codec_name = g_strdup ("AMR Wide Band (WB)");
      break;

    case GST_RIFF_WAVE_FORMAT_VORBIS1: /* ogg/vorbis mode 1 */
    case GST_RIFF_WAVE_FORMAT_VORBIS2: /* ogg/vorbis mode 2 */
    case GST_RIFF_WAVE_FORMAT_VORBIS3: /* ogg/vorbis mode 3 */
    case GST_RIFF_WAVE_FORMAT_VORBIS1PLUS:     /* ogg/vorbis mode 1+ */
    case GST_RIFF_WAVE_FORMAT_VORBIS2PLUS:     /* ogg/vorbis mode 2+ */
    case GST_RIFF_WAVE_FORMAT_VORBIS3PLUS:     /* ogg/vorbis mode 3+ */
      caps = gst_caps_new_empty_simple ("audio/x-vorbis");
      if (codec_name)
        *codec_name = g_strdup ("Vorbis");
      break;

    case GST_RIFF_WAVE_FORMAT_A52:
      caps = gst_caps_new_empty_simple ("audio/x-ac3");
      if (codec_name)
        *codec_name = g_strdup ("AC-3 audio");
      break;
    case GST_RIFF_WAVE_FORMAT_DTS:
      caps = gst_caps_new_empty_simple ("audio/x-dts");
      if (codec_name)
        *codec_name = g_strdup ("DTS audio");
      /* wavparse is not always able to specify rate/channels for DTS-in-wav */
      rate_chan = FALSE;
      break;
    case GST_RIFF_WAVE_FORMAT_AAC:
    case GST_RIFF_WAVE_FORMAT_AAC_AC:
    case GST_RIFF_WAVE_FORMAT_AAC_pm:
    {
      caps = gst_caps_new_simple ("audio/mpeg",
          "mpegversion", G_TYPE_INT, 4, NULL);
      if (codec_name)
        *codec_name = g_strdup ("MPEG-4 AAC audio");
      break;
    }
    case GST_RIFF_WAVE_FORMAT_WMAV1:
    case GST_RIFF_WAVE_FORMAT_WMAV2:
    case GST_RIFF_WAVE_FORMAT_WMAV3:
    case GST_RIFF_WAVE_FORMAT_WMAV3_L:
    {
      gint version = (codec_id - GST_RIFF_WAVE_FORMAT_WMAV1) + 1;

      block_align = TRUE;

      caps = gst_caps_new_simple ("audio/x-wma",
          "wmaversion", G_TYPE_INT, version, NULL);

      if (codec_name) {
        if (codec_id == GST_RIFF_WAVE_FORMAT_WMAV3_L)
          *codec_name = g_strdup ("WMA Lossless");
        else
          *codec_name = g_strdup_printf ("WMA Version %d", version + 6);
      }

      if (strf != NULL) {
        gst_caps_set_simple (caps,
            "bitrate", G_TYPE_INT, strf->av_bps * 8,
            "depth", G_TYPE_INT, strf->bits_per_sample, NULL);
      } else {
        gst_caps_set_simple (caps,
            "bitrate", GST_TYPE_INT_RANGE, 0, G_MAXINT, NULL);
      }
      break;
    }
    case GST_RIFF_WAVE_FORMAT_SONY_ATRAC3:
      caps = gst_caps_new_empty_simple ("audio/x-vnd.sony.atrac3");
      if (codec_name)
        *codec_name = g_strdup ("Sony ATRAC3");
      break;

    case GST_RIFF_WAVE_FORMAT_SIREN:
      caps = gst_caps_new_empty_simple ("audio/x-siren");
      if (codec_name)
        *codec_name = g_strdup ("Siren7");
      rate_chan = FALSE;
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM_IMA_DK4:
      caps =
          gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "dk4",
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("IMA/DK4 ADPCM");
      break;
    case GST_RIFF_WAVE_FORMAT_ADPCM_IMA_DK3:
      caps =
          gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "dk3",
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("IMA/DK3 ADPCM");
      break;

    case GST_RIFF_WAVE_FORMAT_ADPCM_IMA_WAV:
      caps =
          gst_caps_new_simple ("audio/x-adpcm", "layout", G_TYPE_STRING, "dvi",
          NULL);
      if (codec_name)
        *codec_name = g_strdup ("IMA/WAV ADPCM");
      break;
    case GST_RIFF_WAVE_FORMAT_EXTENSIBLE:{
      guint16 valid_bits_per_sample;
      guint32 channel_mask;
      guint32 subformat_guid[4];
      GstMapInfo info;
      gsize size;

      /* should be at least 22 bytes */
      size = gst_buffer_get_size (strf_data);

      if (strf_data == NULL || size < 22) {
        GST_WARNING ("WAVE_FORMAT_EXTENSIBLE data size is %" G_GSIZE_FORMAT
            " (expected: 22)", (strf_data) ? size : -1);
        return NULL;
      }

      gst_buffer_map (strf_data, &info, GST_MAP_READ);
      valid_bits_per_sample = GST_READ_UINT16_LE (info.data);
      channel_mask = GST_READ_UINT32_LE (info.data + 2);
      subformat_guid[0] = GST_READ_UINT32_LE (info.data + 6);
      subformat_guid[1] = GST_READ_UINT32_LE (info.data + 10);
      subformat_guid[2] = GST_READ_UINT32_LE (info.data + 14);
      subformat_guid[3] = GST_READ_UINT32_LE (info.data + 18);
      gst_buffer_unmap (strf_data, &info);

      GST_DEBUG ("valid bps    = %u", valid_bits_per_sample);
      GST_DEBUG ("channel mask = 0x%08x", channel_mask);
      GST_DEBUG ("GUID         = %08x-%08x-%08x-%08x", subformat_guid[0],
          subformat_guid[1], subformat_guid[2], subformat_guid[3]);

      if (subformat_guid[1] == 0x00100000 &&
          subformat_guid[2] == 0xaa000080 && subformat_guid[3] == 0x719b3800) {
        if (subformat_guid[0] == 0x00000001) {
          GST_DEBUG ("PCM");
          if (strf != NULL && strf->blockalign != 0 && strf->channels != 0
              && strf->rate != 0) {
            gint ba = strf->blockalign;
            gint wd = ba * 8 / strf->channels;
            gint ws;
            GstAudioFormat format;

            /* in riff, the depth is stored in the size field but it just
             * means that the _least_ significant bits are cleared. We can
             * therefore just play the sample as if it had a depth == width */
            ws = wd;

            /* For reference, use this to get the actual depth:
             * ws = strf->bits_per_sample;
             * if (valid_bits_per_sample != 0)
             *   ws = valid_bits_per_sample; */

            format =
                gst_audio_format_build_integer (wd != 8, G_LITTLE_ENDIAN, wd,
                ws);

            caps = gst_caps_new_simple ("audio/x-raw",
                "format", G_TYPE_STRING, gst_audio_format_to_string (format),
                "layout", G_TYPE_STRING, "interleaved",
                "channels", G_TYPE_INT, strf->channels,
                "rate", G_TYPE_INT, strf->rate, NULL);

            if (codec_name) {
              *codec_name = g_strdup_printf ("Uncompressed %d-bit PCM audio",
                  strf->bits_per_sample);
            }
          }
        } else if (subformat_guid[0] == 0x00000003) {
          GST_DEBUG ("FLOAT");
          if (strf != NULL && strf->blockalign != 0 && strf->channels != 0
              && strf->rate != 0) {
            gint ba = strf->blockalign;
            gint wd = ba * 8 / strf->channels;

            caps = gst_caps_new_simple ("audio/x-raw",
                "format", G_TYPE_STRING, wd == 32 ? "F32LE" : "F64LE",
                "layout", G_TYPE_STRING, "interleaved",
                "channels", G_TYPE_INT, strf->channels,
                "rate", G_TYPE_INT, strf->rate, NULL);

            if (codec_name) {
              *codec_name =
                  g_strdup_printf ("Uncompressed %d-bit IEEE float audio",
                  strf->bits_per_sample);
            }
          }
        } else if (subformat_guid[0] == 0x0000006) {
          GST_DEBUG ("ALAW");
          if (strf != NULL) {
            if (strf->bits_per_sample != 8) {
              GST_WARNING ("invalid depth (%d) of alaw audio, overwriting.",
                  strf->bits_per_sample);
              strf->bits_per_sample = 8;
              strf->av_bps = 8;
              strf->blockalign = strf->av_bps * strf->channels;
            }
            if (strf->av_bps == 0 || strf->blockalign == 0) {
              GST_WARNING
                  ("fixing av_bps (%d) and blockalign (%d) of alaw audio",
                  strf->av_bps, strf->blockalign);
              strf->av_bps = strf->bits_per_sample;
              strf->blockalign = strf->av_bps * strf->channels;
            }
          }
          caps = gst_caps_new_empty_simple ("audio/x-alaw");

          if (codec_name)
            *codec_name = g_strdup ("A-law audio");
        } else if (subformat_guid[0] == 0x00000007) {
          GST_DEBUG ("MULAW");
          if (strf != NULL) {
            if (strf->bits_per_sample != 8) {
              GST_WARNING ("invalid depth (%d) of mulaw audio, overwriting.",
                  strf->bits_per_sample);
              strf->bits_per_sample = 8;
              strf->av_bps = 8;
              strf->blockalign = strf->av_bps * strf->channels;
            }
            if (strf->av_bps == 0 || strf->blockalign == 0) {
              GST_WARNING
                  ("fixing av_bps (%d) and blockalign (%d) of mulaw audio",
                  strf->av_bps, strf->blockalign);
              strf->av_bps = strf->bits_per_sample;
              strf->blockalign = strf->av_bps * strf->channels;
            }
          }
          caps = gst_caps_new_empty_simple ("audio/x-mulaw");
          if (codec_name)
            *codec_name = g_strdup ("Mu-law audio");
        } else if (subformat_guid[0] == 0x00000092) {
          GST_DEBUG ("FIXME: handle DOLBY AC3 SPDIF format");
          GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE AC-3 SPDIF audio");
          caps = gst_caps_new_empty_simple ("audio/x-ac3");
          if (codec_name)
            *codec_name = g_strdup ("wavext AC-3 SPDIF audio");
        } else if ((subformat_guid[0] & 0xffff) ==
            GST_RIFF_WAVE_FORMAT_EXTENSIBLE) {
          GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE nested");
        } else {
          /* recurse where no special consideration has yet to be identified 
           * for the subformat guid */
          caps = gst_riff_create_audio_caps (subformat_guid[0], strh, strf,
              strf_data, strd_data, codec_name, channel_reorder_map);
          if (!codec_name)
            GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE audio");
          if (caps) {
            if (codec_name) {
              GST_DEBUG ("WAVE_FORMAT_EXTENSIBLE %s", *codec_name);
              *codec_name = g_strjoin ("wavext ", *codec_name, NULL);
            }
            return caps;
          }
        }
      } else if (subformat_guid[0] == 0x6ba47966 &&
          subformat_guid[1] == 0x41783f83 &&
          subformat_guid[2] == 0xf0006596 && subformat_guid[3] == 0xe59262bf) {
        caps = gst_caps_new_empty_simple ("application/x-ogg-avi");
        if (codec_name)
          *codec_name = g_strdup ("Ogg-AVI");
      }

      if (caps == NULL) {
        GST_WARNING ("Unknown WAVE_FORMAT_EXTENSIBLE audio format");
        return NULL;
      }

      if (strf != NULL) {
        /* If channel_mask == 0 and channels > 1 let's
         * assume default layout as some wav files don't have the
         * channel mask set. Don't set the layout for 1 channel. */
        if (channel_mask == 0 && strf->channels > 1)
          channel_mask =
              gst_riff_wavext_get_default_channel_mask (strf->channels);

        if ((channel_mask != 0 || strf->channels > 1) &&
            !gst_riff_wavext_add_channel_mask (caps, strf->channels,
                channel_mask, channel_reorder_map)) {
          GST_WARNING ("failed to add channel layout");
          gst_caps_unref (caps);
          caps = NULL;
        }
        rate_chan = FALSE;
      }

      break;
    }
      /* can anything decode these? pitfdll? */
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC8:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC10:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC16:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_AC20:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_METAVOICE:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_METASOUND:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_RT29HW:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_VR12:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_VR18:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_TQ40:
    case GST_RIFF_WAVE_FORMAT_VOXWARE_TQ60:{
      caps = gst_caps_new_simple ("audio/x-voxware",
          "voxwaretype", G_TYPE_INT, (gint) codec_id, NULL);
      if (codec_name)
        *codec_name = g_strdup ("Voxware");
      break;
    }
    default:
    unknown:
      GST_WARNING ("Unknown audio tag 0x%04x", codec_id);
      return NULL;
  }

  if (strf != NULL) {
    if (rate_chan) {
      gst_caps_set_simple (caps,
          "rate", G_TYPE_INT, strf->rate,
          "channels", G_TYPE_INT, strf->channels, NULL);
    }
    if (block_align) {
      gst_caps_set_simple (caps,
          "block_align", G_TYPE_INT, strf->blockalign, NULL);
    }
  } else {
    if (block_align) {
      gst_caps_set_simple (caps,
          "block_align", GST_TYPE_INT_RANGE, 1, G_MAXINT, NULL);
    }
  }

  /* extradata */
  if (strf_data || strd_data) {
    gst_caps_set_simple (caps, "codec_data", GST_TYPE_BUFFER,
        strf_data ? strf_data : strd_data, NULL);
  }

  return caps;
}