gst_matroska_demux_parse_stream (GstMatroskaDemux * demux, GstEbmlRead * ebml,
    GstMatroskaTrackContext ** dest_context)
{
  GstMatroskaTrackContext *context;
  GstCaps *caps = NULL;
  GstTagList *cached_taglist;
  GstFlowReturn ret;
  guint32 id, riff_fourcc = 0;
  guint16 riff_audio_fmt = 0;
  gchar *codec = NULL;

  DEBUG_ELEMENT_START (demux, ebml, "TrackEntry");

  /* start with the master */
  if ((ret = gst_ebml_read_master (ebml, &id)) != GST_FLOW_OK) {
    DEBUG_ELEMENT_STOP (demux, ebml, "TrackEntry", ret);
    return ret;
  }

  /* allocate generic... if we know the type, we'll g_renew()
   * with the precise type */
  context = g_new0 (GstMatroskaTrackContext, 1);
  context->index_writer_id = -1;
  context->type = 0;            /* no type yet */
  context->default_duration = 0;
  context->pos = 0;
  context->set_discont = TRUE;
  context->timecodescale = 1.0;
  context->flags =
      GST_MATROSKA_TRACK_ENABLED | GST_MATROSKA_TRACK_DEFAULT |
      GST_MATROSKA_TRACK_LACING;
  context->from_time = GST_CLOCK_TIME_NONE;
  context->from_offset = -1;
  context->to_offset = G_MAXINT64;
  context->alignment = 1;
  context->dts_only = FALSE;
  context->intra_only = FALSE;
  context->tags = gst_tag_list_new_empty ();
  g_queue_init (&context->protection_event_queue);
  context->protection_info = NULL;

  GST_DEBUG_OBJECT (demux, "Parsing a TrackEntry (%d tracks parsed so far)",
      demux->common.num_streams);

  /* try reading the trackentry headers */
  while (ret == GST_FLOW_OK && gst_ebml_read_has_remaining (ebml, 1, TRUE)) {
    if ((ret = gst_ebml_peek_id (ebml, &id)) != GST_FLOW_OK)
      break;

    switch (id) {
        /* track number (unique stream ID) */
      case GST_MATROSKA_ID_TRACKNUMBER:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num == 0) {
          GST_ERROR_OBJECT (demux, "Invalid TrackNumber 0");
          ret = GST_FLOW_ERROR;
          break;
        }

        GST_DEBUG_OBJECT (demux, "TrackNumber: %" G_GUINT64_FORMAT, num);
        context->num = num;
        break;
      }
        /* track UID (unique identifier) */
      case GST_MATROSKA_ID_TRACKUID:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num == 0) {
          GST_ERROR_OBJECT (demux, "Invalid TrackUID 0");
          ret = GST_FLOW_ERROR;
          break;
        }

        GST_DEBUG_OBJECT (demux, "TrackUID: %" G_GUINT64_FORMAT, num);
        context->uid = num;
        break;
      }

        /* track type (video, audio, combined, subtitle, etc.) */
      case GST_MATROSKA_ID_TRACKTYPE:{
        guint64 track_type;

        if ((ret = gst_ebml_read_uint (ebml, &id, &track_type)) != GST_FLOW_OK) {
          break;
        }

        if (context->type != 0 && context->type != track_type) {
          GST_WARNING_OBJECT (demux,
              "More than one tracktype defined in a TrackEntry - skipping");
          break;
        } else if (track_type < 1 || track_type > 254) {
          GST_WARNING_OBJECT (demux, "Invalid TrackType %" G_GUINT64_FORMAT,
              track_type);
          break;
        }

        GST_DEBUG_OBJECT (demux, "TrackType: %" G_GUINT64_FORMAT, track_type);

        /* ok, so we're actually going to reallocate this thing */
        switch (track_type) {
          case GST_MATROSKA_TRACK_TYPE_VIDEO:
            gst_matroska_track_init_video_context (&context);
            break;
          case GST_MATROSKA_TRACK_TYPE_AUDIO:
            gst_matroska_track_init_audio_context (&context);
            break;
          case GST_MATROSKA_TRACK_TYPE_SUBTITLE:
            gst_matroska_track_init_subtitle_context (&context);
            break;
          case GST_MATROSKA_TRACK_TYPE_COMPLEX:
          case GST_MATROSKA_TRACK_TYPE_LOGO:
          case GST_MATROSKA_TRACK_TYPE_BUTTONS:
          case GST_MATROSKA_TRACK_TYPE_CONTROL:
          default:
            GST_WARNING_OBJECT (demux,
                "Unknown or unsupported TrackType %" G_GUINT64_FORMAT,
                track_type);
            context->type = 0;
            break;
        }
        break;
      }

        /* tracktype specific stuff for video */
      case GST_MATROSKA_ID_TRACKVIDEO:{
        GstMatroskaTrackVideoContext *videocontext;

        DEBUG_ELEMENT_START (demux, ebml, "TrackVideo");

        if (!gst_matroska_track_init_video_context (&context)) {
          GST_WARNING_OBJECT (demux,
              "TrackVideo element in non-video track - ignoring track");
          ret = GST_FLOW_ERROR;
          break;
        } else if ((ret = gst_ebml_read_master (ebml, &id)) != GST_FLOW_OK) {
          break;
        }
        videocontext = (GstMatroskaTrackVideoContext *) context;

        while (ret == GST_FLOW_OK &&
            gst_ebml_read_has_remaining (ebml, 1, TRUE)) {
          if ((ret = gst_ebml_peek_id (ebml, &id)) != GST_FLOW_OK)
            break;

          switch (id) {
              /* Should be one level up but some broken muxers write it here. */
            case GST_MATROSKA_ID_TRACKDEFAULTDURATION:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackDefaultDuration 0");
                break;
              }

              GST_DEBUG_OBJECT (demux,
                  "TrackDefaultDuration: %" G_GUINT64_FORMAT, num);
              context->default_duration = num;
              break;
            }

              /* video framerate */
              /* NOTE: This one is here only for backward compatibility.
               * Use _TRACKDEFAULDURATION one level up. */
            case GST_MATROSKA_ID_VIDEOFRAMERATE:{
              gdouble num;

              if ((ret = gst_ebml_read_float (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num <= 0.0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackVideoFPS %lf", num);
                break;
              }

              GST_DEBUG_OBJECT (demux, "TrackVideoFrameRate: %lf", num);
              if (context->default_duration == 0)
                context->default_duration =
                    gst_gdouble_to_guint64 ((gdouble) GST_SECOND * (1.0 / num));
              videocontext->default_fps = num;
              break;
            }

              /* width of the size to display the video at */
            case GST_MATROSKA_ID_VIDEODISPLAYWIDTH:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackVideoDisplayWidth 0");
                break;
              }

              GST_DEBUG_OBJECT (demux,
                  "TrackVideoDisplayWidth: %" G_GUINT64_FORMAT, num);
              videocontext->display_width = num;
              break;
            }

              /* height of the size to display the video at */
            case GST_MATROSKA_ID_VIDEODISPLAYHEIGHT:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackVideoDisplayHeight 0");
                break;
              }

              GST_DEBUG_OBJECT (demux,
                  "TrackVideoDisplayHeight: %" G_GUINT64_FORMAT, num);
              videocontext->display_height = num;
              break;
            }

              /* width of the video in the file */
            case GST_MATROSKA_ID_VIDEOPIXELWIDTH:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackVideoPixelWidth 0");
                break;
              }

              GST_DEBUG_OBJECT (demux,
                  "TrackVideoPixelWidth: %" G_GUINT64_FORMAT, num);
              videocontext->pixel_width = num;
              break;
            }

              /* height of the video in the file */
            case GST_MATROSKA_ID_VIDEOPIXELHEIGHT:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackVideoPixelHeight 0");
                break;
              }

              GST_DEBUG_OBJECT (demux,
                  "TrackVideoPixelHeight: %" G_GUINT64_FORMAT, num);
              videocontext->pixel_height = num;
              break;
            }

              /* whether the video is interlaced */
            case GST_MATROSKA_ID_VIDEOFLAGINTERLACED:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 1)
                videocontext->interlace_mode =
                    GST_MATROSKA_INTERLACE_MODE_INTERLACED;
              else if (num == 2)
                videocontext->interlace_mode =
                    GST_MATROSKA_INTERLACE_MODE_PROGRESSIVE;
              else
                videocontext->interlace_mode =
                    GST_MATROSKA_INTERLACE_MODE_UNKNOWN;

              GST_DEBUG_OBJECT (demux, "video track interlacing mode: %d",
                  videocontext->interlace_mode);
              break;
            }

              /* interlaced field order */
            case GST_MATROSKA_ID_VIDEOFIELDORDER:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (videocontext->interlace_mode !=
                  GST_MATROSKA_INTERLACE_MODE_INTERLACED) {
                GST_WARNING_OBJECT (demux,
                    "FieldOrder element when not interlaced - ignoring");
                break;
              }

              if (num == 0)
                /* turns out we're actually progressive */
                videocontext->interlace_mode =
                    GST_MATROSKA_INTERLACE_MODE_PROGRESSIVE;
              else if (num == 2)
                videocontext->field_order = GST_VIDEO_FIELD_ORDER_UNKNOWN;
              else if (num == 9)
                videocontext->field_order =
                    GST_VIDEO_FIELD_ORDER_TOP_FIELD_FIRST;
              else if (num == 14)
                videocontext->field_order =
                    GST_VIDEO_FIELD_ORDER_BOTTOM_FIELD_FIRST;
              else {
                GST_FIXME_OBJECT (demux,
                    "Unknown or unsupported FieldOrder %" G_GUINT64_FORMAT,
                    num);
                videocontext->field_order = GST_VIDEO_FIELD_ORDER_UNKNOWN;
              }

              GST_DEBUG_OBJECT (demux, "video track field order: %d",
                  videocontext->field_order);
              break;
            }

              /* aspect ratio behaviour */
            case GST_MATROSKA_ID_VIDEOASPECTRATIOTYPE:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num != GST_MATROSKA_ASPECT_RATIO_MODE_FREE &&
                  num != GST_MATROSKA_ASPECT_RATIO_MODE_KEEP &&
                  num != GST_MATROSKA_ASPECT_RATIO_MODE_FIXED) {
                GST_WARNING_OBJECT (demux,
                    "Unknown TrackVideoAspectRatioType 0x%x", (guint) num);
                break;
              }
              GST_DEBUG_OBJECT (demux,
                  "TrackVideoAspectRatioType: %" G_GUINT64_FORMAT, num);
              videocontext->asr_mode = num;
              break;
            }

              /* colourspace (only matters for raw video) fourcc */
            case GST_MATROSKA_ID_VIDEOCOLOURSPACE:{
              guint8 *data;
              guint64 datalen;

              if ((ret =
                      gst_ebml_read_binary (ebml, &id, &data,
                          &datalen)) != GST_FLOW_OK)
                break;

              if (datalen != 4) {
                g_free (data);
                GST_WARNING_OBJECT (demux,
                    "Invalid TrackVideoColourSpace length %" G_GUINT64_FORMAT,
                    datalen);
                break;
              }

              memcpy (&videocontext->fourcc, data, 4);
              GST_DEBUG_OBJECT (demux,
                  "TrackVideoColourSpace: %" GST_FOURCC_FORMAT,
                  GST_FOURCC_ARGS (videocontext->fourcc));
              g_free (data);
              break;
            }

              /* color info */
            case GST_MATROSKA_ID_VIDEOCOLOUR:{
              ret = gst_matroska_demux_parse_colour (demux, ebml, videocontext);
              break;
            }

            case GST_MATROSKA_ID_VIDEOSTEREOMODE:
            {
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              GST_DEBUG_OBJECT (demux, "StereoMode: %" G_GUINT64_FORMAT, num);

              switch (num) {
                case GST_MATROSKA_STEREO_MODE_SBS_RL:
                  videocontext->multiview_flags =
                      GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST;
                  /* fall through */
                case GST_MATROSKA_STEREO_MODE_SBS_LR:
                  videocontext->multiview_mode =
                      GST_VIDEO_MULTIVIEW_MODE_SIDE_BY_SIDE;
                  break;
                case GST_MATROSKA_STEREO_MODE_TB_RL:
                  videocontext->multiview_flags =
                      GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST;
                  /* fall through */
                case GST_MATROSKA_STEREO_MODE_TB_LR:
                  videocontext->multiview_mode =
                      GST_VIDEO_MULTIVIEW_MODE_TOP_BOTTOM;
                  break;
                case GST_MATROSKA_STEREO_MODE_CHECKER_RL:
                  videocontext->multiview_flags =
                      GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST;
                  /* fall through */
                case GST_MATROSKA_STEREO_MODE_CHECKER_LR:
                  videocontext->multiview_mode =
                      GST_VIDEO_MULTIVIEW_MODE_CHECKERBOARD;
                  break;
                case GST_MATROSKA_STEREO_MODE_FBF_RL:
                  videocontext->multiview_flags =
                      GST_VIDEO_MULTIVIEW_FLAGS_RIGHT_VIEW_FIRST;
                  /* fall through */
                case GST_MATROSKA_STEREO_MODE_FBF_LR:
                  videocontext->multiview_mode =
                      GST_VIDEO_MULTIVIEW_MODE_FRAME_BY_FRAME;
                  /* FIXME: In frame-by-frame mode, left/right frame buffers are
                   * laced within one block, and we'll need to apply FIRST_IN_BUNDLE
                   * accordingly. See http://www.matroska.org/technical/specs/index.html#StereoMode */
                  GST_FIXME_OBJECT (demux,
                      "Frame-by-frame stereoscopic mode not fully implemented");
                  break;
              }
              break;
            }

            default:
              GST_WARNING_OBJECT (demux,
                  "Unknown TrackVideo subelement 0x%x - ignoring", id);
              /* fall through */
            case GST_MATROSKA_ID_VIDEODISPLAYUNIT:
            case GST_MATROSKA_ID_VIDEOPIXELCROPBOTTOM:
            case GST_MATROSKA_ID_VIDEOPIXELCROPTOP:
            case GST_MATROSKA_ID_VIDEOPIXELCROPLEFT:
            case GST_MATROSKA_ID_VIDEOPIXELCROPRIGHT:
            case GST_MATROSKA_ID_VIDEOGAMMAVALUE:
              ret = gst_ebml_read_skip (ebml);
              break;
          }
        }

        DEBUG_ELEMENT_STOP (demux, ebml, "TrackVideo", ret);
        break;
      }

        /* tracktype specific stuff for audio */
      case GST_MATROSKA_ID_TRACKAUDIO:{
        GstMatroskaTrackAudioContext *audiocontext;

        DEBUG_ELEMENT_START (demux, ebml, "TrackAudio");

        if (!gst_matroska_track_init_audio_context (&context)) {
          GST_WARNING_OBJECT (demux,
              "TrackAudio element in non-audio track - ignoring track");
          ret = GST_FLOW_ERROR;
          break;
        }

        if ((ret = gst_ebml_read_master (ebml, &id)) != GST_FLOW_OK)
          break;

        audiocontext = (GstMatroskaTrackAudioContext *) context;

        while (ret == GST_FLOW_OK &&
            gst_ebml_read_has_remaining (ebml, 1, TRUE)) {
          if ((ret = gst_ebml_peek_id (ebml, &id)) != GST_FLOW_OK)
            break;

          switch (id) {
              /* samplerate */
            case GST_MATROSKA_ID_AUDIOSAMPLINGFREQ:{
              gdouble num;

              if ((ret = gst_ebml_read_float (ebml, &id, &num)) != GST_FLOW_OK)
                break;


              if (num <= 0.0) {
                GST_WARNING_OBJECT (demux,
                    "Invalid TrackAudioSamplingFrequency %lf", num);
                break;
              }

              GST_DEBUG_OBJECT (demux, "TrackAudioSamplingFrequency: %lf", num);
              audiocontext->samplerate = num;
              break;
            }

              /* bitdepth */
            case GST_MATROSKA_ID_AUDIOBITDEPTH:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackAudioBitDepth 0");
                break;
              }

              GST_DEBUG_OBJECT (demux, "TrackAudioBitDepth: %" G_GUINT64_FORMAT,
                  num);
              audiocontext->bitdepth = num;
              break;
            }

              /* channels */
            case GST_MATROSKA_ID_AUDIOCHANNELS:{
              guint64 num;

              if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
                break;

              if (num == 0) {
                GST_WARNING_OBJECT (demux, "Invalid TrackAudioChannels 0");
                break;
              }

              GST_DEBUG_OBJECT (demux, "TrackAudioChannels: %" G_GUINT64_FORMAT,
                  num);
              audiocontext->channels = num;
              break;
            }

            default:
              GST_WARNING_OBJECT (demux,
                  "Unknown TrackAudio subelement 0x%x - ignoring", id);
              /* fall through */
            case GST_MATROSKA_ID_AUDIOCHANNELPOSITIONS:
            case GST_MATROSKA_ID_AUDIOOUTPUTSAMPLINGFREQ:
              ret = gst_ebml_read_skip (ebml);
              break;
          }
        }

        DEBUG_ELEMENT_STOP (demux, ebml, "TrackAudio", ret);

        break;
      }

        /* codec identifier */
      case GST_MATROSKA_ID_CODECID:{
        gchar *text;

        if ((ret = gst_ebml_read_ascii (ebml, &id, &text)) != GST_FLOW_OK)
          break;

        GST_DEBUG_OBJECT (demux, "CodecID: %s", GST_STR_NULL (text));
        context->codec_id = text;
        break;
      }

        /* codec private data */
      case GST_MATROSKA_ID_CODECPRIVATE:{
        guint8 *data;
        guint64 size;

        if ((ret =
                gst_ebml_read_binary (ebml, &id, &data, &size)) != GST_FLOW_OK)
          break;

        context->codec_priv = data;
        context->codec_priv_size = size;

        GST_DEBUG_OBJECT (demux, "CodecPrivate of size %" G_GUINT64_FORMAT,
            size);
        break;
      }

        /* name of the codec */
      case GST_MATROSKA_ID_CODECNAME:{
        gchar *text;

        if ((ret = gst_ebml_read_utf8 (ebml, &id, &text)) != GST_FLOW_OK)
          break;

        GST_DEBUG_OBJECT (demux, "CodecName: %s", GST_STR_NULL (text));
        context->codec_name = text;
        break;
      }

        /* codec delay */
      case GST_MATROSKA_ID_CODECDELAY:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        context->codec_delay = num;

        GST_DEBUG_OBJECT (demux, "CodecDelay: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (num));
        break;
      }

        /* codec delay */
      case GST_MATROSKA_ID_SEEKPREROLL:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        context->seek_preroll = num;

        GST_DEBUG_OBJECT (demux, "SeekPreroll: %" GST_TIME_FORMAT,
            GST_TIME_ARGS (num));
        break;
      }

        /* name of this track */
      case GST_MATROSKA_ID_TRACKNAME:{
        gchar *text;

        if ((ret = gst_ebml_read_utf8 (ebml, &id, &text)) != GST_FLOW_OK)
          break;

        context->name = text;
        GST_DEBUG_OBJECT (demux, "TrackName: %s", GST_STR_NULL (text));
        break;
      }

        /* language (matters for audio/subtitles, mostly) */
      case GST_MATROSKA_ID_TRACKLANGUAGE:{
        gchar *text;

        if ((ret = gst_ebml_read_utf8 (ebml, &id, &text)) != GST_FLOW_OK)
          break;


        context->language = text;

        /* fre-ca => fre */
        if (strlen (context->language) >= 4 && context->language[3] == '-')
          context->language[3] = '\0';

        GST_DEBUG_OBJECT (demux, "TrackLanguage: %s",
            GST_STR_NULL (context->language));
        break;
      }

        /* whether this is actually used */
      case GST_MATROSKA_ID_TRACKFLAGENABLED:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num)
          context->flags |= GST_MATROSKA_TRACK_ENABLED;
        else
          context->flags &= ~GST_MATROSKA_TRACK_ENABLED;

        GST_DEBUG_OBJECT (demux, "TrackEnabled: %d",
            (context->flags & GST_MATROSKA_TRACK_ENABLED) ? 1 : 0);
        break;
      }

        /* whether it's the default for this track type */
      case GST_MATROSKA_ID_TRACKFLAGDEFAULT:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num)
          context->flags |= GST_MATROSKA_TRACK_DEFAULT;
        else
          context->flags &= ~GST_MATROSKA_TRACK_DEFAULT;

        GST_DEBUG_OBJECT (demux, "TrackDefault: %d",
            (context->flags & GST_MATROSKA_TRACK_DEFAULT) ? 1 : 0);
        break;
      }

        /* whether the track must be used during playback */
      case GST_MATROSKA_ID_TRACKFLAGFORCED:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num)
          context->flags |= GST_MATROSKA_TRACK_FORCED;
        else
          context->flags &= ~GST_MATROSKA_TRACK_FORCED;

        GST_DEBUG_OBJECT (demux, "TrackForced: %d",
            (context->flags & GST_MATROSKA_TRACK_FORCED) ? 1 : 0);
        break;
      }

        /* lacing (like MPEG, where blocks don't end/start on frame
         * boundaries) */
      case GST_MATROSKA_ID_TRACKFLAGLACING:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num)
          context->flags |= GST_MATROSKA_TRACK_LACING;
        else
          context->flags &= ~GST_MATROSKA_TRACK_LACING;

        GST_DEBUG_OBJECT (demux, "TrackLacing: %d",
            (context->flags & GST_MATROSKA_TRACK_LACING) ? 1 : 0);
        break;
      }

        /* default length (in time) of one data block in this track */
      case GST_MATROSKA_ID_TRACKDEFAULTDURATION:{
        guint64 num;

        if ((ret = gst_ebml_read_uint (ebml, &id, &num)) != GST_FLOW_OK)
          break;


        if (num == 0) {
          GST_WARNING_OBJECT (demux, "Invalid TrackDefaultDuration 0");
          break;
        }

        GST_DEBUG_OBJECT (demux, "TrackDefaultDuration: %" G_GUINT64_FORMAT,
            num);
        context->default_duration = num;
        break;
      }

      case GST_MATROSKA_ID_CONTENTENCODINGS:{
        ret = gst_matroska_read_common_read_track_encodings (&demux->common,
            ebml, context);
        break;
      }

      case GST_MATROSKA_ID_TRACKTIMECODESCALE:{
        gdouble num;

        if ((ret = gst_ebml_read_float (ebml, &id, &num)) != GST_FLOW_OK)
          break;

        if (num <= 0.0) {
          GST_WARNING_OBJECT (demux, "Invalid TrackTimeCodeScale %lf", num);
          break;
        }

        GST_DEBUG_OBJECT (demux, "TrackTimeCodeScale: %lf", num);
        context->timecodescale = num;
        break;
      }

      default:
        GST_WARNING ("Unknown TrackEntry subelement 0x%x - ignoring", id);
        /* pass-through */

        /* we ignore these because they're nothing useful (i.e. crap)
         * or simply not implemented yet. */
      case GST_MATROSKA_ID_TRACKMINCACHE:
      case GST_MATROSKA_ID_TRACKMAXCACHE:
      case GST_MATROSKA_ID_MAXBLOCKADDITIONID:
      case GST_MATROSKA_ID_TRACKATTACHMENTLINK:
      case GST_MATROSKA_ID_TRACKOVERLAY:
      case GST_MATROSKA_ID_TRACKTRANSLATE:
      case GST_MATROSKA_ID_TRACKOFFSET:
      case GST_MATROSKA_ID_CODECSETTINGS:
      case GST_MATROSKA_ID_CODECINFOURL:
      case GST_MATROSKA_ID_CODECDOWNLOADURL:
      case GST_MATROSKA_ID_CODECDECODEALL:
        ret = gst_ebml_read_skip (ebml);
        break;
    }
  }

  DEBUG_ELEMENT_STOP (demux, ebml, "TrackEntry", ret);

  /* Decode codec private data if necessary */
  if (context->encodings && context->encodings->len > 0 && context->codec_priv
      && context->codec_priv_size > 0) {
    if (!gst_matroska_decode_data (context->encodings,
            &context->codec_priv, &context->codec_priv_size,
            GST_MATROSKA_TRACK_ENCODING_SCOPE_CODEC_DATA, TRUE)) {
      GST_WARNING_OBJECT (demux, "Decoding codec private data failed");
      ret = GST_FLOW_ERROR;
    }
  }

  if (context->type == 0 || context->codec_id == NULL || (ret != GST_FLOW_OK
          && ret != GST_FLOW_EOS)) {
    if (ret == GST_FLOW_OK || ret == GST_FLOW_EOS)
      GST_WARNING_OBJECT (ebml, "Unknown stream/codec in track entry header");

    gst_matroska_track_free (context);
    context = NULL;
    *dest_context = NULL;
    return ret;
  }

  /* check for a cached track taglist  */
  cached_taglist =
      (GstTagList *) g_hash_table_lookup (demux->common.cached_track_taglists,
      GUINT_TO_POINTER (context->uid));
  if (cached_taglist)
    gst_tag_list_insert (context->tags, cached_taglist, GST_TAG_MERGE_APPEND);

  /* compute caps */
  switch (context->type) {
    case GST_MATROSKA_TRACK_TYPE_VIDEO:{
      GstMatroskaTrackVideoContext *videocontext =
          (GstMatroskaTrackVideoContext *) context;

      caps = gst_matroska_demux_video_caps (videocontext,
          context->codec_id, context->codec_priv,
          context->codec_priv_size, &codec, &riff_fourcc);

      if (codec) {
        gst_tag_list_add (context->tags, GST_TAG_MERGE_REPLACE,
            GST_TAG_VIDEO_CODEC, codec, NULL);
        context->tags_changed = TRUE;
        g_free (codec);
      }
      break;
    }

    case GST_MATROSKA_TRACK_TYPE_AUDIO:{
      GstClockTime lead_in_ts = 0;
      GstMatroskaTrackAudioContext *audiocontext =
          (GstMatroskaTrackAudioContext *) context;

      caps = gst_matroska_demux_audio_caps (audiocontext,
          context->codec_id, context->codec_priv, context->codec_priv_size,
          &codec, &riff_audio_fmt, &lead_in_ts);
      if (lead_in_ts > demux->audio_lead_in_ts) {
        demux->audio_lead_in_ts = lead_in_ts;
        GST_DEBUG_OBJECT (demux, "Increased audio lead-in to %" GST_TIME_FORMAT,
            GST_TIME_ARGS (lead_in_ts));
      }

      if (codec) {
        gst_tag_list_add (context->tags, GST_TAG_MERGE_REPLACE,
            GST_TAG_AUDIO_CODEC, codec, NULL);
        context->tags_changed = TRUE;
        g_free (codec);
      }
      break;
    }

    case GST_MATROSKA_TRACK_TYPE_SUBTITLE:{
      GstMatroskaTrackSubtitleContext *subtitlecontext =
          (GstMatroskaTrackSubtitleContext *) context;

      caps = gst_matroska_demux_subtitle_caps (subtitlecontext,
          context->codec_id, context->codec_priv, context->codec_priv_size);
      break;
    }

    case GST_MATROSKA_TRACK_TYPE_COMPLEX:
    case GST_MATROSKA_TRACK_TYPE_LOGO:
    case GST_MATROSKA_TRACK_TYPE_BUTTONS:
    case GST_MATROSKA_TRACK_TYPE_CONTROL:
    default:
      /* we should already have quit by now */
      g_assert_not_reached ();
  }

  if ((context->language == NULL || *context->language == '\0') &&
      (context->type == GST_MATROSKA_TRACK_TYPE_AUDIO ||
          context->type == GST_MATROSKA_TRACK_TYPE_SUBTITLE)) {
    GST_LOG ("stream %d: language=eng (assuming default)", context->index);
    context->language = g_strdup ("eng");
  }

  if (context->language) {
    const gchar *lang;

    /* Matroska contains ISO 639-2B codes, we want ISO 639-1 */
    lang = gst_tag_get_language_code (context->language);
    gst_tag_list_add (context->tags, GST_TAG_MERGE_REPLACE,
        GST_TAG_LANGUAGE_CODE, (lang) ? lang : context->language, NULL);

    if (context->name) {
      gst_tag_list_add (context->tags, GST_TAG_MERGE_REPLACE,
          GST_TAG_TITLE, context->name, NULL);
    }
    context->tags_changed = TRUE;
  }

  if (caps == NULL) {
    GST_WARNING_OBJECT (demux, "could not determine caps for stream with "
        "codec_id='%s'", context->codec_id);
    switch (context->type) {
      case GST_MATROSKA_TRACK_TYPE_VIDEO:
        caps = gst_caps_new_empty_simple ("video/x-unknown");
        break;
      case GST_MATROSKA_TRACK_TYPE_AUDIO:
        caps = gst_caps_new_empty_simple ("audio/x-unknown");
        break;
      case GST_MATROSKA_TRACK_TYPE_SUBTITLE:
        caps = gst_caps_new_empty_simple ("application/x-subtitle-unknown");
        break;
      case GST_MATROSKA_TRACK_TYPE_COMPLEX:
      default:
        caps = gst_caps_new_empty_simple ("application/x-matroska-unknown");
        break;
    }
    gst_caps_set_simple (caps, "codec-id", G_TYPE_STRING, context->codec_id,
        NULL);

    /* add any unrecognised riff fourcc / audio format, but after codec-id */
    if (context->type == GST_MATROSKA_TRACK_TYPE_AUDIO && riff_audio_fmt != 0)
      gst_caps_set_simple (caps, "format", G_TYPE_INT, riff_audio_fmt, NULL);
    else if (context->type == GST_MATROSKA_TRACK_TYPE_VIDEO && riff_fourcc != 0) {
      gchar *fstr = g_strdup_printf ("%" GST_FOURCC_FORMAT,
          GST_FOURCC_ARGS (riff_fourcc));
      gst_caps_set_simple (caps, "fourcc", G_TYPE_STRING, fstr, NULL);
      g_free (fstr);
    }
  } else if (context->stream_headers != NULL) {
    gst_matroska_demux_add_stream_headers_to_caps (demux,
        context->stream_headers, caps);
  }

  if (context->encodings) {
    GstMatroskaTrackEncoding *enc;
    guint i;

    for (i = 0; i < context->encodings->len; i++) {
      enc = &g_array_index (context->encodings, GstMatroskaTrackEncoding, i);
      if (enc->type == GST_MATROSKA_ENCODING_ENCRYPTION /* encryption */ ) {
        GstStructure *s = gst_caps_get_structure (caps, 0);
        if (!gst_structure_has_name (s, "application/x-webm-enc")) {
          gst_structure_set (s, "original-media-type", G_TYPE_STRING,
              gst_structure_get_name (s), NULL);
          gst_structure_set (s, "encryption-algorithm", G_TYPE_STRING,
              gst_matroska_track_encryption_algorithm_name (enc->enc_algo),
              NULL);
          gst_structure_set (s, "encoding-scope", G_TYPE_STRING,
              gst_matroska_track_encoding_scope_name (enc->scope), NULL);
          gst_structure_set (s, "cipher-mode", G_TYPE_STRING,
              gst_matroska_track_encryption_cipher_mode_name
              (enc->enc_cipher_mode), NULL);
          gst_structure_set_name (s, "application/x-webm-enc");
        }
      }
    }
  }

  context->caps = caps;

  /* tadaah! */
  *dest_context = context;
  return ret;
}