static int h2c_decode_headers(struct h2c *h2c, struct buffer *rxbuf, uint32_t *flags)
{
	const uint8_t *hdrs = (uint8_t *)b_head(&h2c->dbuf);
	struct buffer *tmp = get_trash_chunk();
	struct http_hdr list[MAX_HTTP_HDR * 2];
	struct buffer *copy = NULL;
	unsigned int msgf;
	struct htx *htx = NULL;
	int flen; // header frame len
	int hole = 0;
	int ret = 0;
	int outlen;
	int wrap;
	int try = 0;

next_frame:
	if (b_data(&h2c->dbuf) - hole < h2c->dfl)
		goto leave; // incomplete input frame

	/* No END_HEADERS means there's one or more CONTINUATION frames. In
	 * this case, we'll try to paste it immediately after the initial
	 * HEADERS frame payload and kill any possible padding. The initial
	 * frame's length will be increased to represent the concatenation
	 * of the two frames. The next frame is read from position <tlen>
	 * and written at position <flen> (minus padding if some is present).
	 */
	if (unlikely(!(h2c->dff & H2_F_HEADERS_END_HEADERS))) {
		struct h2_fh hdr;
		int clen; // CONTINUATION frame's payload length

		if (!h2_peek_frame_hdr(&h2c->dbuf, h2c->dfl + hole, &hdr)) {
			/* no more data, the buffer may be full, either due to
			 * too large a frame or because of too large a hole that
			 * we're going to compact at the end.
			 */
			goto leave;
		}

		if (hdr.ft != H2_FT_CONTINUATION) {
			/* RFC7540#6.10: frame of unexpected type */
			h2c_error(h2c, H2_ERR_PROTOCOL_ERROR);
			goto fail;
		}

		if (hdr.sid != h2c->dsi) {
			/* RFC7540#6.10: frame of different stream */
			h2c_error(h2c, H2_ERR_PROTOCOL_ERROR);
			goto fail;
		}

		if ((unsigned)hdr.len > (unsigned)global.tune.bufsize) {
			/* RFC7540#4.2: invalid frame length */
			h2c_error(h2c, H2_ERR_FRAME_SIZE_ERROR);
			goto fail;
		}

		/* detect when we must stop aggragating frames */
		h2c->dff |= hdr.ff & H2_F_HEADERS_END_HEADERS;

		/* Take as much as we can of the CONTINUATION frame's payload */
		clen = b_data(&h2c->dbuf) - (h2c->dfl + hole + 9);
		if (clen > hdr.len)
			clen = hdr.len;

		/* Move the frame's payload over the padding, hole and frame
		 * header. At least one of hole or dpl is null (see diagrams
		 * above). The hole moves after the new aggragated frame.
		 */
		b_move(&h2c->dbuf, b_peek_ofs(&h2c->dbuf, h2c->dfl + hole + 9), clen, -(h2c->dpl + hole + 9));
		h2c->dfl += clen - h2c->dpl;
		hole     += h2c->dpl + 9;
		h2c->dpl  = 0;
		goto next_frame;
	}

	flen = h2c->dfl - h2c->dpl;

	/* if the input buffer wraps, take a temporary copy of it (rare) */
	wrap = b_wrap(&h2c->dbuf) - b_head(&h2c->dbuf);
	if (wrap < h2c->dfl) {
		copy = alloc_trash_chunk();
		if (!copy) {
			h2c_error(h2c, H2_ERR_INTERNAL_ERROR);
			goto fail;
		}
		memcpy(copy->area, b_head(&h2c->dbuf), wrap);
		memcpy(copy->area + wrap, b_orig(&h2c->dbuf), h2c->dfl - wrap);
		hdrs = (uint8_t *) copy->area;
	}

	/* Skip StreamDep and weight for now (we don't support PRIORITY) */
	if (h2c->dff & H2_F_HEADERS_PRIORITY) {
		if (read_n32(hdrs) == h2c->dsi) {
			/* RFC7540#5.3.1 : stream dep may not depend on itself */
			h2c_error(h2c, H2_ERR_PROTOCOL_ERROR);
			goto fail;
		}

		hdrs += 5; // stream dep = 4, weight = 1
		flen -= 5;
	}

	if (!h2_get_buf(h2c, rxbuf)) {
		h2c->flags |= H2_CF_DEM_SALLOC;
		goto leave;
	}

	/* we can't retry a failed decompression operation so we must be very
	 * careful not to take any risks. In practice the output buffer is
	 * always empty except maybe for trailers, in which case we simply have
	 * to wait for the upper layer to finish consuming what is available.
	 */

	if (h2c->proxy->options2 & PR_O2_USE_HTX) {
		htx = htx_from_buf(rxbuf);
		if (!htx_is_empty(htx)) {
			h2c->flags |= H2_CF_DEM_SFULL;
			goto leave;
		}
	} else {
		if (b_data(rxbuf)) {
			h2c->flags |= H2_CF_DEM_SFULL;
			goto leave;
		}

		rxbuf->head = 0;
		try = b_size(rxbuf);
	}

	/* past this point we cannot roll back in case of error */
	outlen = hpack_decode_frame(h2c->ddht, hdrs, flen, list,
	                            sizeof(list)/sizeof(list[0]), tmp);
	if (outlen < 0) {
		h2c_error(h2c, H2_ERR_COMPRESSION_ERROR);
		goto fail;
	}

	/* The PACK decompressor was updated, let's update the input buffer and
	 * the parser's state to commit these changes and allow us to later
	 * fail solely on the stream if needed.
	 */
	b_del(&h2c->dbuf, h2c->dfl + hole);
	h2c->dfl = hole = 0;
	h2c->st0 = H2_CS_FRAME_H;

	/* OK now we have our header list in <list> */
	msgf = (h2c->dff & H2_F_HEADERS_END_STREAM) ? 0 : H2_MSGF_BODY;

	if (*flags & H2_SF_HEADERS_RCVD)
		goto trailers;

	/* This is the first HEADERS frame so it's a headers block */
	if (htx) {
		/* HTX mode */
		if (h2c->flags & H2_CF_IS_BACK)
			outlen = h2_make_htx_response(list, htx, &msgf);
		else
			outlen = h2_make_htx_request(list, htx, &msgf);
	} else {
		/* HTTP/1 mode */
		outlen = h2_make_h1_request(list, b_tail(rxbuf), try, &msgf);
		if (outlen > 0)
			b_add(rxbuf, outlen);
	}

	if (outlen < 0) {
		/* too large headers? this is a stream error only */
		goto fail;
	}

	if (msgf & H2_MSGF_BODY) {
		/* a payload is present */
		if (msgf & H2_MSGF_BODY_CL)
			*flags |= H2_SF_DATA_CLEN;
		else if (!(msgf & H2_MSGF_BODY_TUNNEL) && !htx)
			*flags |= H2_SF_DATA_CHNK;
	}

 done:
	/* indicate that a HEADERS frame was received for this stream */
	*flags |= H2_SF_HEADERS_RCVD;

	if (h2c->dff & H2_F_HEADERS_END_STREAM) {
		/* Mark the end of message, either using EOM in HTX or with the
		 * trailing CRLF after the end of trailers. Note that DATA_CHNK
		 * is not set during headers with END_STREAM.
		 */
		if (htx) {
			if (!htx_add_endof(htx, HTX_BLK_EOM))
				goto fail;
		}
		else if (*flags & H2_SF_DATA_CHNK) {
			if (!b_putblk(rxbuf, "\r\n", 2))
				goto fail;
		}
	}

	/* success */
	ret = 1;

 leave:
	/* If there is a hole left and it's not at the end, we are forced to
	 * move the remaining data over it.
	 */
	if (hole) {
		if (b_data(&h2c->dbuf) > h2c->dfl + hole)
			b_move(&h2c->dbuf, b_peek_ofs(&h2c->dbuf, h2c->dfl + hole),
			       b_data(&h2c->dbuf) - (h2c->dfl + hole), -hole);
		b_sub(&h2c->dbuf, hole);
	}

	if (b_full(&h2c->dbuf) && h2c->dfl > b_data(&h2c->dbuf)) {
		/* too large frames */
		h2c_error(h2c, H2_ERR_INTERNAL_ERROR);
		ret = -1;
	}

	if (htx)
		htx_to_buf(htx, rxbuf);
	free_trash_chunk(copy);
	return ret;

 fail:
	ret = -1;
	goto leave;

 trailers:
	/* This is the last HEADERS frame hence a trailer */

	if (!(h2c->dff & H2_F_HEADERS_END_STREAM)) {
		/* It's a trailer but it's missing ES flag */
		h2c_error(h2c, H2_ERR_PROTOCOL_ERROR);
		goto fail;
	}

	/* Trailers terminate a DATA sequence. In HTX we have to emit an EOD
	 * block, and when using chunks we must send the 0 CRLF marker. For
	 * other modes, the trailers are silently dropped.
	 */
	if (htx) {
		if (!htx_add_endof(htx, HTX_BLK_EOD))
			goto fail;
		if (h2_make_htx_trailers(list, htx) <= 0)
			goto fail;
	}
	else if (*flags & H2_SF_DATA_CHNK) {
		/* Legacy mode with chunked encoding : we must finalize the
		 * data block message emit the trailing CRLF */
		if (!b_putblk(rxbuf, "0\r\n", 3))
			goto fail;

		outlen = h2_make_h1_trailers(list, b_tail(rxbuf), try);
		if (outlen > 0)
			b_add(rxbuf, outlen);
		else
			goto fail;
	}

	goto done;
}