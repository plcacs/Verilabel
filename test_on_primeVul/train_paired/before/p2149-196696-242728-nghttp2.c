ssize_t nghttp2_session_mem_recv(nghttp2_session *session, const uint8_t *in,
                                 size_t inlen) {
  const uint8_t *first = in, *last = in + inlen;
  nghttp2_inbound_frame *iframe = &session->iframe;
  size_t readlen;
  ssize_t padlen;
  int rv;
  int busy = 0;
  nghttp2_frame_hd cont_hd;
  nghttp2_stream *stream;
  size_t pri_fieldlen;
  nghttp2_mem *mem;

  DEBUGF("recv: connection recv_window_size=%d, local_window=%d\n",
         session->recv_window_size, session->local_window_size);

  mem = &session->mem;

  /* We may have idle streams more than we expect (e.g.,
     nghttp2_session_change_stream_priority() or
     nghttp2_session_create_idle_stream()).  Adjust them here. */
  rv = nghttp2_session_adjust_idle_stream(session);
  if (nghttp2_is_fatal(rv)) {
    return rv;
  }

  if (!nghttp2_session_want_read(session)) {
    return (ssize_t)inlen;
  }

  for (;;) {
    switch (iframe->state) {
    case NGHTTP2_IB_READ_CLIENT_MAGIC:
      readlen = nghttp2_min(inlen, iframe->payloadleft);

      if (memcmp(&NGHTTP2_CLIENT_MAGIC[NGHTTP2_CLIENT_MAGIC_LEN -
                                       iframe->payloadleft],
                 in, readlen) != 0) {
        return NGHTTP2_ERR_BAD_CLIENT_MAGIC;
      }

      iframe->payloadleft -= readlen;
      in += readlen;

      if (iframe->payloadleft == 0) {
        session_inbound_frame_reset(session);
        iframe->state = NGHTTP2_IB_READ_FIRST_SETTINGS;
      }

      break;
    case NGHTTP2_IB_READ_FIRST_SETTINGS:
      DEBUGF("recv: [IB_READ_FIRST_SETTINGS]\n");

      readlen = inbound_frame_buf_read(iframe, in, last);
      in += readlen;

      if (nghttp2_buf_mark_avail(&iframe->sbuf)) {
        return in - first;
      }

      if (iframe->sbuf.pos[3] != NGHTTP2_SETTINGS ||
          (iframe->sbuf.pos[4] & NGHTTP2_FLAG_ACK)) {
        rv = session_call_error_callback(
            session, NGHTTP2_ERR_SETTINGS_EXPECTED,
            "Remote peer returned unexpected data while we expected "
            "SETTINGS frame.  Perhaps, peer does not support HTTP/2 "
            "properly.");

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        rv = nghttp2_session_terminate_session_with_reason(
            session, NGHTTP2_PROTOCOL_ERROR, "SETTINGS expected");

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        return (ssize_t)inlen;
      }

      iframe->state = NGHTTP2_IB_READ_HEAD;

    /* Fall through */
    case NGHTTP2_IB_READ_HEAD: {
      int on_begin_frame_called = 0;

      DEBUGF("recv: [IB_READ_HEAD]\n");

      readlen = inbound_frame_buf_read(iframe, in, last);
      in += readlen;

      if (nghttp2_buf_mark_avail(&iframe->sbuf)) {
        return in - first;
      }

      nghttp2_frame_unpack_frame_hd(&iframe->frame.hd, iframe->sbuf.pos);
      iframe->payloadleft = iframe->frame.hd.length;

      DEBUGF("recv: payloadlen=%zu, type=%u, flags=0x%02x, stream_id=%d\n",
             iframe->frame.hd.length, iframe->frame.hd.type,
             iframe->frame.hd.flags, iframe->frame.hd.stream_id);

      if (iframe->frame.hd.length > session->local_settings.max_frame_size) {
        DEBUGF("recv: length is too large %zu > %u\n", iframe->frame.hd.length,
               session->local_settings.max_frame_size);

        rv = nghttp2_session_terminate_session_with_reason(
            session, NGHTTP2_FRAME_SIZE_ERROR, "too large frame size");

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        return (ssize_t)inlen;
      }

      switch (iframe->frame.hd.type) {
      case NGHTTP2_DATA: {
        DEBUGF("recv: DATA\n");

        iframe->frame.hd.flags &=
            (NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_PADDED);
        /* Check stream is open. If it is not open or closing,
           ignore payload. */
        busy = 1;

        rv = session_on_data_received_fail_fast(session);
        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }
        if (rv == NGHTTP2_ERR_IGN_PAYLOAD) {
          DEBUGF("recv: DATA not allowed stream_id=%d\n",
                 iframe->frame.hd.stream_id);
          iframe->state = NGHTTP2_IB_IGN_DATA;
          break;
        }

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        rv = inbound_frame_handle_pad(iframe, &iframe->frame.hd);
        if (rv < 0) {
          rv = nghttp2_session_terminate_session_with_reason(
              session, NGHTTP2_PROTOCOL_ERROR,
              "DATA: insufficient padding space");

          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          return (ssize_t)inlen;
        }

        if (rv == 1) {
          iframe->state = NGHTTP2_IB_READ_PAD_DATA;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_DATA;
        break;
      }
      case NGHTTP2_HEADERS:

        DEBUGF("recv: HEADERS\n");

        iframe->frame.hd.flags &=
            (NGHTTP2_FLAG_END_STREAM | NGHTTP2_FLAG_END_HEADERS |
             NGHTTP2_FLAG_PADDED | NGHTTP2_FLAG_PRIORITY);

        rv = inbound_frame_handle_pad(iframe, &iframe->frame.hd);
        if (rv < 0) {
          rv = nghttp2_session_terminate_session_with_reason(
              session, NGHTTP2_PROTOCOL_ERROR,
              "HEADERS: insufficient padding space");
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          return (ssize_t)inlen;
        }

        if (rv == 1) {
          iframe->state = NGHTTP2_IB_READ_NBYTE;
          break;
        }

        pri_fieldlen = nghttp2_frame_priority_len(iframe->frame.hd.flags);

        if (pri_fieldlen > 0) {
          if (iframe->payloadleft < pri_fieldlen) {
            busy = 1;
            iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
            break;
          }

          iframe->state = NGHTTP2_IB_READ_NBYTE;

          inbound_frame_set_mark(iframe, pri_fieldlen);

          break;
        }

        /* Call on_begin_frame_callback here because
           session_process_headers_frame() may call
           on_begin_headers_callback */
        rv = session_call_on_begin_frame(session, &iframe->frame.hd);

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        on_begin_frame_called = 1;

        rv = session_process_headers_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        busy = 1;

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        if (rv == NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE) {
          rv = nghttp2_session_add_rst_stream(
              session, iframe->frame.hd.stream_id, NGHTTP2_INTERNAL_ERROR);
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        if (rv == NGHTTP2_ERR_IGN_HEADER_BLOCK) {
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_HEADER_BLOCK;

        break;
      case NGHTTP2_PRIORITY:
        DEBUGF("recv: PRIORITY\n");

        iframe->frame.hd.flags = NGHTTP2_FLAG_NONE;

        if (iframe->payloadleft != NGHTTP2_PRIORITY_SPECLEN) {
          busy = 1;

          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;

          break;
        }

        iframe->state = NGHTTP2_IB_READ_NBYTE;

        inbound_frame_set_mark(iframe, NGHTTP2_PRIORITY_SPECLEN);

        break;
      case NGHTTP2_RST_STREAM:
      case NGHTTP2_WINDOW_UPDATE:
#ifdef DEBUGBUILD
        switch (iframe->frame.hd.type) {
        case NGHTTP2_RST_STREAM:
          DEBUGF("recv: RST_STREAM\n");
          break;
        case NGHTTP2_WINDOW_UPDATE:
          DEBUGF("recv: WINDOW_UPDATE\n");
          break;
        }
#endif /* DEBUGBUILD */

        iframe->frame.hd.flags = NGHTTP2_FLAG_NONE;

        if (iframe->payloadleft != 4) {
          busy = 1;
          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_NBYTE;

        inbound_frame_set_mark(iframe, 4);

        break;
      case NGHTTP2_SETTINGS:
        DEBUGF("recv: SETTINGS\n");

        iframe->frame.hd.flags &= NGHTTP2_FLAG_ACK;

        if ((iframe->frame.hd.length % NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH) ||
            ((iframe->frame.hd.flags & NGHTTP2_FLAG_ACK) &&
             iframe->payloadleft > 0)) {
          busy = 1;
          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_SETTINGS;

        if (iframe->payloadleft) {
          nghttp2_settings_entry *min_header_table_size_entry;

          /* We allocate iv with additional one entry, to store the
             minimum header table size. */
          iframe->max_niv =
              iframe->frame.hd.length / NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH + 1;

          if (iframe->max_niv - 1 > session->max_settings) {
            rv = nghttp2_session_terminate_session_with_reason(
                session, NGHTTP2_ENHANCE_YOUR_CALM,
                "SETTINGS: too many setting entries");
            if (nghttp2_is_fatal(rv)) {
              return rv;
            }
            return (ssize_t)inlen;
          }

          iframe->iv = nghttp2_mem_malloc(mem, sizeof(nghttp2_settings_entry) *
                                                   iframe->max_niv);

          if (!iframe->iv) {
            return NGHTTP2_ERR_NOMEM;
          }

          min_header_table_size_entry = &iframe->iv[iframe->max_niv - 1];
          min_header_table_size_entry->settings_id =
              NGHTTP2_SETTINGS_HEADER_TABLE_SIZE;
          min_header_table_size_entry->value = UINT32_MAX;

          inbound_frame_set_mark(iframe, NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH);
          break;
        }

        busy = 1;

        inbound_frame_set_mark(iframe, 0);

        break;
      case NGHTTP2_PUSH_PROMISE:
        DEBUGF("recv: PUSH_PROMISE\n");

        iframe->frame.hd.flags &=
            (NGHTTP2_FLAG_END_HEADERS | NGHTTP2_FLAG_PADDED);

        rv = inbound_frame_handle_pad(iframe, &iframe->frame.hd);
        if (rv < 0) {
          rv = nghttp2_session_terminate_session_with_reason(
              session, NGHTTP2_PROTOCOL_ERROR,
              "PUSH_PROMISE: insufficient padding space");
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          return (ssize_t)inlen;
        }

        if (rv == 1) {
          iframe->state = NGHTTP2_IB_READ_NBYTE;
          break;
        }

        if (iframe->payloadleft < 4) {
          busy = 1;
          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_NBYTE;

        inbound_frame_set_mark(iframe, 4);

        break;
      case NGHTTP2_PING:
        DEBUGF("recv: PING\n");

        iframe->frame.hd.flags &= NGHTTP2_FLAG_ACK;

        if (iframe->payloadleft != 8) {
          busy = 1;
          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_NBYTE;
        inbound_frame_set_mark(iframe, 8);

        break;
      case NGHTTP2_GOAWAY:
        DEBUGF("recv: GOAWAY\n");

        iframe->frame.hd.flags = NGHTTP2_FLAG_NONE;

        if (iframe->payloadleft < 8) {
          busy = 1;
          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_NBYTE;
        inbound_frame_set_mark(iframe, 8);

        break;
      case NGHTTP2_CONTINUATION:
        DEBUGF("recv: unexpected CONTINUATION\n");

        /* Receiving CONTINUATION in this state are subject to
           connection error of type PROTOCOL_ERROR */
        rv = nghttp2_session_terminate_session_with_reason(
            session, NGHTTP2_PROTOCOL_ERROR, "CONTINUATION: unexpected");
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        return (ssize_t)inlen;
      default:
        DEBUGF("recv: extension frame\n");

        if (check_ext_type_set(session->user_recv_ext_types,
                               iframe->frame.hd.type)) {
          if (!session->callbacks.unpack_extension_callback) {
            /* Silently ignore unknown frame type. */

            busy = 1;

            iframe->state = NGHTTP2_IB_IGN_PAYLOAD;

            break;
          }

          busy = 1;

          iframe->state = NGHTTP2_IB_READ_EXTENSION_PAYLOAD;

          break;
        } else {
          switch (iframe->frame.hd.type) {
          case NGHTTP2_ALTSVC:
            if ((session->builtin_recv_ext_types & NGHTTP2_TYPEMASK_ALTSVC) ==
                0) {
              busy = 1;
              iframe->state = NGHTTP2_IB_IGN_PAYLOAD;
              break;
            }

            DEBUGF("recv: ALTSVC\n");

            iframe->frame.hd.flags = NGHTTP2_FLAG_NONE;
            iframe->frame.ext.payload = &iframe->ext_frame_payload.altsvc;

            if (session->server) {
              busy = 1;
              iframe->state = NGHTTP2_IB_IGN_PAYLOAD;
              break;
            }

            if (iframe->payloadleft < 2) {
              busy = 1;
              iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
              break;
            }

            busy = 1;

            iframe->state = NGHTTP2_IB_READ_NBYTE;
            inbound_frame_set_mark(iframe, 2);

            break;
          case NGHTTP2_ORIGIN:
            if (!(session->builtin_recv_ext_types & NGHTTP2_TYPEMASK_ORIGIN)) {
              busy = 1;
              iframe->state = NGHTTP2_IB_IGN_PAYLOAD;
              break;
            }

            DEBUGF("recv: ORIGIN\n");

            iframe->frame.ext.payload = &iframe->ext_frame_payload.origin;

            if (session->server || iframe->frame.hd.stream_id ||
                (iframe->frame.hd.flags & 0xf0)) {
              busy = 1;
              iframe->state = NGHTTP2_IB_IGN_PAYLOAD;
              break;
            }

            iframe->frame.hd.flags = NGHTTP2_FLAG_NONE;

            if (iframe->payloadleft) {
              iframe->raw_lbuf = nghttp2_mem_malloc(mem, iframe->payloadleft);

              if (iframe->raw_lbuf == NULL) {
                return NGHTTP2_ERR_NOMEM;
              }

              nghttp2_buf_wrap_init(&iframe->lbuf, iframe->raw_lbuf,
                                    iframe->payloadleft);
            } else {
              busy = 1;
            }

            iframe->state = NGHTTP2_IB_READ_ORIGIN_PAYLOAD;

            break;
          default:
            busy = 1;

            iframe->state = NGHTTP2_IB_IGN_PAYLOAD;

            break;
          }
        }
      }

      if (!on_begin_frame_called) {
        switch (iframe->state) {
        case NGHTTP2_IB_IGN_HEADER_BLOCK:
        case NGHTTP2_IB_IGN_PAYLOAD:
        case NGHTTP2_IB_FRAME_SIZE_ERROR:
        case NGHTTP2_IB_IGN_DATA:
        case NGHTTP2_IB_IGN_ALL:
          break;
        default:
          rv = session_call_on_begin_frame(session, &iframe->frame.hd);

          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
        }
      }

      break;
    }
    case NGHTTP2_IB_READ_NBYTE:
      DEBUGF("recv: [IB_READ_NBYTE]\n");

      readlen = inbound_frame_buf_read(iframe, in, last);
      in += readlen;
      iframe->payloadleft -= readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu, left=%zd\n", readlen,
             iframe->payloadleft, nghttp2_buf_mark_avail(&iframe->sbuf));

      if (nghttp2_buf_mark_avail(&iframe->sbuf)) {
        return in - first;
      }

      switch (iframe->frame.hd.type) {
      case NGHTTP2_HEADERS:
        if (iframe->padlen == 0 &&
            (iframe->frame.hd.flags & NGHTTP2_FLAG_PADDED)) {
          pri_fieldlen = nghttp2_frame_priority_len(iframe->frame.hd.flags);
          padlen = inbound_frame_compute_pad(iframe);
          if (padlen < 0 ||
              (size_t)padlen + pri_fieldlen > 1 + iframe->payloadleft) {
            rv = nghttp2_session_terminate_session_with_reason(
                session, NGHTTP2_PROTOCOL_ERROR, "HEADERS: invalid padding");
            if (nghttp2_is_fatal(rv)) {
              return rv;
            }
            return (ssize_t)inlen;
          }
          iframe->frame.headers.padlen = (size_t)padlen;

          if (pri_fieldlen > 0) {
            if (iframe->payloadleft < pri_fieldlen) {
              busy = 1;
              iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
              break;
            }
            iframe->state = NGHTTP2_IB_READ_NBYTE;
            inbound_frame_set_mark(iframe, pri_fieldlen);
            break;
          } else {
            /* Truncate buffers used for padding spec */
            inbound_frame_set_mark(iframe, 0);
          }
        }

        rv = session_process_headers_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        busy = 1;

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        if (rv == NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE) {
          rv = nghttp2_session_add_rst_stream(
              session, iframe->frame.hd.stream_id, NGHTTP2_INTERNAL_ERROR);
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        if (rv == NGHTTP2_ERR_IGN_HEADER_BLOCK) {
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_HEADER_BLOCK;

        break;
      case NGHTTP2_PRIORITY:
        rv = session_process_priority_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        session_inbound_frame_reset(session);

        break;
      case NGHTTP2_RST_STREAM:
        rv = session_process_rst_stream_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        session_inbound_frame_reset(session);

        break;
      case NGHTTP2_PUSH_PROMISE:
        if (iframe->padlen == 0 &&
            (iframe->frame.hd.flags & NGHTTP2_FLAG_PADDED)) {
          padlen = inbound_frame_compute_pad(iframe);
          if (padlen < 0 || (size_t)padlen + 4 /* promised stream id */
                                > 1 + iframe->payloadleft) {
            rv = nghttp2_session_terminate_session_with_reason(
                session, NGHTTP2_PROTOCOL_ERROR,
                "PUSH_PROMISE: invalid padding");
            if (nghttp2_is_fatal(rv)) {
              return rv;
            }
            return (ssize_t)inlen;
          }

          iframe->frame.push_promise.padlen = (size_t)padlen;

          if (iframe->payloadleft < 4) {
            busy = 1;
            iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
            break;
          }

          iframe->state = NGHTTP2_IB_READ_NBYTE;

          inbound_frame_set_mark(iframe, 4);

          break;
        }

        rv = session_process_push_promise_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        busy = 1;

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        if (rv == NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE) {
          rv = nghttp2_session_add_rst_stream(
              session, iframe->frame.push_promise.promised_stream_id,
              NGHTTP2_INTERNAL_ERROR);
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        if (rv == NGHTTP2_ERR_IGN_HEADER_BLOCK) {
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        iframe->state = NGHTTP2_IB_READ_HEADER_BLOCK;

        break;
      case NGHTTP2_PING:
        rv = session_process_ping_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        session_inbound_frame_reset(session);

        break;
      case NGHTTP2_GOAWAY: {
        size_t debuglen;

        /* 8 is Last-stream-ID + Error Code */
        debuglen = iframe->frame.hd.length - 8;

        if (debuglen > 0) {
          iframe->raw_lbuf = nghttp2_mem_malloc(mem, debuglen);

          if (iframe->raw_lbuf == NULL) {
            return NGHTTP2_ERR_NOMEM;
          }

          nghttp2_buf_wrap_init(&iframe->lbuf, iframe->raw_lbuf, debuglen);
        }

        busy = 1;

        iframe->state = NGHTTP2_IB_READ_GOAWAY_DEBUG;

        break;
      }
      case NGHTTP2_WINDOW_UPDATE:
        rv = session_process_window_update_frame(session);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        session_inbound_frame_reset(session);

        break;
      case NGHTTP2_ALTSVC: {
        size_t origin_len;

        origin_len = nghttp2_get_uint16(iframe->sbuf.pos);

        DEBUGF("recv: origin_len=%zu\n", origin_len);

        if (origin_len > iframe->payloadleft) {
          busy = 1;
          iframe->state = NGHTTP2_IB_FRAME_SIZE_ERROR;
          break;
        }

        if (iframe->frame.hd.length > 2) {
          iframe->raw_lbuf =
              nghttp2_mem_malloc(mem, iframe->frame.hd.length - 2);

          if (iframe->raw_lbuf == NULL) {
            return NGHTTP2_ERR_NOMEM;
          }

          nghttp2_buf_wrap_init(&iframe->lbuf, iframe->raw_lbuf,
                                iframe->frame.hd.length);
        }

        busy = 1;

        iframe->state = NGHTTP2_IB_READ_ALTSVC_PAYLOAD;

        break;
      }
      default:
        /* This is unknown frame */
        session_inbound_frame_reset(session);

        break;
      }
      break;
    case NGHTTP2_IB_READ_HEADER_BLOCK:
    case NGHTTP2_IB_IGN_HEADER_BLOCK: {
      ssize_t data_readlen;
      size_t trail_padlen;
      int final;
#ifdef DEBUGBUILD
      if (iframe->state == NGHTTP2_IB_READ_HEADER_BLOCK) {
        DEBUGF("recv: [IB_READ_HEADER_BLOCK]\n");
      } else {
        DEBUGF("recv: [IB_IGN_HEADER_BLOCK]\n");
      }
#endif /* DEBUGBUILD */

      readlen = inbound_frame_payload_readlen(iframe, in, last);

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft - readlen);

      data_readlen = inbound_frame_effective_readlen(
          iframe, iframe->payloadleft - readlen, readlen);

      if (data_readlen == -1) {
        /* everything is padding */
        data_readlen = 0;
      }

      trail_padlen = nghttp2_frame_trail_padlen(&iframe->frame, iframe->padlen);

      final = (iframe->frame.hd.flags & NGHTTP2_FLAG_END_HEADERS) &&
              iframe->payloadleft - (size_t)data_readlen == trail_padlen;

      if (data_readlen > 0 || (data_readlen == 0 && final)) {
        size_t hd_proclen = 0;

        DEBUGF("recv: block final=%d\n", final);

        rv =
            inflate_header_block(session, &iframe->frame, &hd_proclen,
                                 (uint8_t *)in, (size_t)data_readlen, final,
                                 iframe->state == NGHTTP2_IB_READ_HEADER_BLOCK);

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        if (rv == NGHTTP2_ERR_PAUSE) {
          in += hd_proclen;
          iframe->payloadleft -= hd_proclen;

          return in - first;
        }

        if (rv == NGHTTP2_ERR_TEMPORAL_CALLBACK_FAILURE) {
          /* The application says no more headers. We decompress the
             rest of the header block but not invoke on_header_callback
             and on_frame_recv_callback. */
          in += hd_proclen;
          iframe->payloadleft -= hd_proclen;

          /* Use promised stream ID for PUSH_PROMISE */
          rv = nghttp2_session_add_rst_stream(
              session,
              iframe->frame.hd.type == NGHTTP2_PUSH_PROMISE
                  ? iframe->frame.push_promise.promised_stream_id
                  : iframe->frame.hd.stream_id,
              NGHTTP2_INTERNAL_ERROR);
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
          busy = 1;
          iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
          break;
        }

        in += readlen;
        iframe->payloadleft -= readlen;

        if (rv == NGHTTP2_ERR_HEADER_COMP) {
          /* GOAWAY is already issued */
          if (iframe->payloadleft == 0) {
            session_inbound_frame_reset(session);
          } else {
            busy = 1;
            iframe->state = NGHTTP2_IB_IGN_PAYLOAD;
          }
          break;
        }
      } else {
        in += readlen;
        iframe->payloadleft -= readlen;
      }

      if (iframe->payloadleft) {
        break;
      }

      if ((iframe->frame.hd.flags & NGHTTP2_FLAG_END_HEADERS) == 0) {

        inbound_frame_set_mark(iframe, NGHTTP2_FRAME_HDLEN);

        iframe->padlen = 0;

        if (iframe->state == NGHTTP2_IB_READ_HEADER_BLOCK) {
          iframe->state = NGHTTP2_IB_EXPECT_CONTINUATION;
        } else {
          iframe->state = NGHTTP2_IB_IGN_CONTINUATION;
        }
      } else {
        if (iframe->state == NGHTTP2_IB_READ_HEADER_BLOCK) {
          rv = session_after_header_block_received(session);
          if (nghttp2_is_fatal(rv)) {
            return rv;
          }
        }
        session_inbound_frame_reset(session);
      }
      break;
    }
    case NGHTTP2_IB_IGN_PAYLOAD:
      DEBUGF("recv: [IB_IGN_PAYLOAD]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);
      iframe->payloadleft -= readlen;
      in += readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (iframe->payloadleft) {
        break;
      }

      switch (iframe->frame.hd.type) {
      case NGHTTP2_HEADERS:
      case NGHTTP2_PUSH_PROMISE:
      case NGHTTP2_CONTINUATION:
        /* Mark inflater bad so that we won't perform further decoding */
        session->hd_inflater.ctx.bad = 1;
        break;
      default:
        break;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_FRAME_SIZE_ERROR:
      DEBUGF("recv: [IB_FRAME_SIZE_ERROR]\n");

      rv = session_handle_frame_size_error(session);
      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      assert(iframe->state == NGHTTP2_IB_IGN_ALL);

      return (ssize_t)inlen;
    case NGHTTP2_IB_READ_SETTINGS:
      DEBUGF("recv: [IB_READ_SETTINGS]\n");

      readlen = inbound_frame_buf_read(iframe, in, last);
      iframe->payloadleft -= readlen;
      in += readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (nghttp2_buf_mark_avail(&iframe->sbuf)) {
        break;
      }

      if (readlen > 0) {
        inbound_frame_set_settings_entry(iframe);
      }
      if (iframe->payloadleft) {
        inbound_frame_set_mark(iframe, NGHTTP2_FRAME_SETTINGS_ENTRY_LENGTH);
        break;
      }

      rv = session_process_settings_frame(session);

      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      if (iframe->state == NGHTTP2_IB_IGN_ALL) {
        return (ssize_t)inlen;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_READ_GOAWAY_DEBUG:
      DEBUGF("recv: [IB_READ_GOAWAY_DEBUG]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);

      if (readlen > 0) {
        iframe->lbuf.last = nghttp2_cpymem(iframe->lbuf.last, in, readlen);

        iframe->payloadleft -= readlen;
        in += readlen;
      }

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (iframe->payloadleft) {
        assert(nghttp2_buf_avail(&iframe->lbuf) > 0);

        break;
      }

      rv = session_process_goaway_frame(session);

      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      if (iframe->state == NGHTTP2_IB_IGN_ALL) {
        return (ssize_t)inlen;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_EXPECT_CONTINUATION:
    case NGHTTP2_IB_IGN_CONTINUATION:
#ifdef DEBUGBUILD
      if (iframe->state == NGHTTP2_IB_EXPECT_CONTINUATION) {
        fprintf(stderr, "recv: [IB_EXPECT_CONTINUATION]\n");
      } else {
        fprintf(stderr, "recv: [IB_IGN_CONTINUATION]\n");
      }
#endif /* DEBUGBUILD */

      readlen = inbound_frame_buf_read(iframe, in, last);
      in += readlen;

      if (nghttp2_buf_mark_avail(&iframe->sbuf)) {
        return in - first;
      }

      nghttp2_frame_unpack_frame_hd(&cont_hd, iframe->sbuf.pos);
      iframe->payloadleft = cont_hd.length;

      DEBUGF("recv: payloadlen=%zu, type=%u, flags=0x%02x, stream_id=%d\n",
             cont_hd.length, cont_hd.type, cont_hd.flags, cont_hd.stream_id);

      if (cont_hd.type != NGHTTP2_CONTINUATION ||
          cont_hd.stream_id != iframe->frame.hd.stream_id) {
        DEBUGF("recv: expected stream_id=%d, type=%d, but got stream_id=%d, "
               "type=%u\n",
               iframe->frame.hd.stream_id, NGHTTP2_CONTINUATION,
               cont_hd.stream_id, cont_hd.type);
        rv = nghttp2_session_terminate_session_with_reason(
            session, NGHTTP2_PROTOCOL_ERROR,
            "unexpected non-CONTINUATION frame or stream_id is invalid");
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        return (ssize_t)inlen;
      }

      /* CONTINUATION won't bear NGHTTP2_PADDED flag */

      iframe->frame.hd.flags = (uint8_t)(
          iframe->frame.hd.flags | (cont_hd.flags & NGHTTP2_FLAG_END_HEADERS));
      iframe->frame.hd.length += cont_hd.length;

      busy = 1;

      if (iframe->state == NGHTTP2_IB_EXPECT_CONTINUATION) {
        iframe->state = NGHTTP2_IB_READ_HEADER_BLOCK;

        rv = session_call_on_begin_frame(session, &cont_hd);

        if (nghttp2_is_fatal(rv)) {
          return rv;
        }
      } else {
        iframe->state = NGHTTP2_IB_IGN_HEADER_BLOCK;
      }

      break;
    case NGHTTP2_IB_READ_PAD_DATA:
      DEBUGF("recv: [IB_READ_PAD_DATA]\n");

      readlen = inbound_frame_buf_read(iframe, in, last);
      in += readlen;
      iframe->payloadleft -= readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu, left=%zu\n", readlen,
             iframe->payloadleft, nghttp2_buf_mark_avail(&iframe->sbuf));

      if (nghttp2_buf_mark_avail(&iframe->sbuf)) {
        return in - first;
      }

      /* Pad Length field is subject to flow control */
      rv = nghttp2_session_update_recv_connection_window_size(session, readlen);
      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      if (iframe->state == NGHTTP2_IB_IGN_ALL) {
        return (ssize_t)inlen;
      }

      /* Pad Length field is consumed immediately */
      rv =
          nghttp2_session_consume(session, iframe->frame.hd.stream_id, readlen);

      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      if (iframe->state == NGHTTP2_IB_IGN_ALL) {
        return (ssize_t)inlen;
      }

      stream = nghttp2_session_get_stream(session, iframe->frame.hd.stream_id);
      if (stream) {
        rv = nghttp2_session_update_recv_stream_window_size(
            session, stream, readlen,
            iframe->payloadleft ||
                (iframe->frame.hd.flags & NGHTTP2_FLAG_END_STREAM) == 0);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }
      }

      busy = 1;

      padlen = inbound_frame_compute_pad(iframe);
      if (padlen < 0) {
        rv = nghttp2_session_terminate_session_with_reason(
            session, NGHTTP2_PROTOCOL_ERROR, "DATA: invalid padding");
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }
        return (ssize_t)inlen;
      }

      iframe->frame.data.padlen = (size_t)padlen;

      iframe->state = NGHTTP2_IB_READ_DATA;

      break;
    case NGHTTP2_IB_READ_DATA:
      stream = nghttp2_session_get_stream(session, iframe->frame.hd.stream_id);

      if (!stream) {
        busy = 1;
        iframe->state = NGHTTP2_IB_IGN_DATA;
        break;
      }

      DEBUGF("recv: [IB_READ_DATA]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);
      iframe->payloadleft -= readlen;
      in += readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (readlen > 0) {
        ssize_t data_readlen;

        rv = nghttp2_session_update_recv_connection_window_size(session,
                                                                readlen);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        rv = nghttp2_session_update_recv_stream_window_size(
            session, stream, readlen,
            iframe->payloadleft ||
                (iframe->frame.hd.flags & NGHTTP2_FLAG_END_STREAM) == 0);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        data_readlen = inbound_frame_effective_readlen(
            iframe, iframe->payloadleft, readlen);

        if (data_readlen == -1) {
          /* everything is padding */
          data_readlen = 0;
        }

        padlen = (ssize_t)readlen - data_readlen;

        if (padlen > 0) {
          /* Padding is considered as "consumed" immediately */
          rv = nghttp2_session_consume(session, iframe->frame.hd.stream_id,
                                       (size_t)padlen);

          if (nghttp2_is_fatal(rv)) {
            return rv;
          }

          if (iframe->state == NGHTTP2_IB_IGN_ALL) {
            return (ssize_t)inlen;
          }
        }

        DEBUGF("recv: data_readlen=%zd\n", data_readlen);

        if (data_readlen > 0) {
          if (session_enforce_http_messaging(session)) {
            if (nghttp2_http_on_data_chunk(stream, (size_t)data_readlen) != 0) {
              if (session->opt_flags & NGHTTP2_OPTMASK_NO_AUTO_WINDOW_UPDATE) {
                /* Consume all data for connection immediately here */
                rv = session_update_connection_consumed_size(
                    session, (size_t)data_readlen);

                if (nghttp2_is_fatal(rv)) {
                  return rv;
                }

                if (iframe->state == NGHTTP2_IB_IGN_DATA) {
                  return (ssize_t)inlen;
                }
              }

              rv = nghttp2_session_add_rst_stream(
                  session, iframe->frame.hd.stream_id, NGHTTP2_PROTOCOL_ERROR);
              if (nghttp2_is_fatal(rv)) {
                return rv;
              }
              busy = 1;
              iframe->state = NGHTTP2_IB_IGN_DATA;
              break;
            }
          }
          if (session->callbacks.on_data_chunk_recv_callback) {
            rv = session->callbacks.on_data_chunk_recv_callback(
                session, iframe->frame.hd.flags, iframe->frame.hd.stream_id,
                in - readlen, (size_t)data_readlen, session->user_data);
            if (rv == NGHTTP2_ERR_PAUSE) {
              return in - first;
            }

            if (nghttp2_is_fatal(rv)) {
              return NGHTTP2_ERR_CALLBACK_FAILURE;
            }
          }
        }
      }

      if (iframe->payloadleft) {
        break;
      }

      rv = session_process_data_frame(session);
      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_IGN_DATA:
      DEBUGF("recv: [IB_IGN_DATA]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);
      iframe->payloadleft -= readlen;
      in += readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (readlen > 0) {
        /* Update connection-level flow control window for ignored
           DATA frame too */
        rv = nghttp2_session_update_recv_connection_window_size(session,
                                                                readlen);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (iframe->state == NGHTTP2_IB_IGN_ALL) {
          return (ssize_t)inlen;
        }

        if (session->opt_flags & NGHTTP2_OPTMASK_NO_AUTO_WINDOW_UPDATE) {

          /* Ignored DATA is considered as "consumed" immediately. */
          rv = session_update_connection_consumed_size(session, readlen);

          if (nghttp2_is_fatal(rv)) {
            return rv;
          }

          if (iframe->state == NGHTTP2_IB_IGN_ALL) {
            return (ssize_t)inlen;
          }
        }
      }

      if (iframe->payloadleft) {
        break;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_IGN_ALL:
      return (ssize_t)inlen;
    case NGHTTP2_IB_READ_EXTENSION_PAYLOAD:
      DEBUGF("recv: [IB_READ_EXTENSION_PAYLOAD]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);
      iframe->payloadleft -= readlen;
      in += readlen;

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (readlen > 0) {
        rv = session_call_on_extension_chunk_recv_callback(
            session, in - readlen, readlen);
        if (nghttp2_is_fatal(rv)) {
          return rv;
        }

        if (rv != 0) {
          busy = 1;

          iframe->state = NGHTTP2_IB_IGN_PAYLOAD;

          break;
        }
      }

      if (iframe->payloadleft > 0) {
        break;
      }

      rv = session_process_extension_frame(session);
      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_READ_ALTSVC_PAYLOAD:
      DEBUGF("recv: [IB_READ_ALTSVC_PAYLOAD]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);
      if (readlen > 0) {
        iframe->lbuf.last = nghttp2_cpymem(iframe->lbuf.last, in, readlen);

        iframe->payloadleft -= readlen;
        in += readlen;
      }

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (iframe->payloadleft) {
        assert(nghttp2_buf_avail(&iframe->lbuf) > 0);

        break;
      }

      rv = session_process_altsvc_frame(session);
      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      session_inbound_frame_reset(session);

      break;
    case NGHTTP2_IB_READ_ORIGIN_PAYLOAD:
      DEBUGF("recv: [IB_READ_ORIGIN_PAYLOAD]\n");

      readlen = inbound_frame_payload_readlen(iframe, in, last);

      if (readlen > 0) {
        iframe->lbuf.last = nghttp2_cpymem(iframe->lbuf.last, in, readlen);

        iframe->payloadleft -= readlen;
        in += readlen;
      }

      DEBUGF("recv: readlen=%zu, payloadleft=%zu\n", readlen,
             iframe->payloadleft);

      if (iframe->payloadleft) {
        assert(nghttp2_buf_avail(&iframe->lbuf) > 0);

        break;
      }

      rv = session_process_origin_frame(session);

      if (nghttp2_is_fatal(rv)) {
        return rv;
      }

      if (iframe->state == NGHTTP2_IB_IGN_ALL) {
        return (ssize_t)inlen;
      }

      session_inbound_frame_reset(session);

      break;
    }

    if (!busy && in == last) {
      break;
    }

    busy = 0;
  }

  assert(in == last);

  return in - first;
}