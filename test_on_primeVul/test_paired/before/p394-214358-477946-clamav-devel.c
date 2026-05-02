int qtm_decompress(struct qtm_stream *qtm, off_t out_bytes) {
  unsigned int frame_start, frame_end, window_posn, match_offset, range;
  unsigned char *window, *i_ptr, *i_end, *runsrc, *rundest;
  int i, j, selector, extra, sym, match_length, ret;
  unsigned short H, L, C, symf;

  register unsigned int bit_buffer;
  register unsigned char bits_left;
  unsigned char bits_needed, bit_run;

  /* easy answers */
  if (!qtm || (out_bytes < 0)) return CL_ENULLARG;
  if (qtm->error) return qtm->error;

  /* flush out any stored-up bytes before we begin */
  i = qtm->o_end - qtm->o_ptr;
  if ((off_t) i > out_bytes) i = (int) out_bytes;
  if (i) {
    if (qtm->wflag && (ret = mspack_write(qtm->ofd, qtm->o_ptr, i, qtm->file)) != CL_SUCCESS) {
      return qtm->error = ret;
    }
    qtm->o_ptr  += i;
    out_bytes   -= i;
  }
  if (out_bytes == 0) return CL_SUCCESS;

  /* restore local state */
  QTM_RESTORE_BITS;
  window = qtm->window;
  window_posn = qtm->window_posn;
  frame_start = qtm->frame_start;
  H = qtm->H;
  L = qtm->L;
  C = qtm->C;

  /* while we do not have enough decoded bytes in reserve: */
  while ((qtm->o_end - qtm->o_ptr) < out_bytes) {

    /* read header if necessary. Initialises H, L and C */
    if (!qtm->header_read) {
      H = 0xFFFF; L = 0; QTM_READ_BITS(C, 16);
      qtm->header_read = 1;
    }

    /* decode more, at most up to to frame boundary */
    frame_end = window_posn + (out_bytes - (qtm->o_end - qtm->o_ptr));
    if ((frame_start + QTM_FRAME_SIZE) < frame_end) {
      frame_end = frame_start + QTM_FRAME_SIZE;
    }

    while (window_posn < frame_end) {
      QTM_GET_SYMBOL(qtm->model7, selector);
      if (selector < 4) {
	struct qtm_model *mdl = (selector == 0) ? &qtm->model0 :
	                        ((selector == 1) ? &qtm->model1 :
				((selector == 2) ? &qtm->model2 :
                                                   &qtm->model3));
	QTM_GET_SYMBOL((*mdl), sym);
	window[window_posn++] = sym;
      }
      else {
	switch (selector) {
	case 4: /* selector 4 = fixed length match (3 bytes) */
	  QTM_GET_SYMBOL(qtm->model4, sym);
	  QTM_READ_BITS(extra, qtm->extra_bits[sym]);
	  match_offset = qtm->position_base[sym] + extra + 1;
	  match_length = 3;
	  break;

	case 5: /* selector 5 = fixed length match (4 bytes) */
	  QTM_GET_SYMBOL(qtm->model5, sym);
	  QTM_READ_BITS(extra, qtm->extra_bits[sym]);
	  match_offset = qtm->position_base[sym] + extra + 1;
	  match_length = 4;
	  break;

	case 6: /* selector 6 = variable length match */
	  QTM_GET_SYMBOL(qtm->model6len, sym);
	  QTM_READ_BITS(extra, qtm->length_extra[sym]);
	  match_length = qtm->length_base[sym] + extra + 5;

	  QTM_GET_SYMBOL(qtm->model6, sym);
	  QTM_READ_BITS(extra, qtm->extra_bits[sym]);
	  match_offset = qtm->position_base[sym] + extra + 1;
	  break;

	default:
	  /* should be impossible, model7 can only return 0-6 */
	  return qtm->error = CL_EFORMAT;
	}

	rundest = &window[window_posn];
	i = match_length;
	/* does match offset wrap the window? */
	if (match_offset > window_posn) {
	  /* j = length from match offset to end of window */
	  j = match_offset - window_posn;
	  if (j > (int) qtm->window_size) {
	    cli_dbgmsg("qtm_decompress: match offset beyond window boundaries\n");
	    return qtm->error = CL_EFORMAT;
	  }
	  runsrc = &window[qtm->window_size - j];
	  if (j < i) {
	    /* if match goes over the window edge, do two copy runs */
	    i -= j; while (j-- > 0) *rundest++ = *runsrc++;
	    runsrc = window;
	  }
	  while (i-- > 0) *rundest++ = *runsrc++;
	}
	else {
	  runsrc = rundest - match_offset;
	  if(i > (int) (qtm->window_size - window_posn))
	    i = qtm->window_size - window_posn;
	  while (i-- > 0) *rundest++ = *runsrc++;
	}
	window_posn += match_length;
      }
    } /* while (window_posn < frame_end) */

    qtm->o_end = &window[window_posn];

    /* another frame completed? */
    if ((window_posn - frame_start) >= QTM_FRAME_SIZE) {
      if ((window_posn - frame_start) != QTM_FRAME_SIZE) {
	cli_dbgmsg("qtm_decompress: overshot frame alignment\n");
	return qtm->error = CL_EFORMAT;
      }

      /* re-align input */
      if (bits_left & 7) QTM_REMOVE_BITS(bits_left & 7);
      do { QTM_READ_BITS(i, 8); } while (i != 0xFF);
      qtm->header_read = 0;

      /* window wrap? */
      if (window_posn == qtm->window_size) {
	/* flush all currently stored data */
	i = (qtm->o_end - qtm->o_ptr);
	if (qtm->wflag && (ret = mspack_write(qtm->ofd, qtm->o_ptr, i, qtm->file)) != CL_SUCCESS) {
	  return qtm->error = ret;
	}
	out_bytes -= i;
	qtm->o_ptr = &window[0];
	qtm->o_end = &window[0];
	window_posn = 0;
      }

      frame_start = window_posn;
    }

  } /* while (more bytes needed) */

  if (out_bytes) {
    i = (int) out_bytes;
    if (qtm->wflag && (ret = mspack_write(qtm->ofd, qtm->o_ptr, i, qtm->file)) != CL_SUCCESS) {
      return qtm->error = ret;
    }
    qtm->o_ptr += i;
  }

  /* store local state */
  QTM_STORE_BITS;
  qtm->window_posn = window_posn;
  qtm->frame_start = frame_start;
  qtm->H = H;
  qtm->L = L;
  qtm->C = C;

  return CL_SUCCESS;
}