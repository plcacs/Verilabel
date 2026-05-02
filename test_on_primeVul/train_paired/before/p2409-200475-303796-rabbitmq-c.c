int amqp_handle_input(amqp_connection_state_t state, amqp_bytes_t received_data,
                      amqp_frame_t *decoded_frame) {
  size_t bytes_consumed;
  void *raw_frame;

  /* Returning frame_type of zero indicates either insufficient input,
     or a complete, ignored frame was read. */
  decoded_frame->frame_type = 0;

  if (received_data.len == 0) {
    return AMQP_STATUS_OK;
  }

  if (state->state == CONNECTION_STATE_IDLE) {
    state->state = CONNECTION_STATE_HEADER;
  }

  bytes_consumed = consume_data(state, &received_data);

  /* do we have target_size data yet? if not, return with the
     expectation that more will arrive */
  if (state->inbound_offset < state->target_size) {
    return (int)bytes_consumed;
  }

  raw_frame = state->inbound_buffer.bytes;

  switch (state->state) {
    case CONNECTION_STATE_INITIAL:
      /* check for a protocol header from the server */
      if (memcmp(raw_frame, "AMQP", 4) == 0) {
        decoded_frame->frame_type = AMQP_PSEUDOFRAME_PROTOCOL_HEADER;
        decoded_frame->channel = 0;

        decoded_frame->payload.protocol_header.transport_high =
            amqp_d8(amqp_offset(raw_frame, 4));
        decoded_frame->payload.protocol_header.transport_low =
            amqp_d8(amqp_offset(raw_frame, 5));
        decoded_frame->payload.protocol_header.protocol_version_major =
            amqp_d8(amqp_offset(raw_frame, 6));
        decoded_frame->payload.protocol_header.protocol_version_minor =
            amqp_d8(amqp_offset(raw_frame, 7));

        return_to_idle(state);
        return (int)bytes_consumed;
      }

    /* it's not a protocol header; fall through to process it as a
       regular frame header */

    case CONNECTION_STATE_HEADER: {
      amqp_channel_t channel;
      amqp_pool_t *channel_pool;
      /* frame length is 3 bytes in */
      channel = amqp_d16(amqp_offset(raw_frame, 1));

      state->target_size =
          amqp_d32(amqp_offset(raw_frame, 3)) + HEADER_SIZE + FOOTER_SIZE;

      if ((size_t)state->frame_max < state->target_size) {
        return AMQP_STATUS_BAD_AMQP_DATA;
      }

      channel_pool = amqp_get_or_create_channel_pool(state, channel);
      if (NULL == channel_pool) {
        return AMQP_STATUS_NO_MEMORY;
      }

      amqp_pool_alloc_bytes(channel_pool, state->target_size,
                            &state->inbound_buffer);
      if (NULL == state->inbound_buffer.bytes) {
        return AMQP_STATUS_NO_MEMORY;
      }
      memcpy(state->inbound_buffer.bytes, state->header_buffer, HEADER_SIZE);
      raw_frame = state->inbound_buffer.bytes;

      state->state = CONNECTION_STATE_BODY;

      bytes_consumed += consume_data(state, &received_data);

      /* do we have target_size data yet? if not, return with the
         expectation that more will arrive */
      if (state->inbound_offset < state->target_size) {
        return (int)bytes_consumed;
      }
    }
    /* fall through to process body */

    case CONNECTION_STATE_BODY: {
      amqp_bytes_t encoded;
      int res;
      amqp_pool_t *channel_pool;

      /* Check frame end marker (footer) */
      if (amqp_d8(amqp_offset(raw_frame, state->target_size - 1)) !=
          AMQP_FRAME_END) {
        return AMQP_STATUS_BAD_AMQP_DATA;
      }

      decoded_frame->frame_type = amqp_d8(amqp_offset(raw_frame, 0));
      decoded_frame->channel = amqp_d16(amqp_offset(raw_frame, 1));

      channel_pool =
          amqp_get_or_create_channel_pool(state, decoded_frame->channel);
      if (NULL == channel_pool) {
        return AMQP_STATUS_NO_MEMORY;
      }

      switch (decoded_frame->frame_type) {
        case AMQP_FRAME_METHOD:
          decoded_frame->payload.method.id =
              amqp_d32(amqp_offset(raw_frame, HEADER_SIZE));
          encoded.bytes = amqp_offset(raw_frame, HEADER_SIZE + 4);
          encoded.len = state->target_size - HEADER_SIZE - 4 - FOOTER_SIZE;

          res = amqp_decode_method(decoded_frame->payload.method.id,
                                   channel_pool, encoded,
                                   &decoded_frame->payload.method.decoded);
          if (res < 0) {
            return res;
          }

          break;

        case AMQP_FRAME_HEADER:
          decoded_frame->payload.properties.class_id =
              amqp_d16(amqp_offset(raw_frame, HEADER_SIZE));
          /* unused 2-byte weight field goes here */
          decoded_frame->payload.properties.body_size =
              amqp_d64(amqp_offset(raw_frame, HEADER_SIZE + 4));
          encoded.bytes = amqp_offset(raw_frame, HEADER_SIZE + 12);
          encoded.len = state->target_size - HEADER_SIZE - 12 - FOOTER_SIZE;
          decoded_frame->payload.properties.raw = encoded;

          res = amqp_decode_properties(
              decoded_frame->payload.properties.class_id, channel_pool, encoded,
              &decoded_frame->payload.properties.decoded);
          if (res < 0) {
            return res;
          }

          break;

        case AMQP_FRAME_BODY:
          decoded_frame->payload.body_fragment.len =
              state->target_size - HEADER_SIZE - FOOTER_SIZE;
          decoded_frame->payload.body_fragment.bytes =
              amqp_offset(raw_frame, HEADER_SIZE);
          break;

        case AMQP_FRAME_HEARTBEAT:
          break;

        default:
          /* Ignore the frame */
          decoded_frame->frame_type = 0;
          break;
      }

      return_to_idle(state);
      return (int)bytes_consumed;
    }

    default:
      amqp_abort("Internal error: invalid amqp_connection_state_t->state %d",
                 state->state);
  }
}