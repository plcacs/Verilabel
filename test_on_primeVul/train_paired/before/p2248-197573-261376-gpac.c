GF_Err adts_dmx_process(GF_Filter *filter)
{
	GF_ADTSDmxCtx *ctx = gf_filter_get_udta(filter);
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u8 *start;
	u32 pck_size, remain, prev_pck_size;
	u64 cts = GF_FILTER_NO_TS;

	//always reparse duration
	if (!ctx->duration.num)
		adts_dmx_check_dur(filter, ctx);

	if (ctx->opid && !ctx->is_playing)
		return GF_OK;

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck) {
		if (gf_filter_pid_is_eos(ctx->ipid)) {
			if (!ctx->adts_buffer_size) {
				if (ctx->opid)
					gf_filter_pid_set_eos(ctx->opid);
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = NULL;
				return GF_EOS;
			}
		} else {
			return GF_OK;
		}
	}

	prev_pck_size = ctx->adts_buffer_size;
	if (pck && !ctx->resume_from) {
		data = (char *) gf_filter_pck_get_data(pck, &pck_size);
		if (!pck_size) {
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OK;
		}

		if (ctx->byte_offset != GF_FILTER_NO_BO) {
			u64 byte_offset = gf_filter_pck_get_byte_offset(pck);
			if (!ctx->adts_buffer_size) {
				ctx->byte_offset = byte_offset;
			} else if (ctx->byte_offset + ctx->adts_buffer_size != byte_offset) {
				ctx->byte_offset = GF_FILTER_NO_BO;
				if ((byte_offset != GF_FILTER_NO_BO) && (byte_offset>ctx->adts_buffer_size) ) {
					ctx->byte_offset = byte_offset - ctx->adts_buffer_size;
				}
			}
		}

		if (ctx->adts_buffer_size + pck_size > ctx->adts_buffer_alloc) {
			ctx->adts_buffer_alloc = ctx->adts_buffer_size + pck_size;
			ctx->adts_buffer = gf_realloc(ctx->adts_buffer, ctx->adts_buffer_alloc);
		}
		memcpy(ctx->adts_buffer + ctx->adts_buffer_size, data, pck_size);
		ctx->adts_buffer_size += pck_size;
	}

	//input pid sets some timescale - we flushed pending data , update cts
	if (ctx->timescale && pck) {
		cts = gf_filter_pck_get_cts(pck);
	}

	if (cts == GF_FILTER_NO_TS) {
		//avoids updating cts
		prev_pck_size = 0;
	}

	remain = ctx->adts_buffer_size;
	start = ctx->adts_buffer;

	if (ctx->resume_from) {
		start += ctx->resume_from - 1;
		remain -= ctx->resume_from - 1;
		ctx->resume_from = 0;
	}

	while (remain) {
		u8 *sync;
		u32 sync_pos, size, offset, bytes_to_drop=0, nb_blocks_per_frame;

		if (!ctx->tag_size && (remain>3)) {

			/* Did we read an ID3v2 ? */
			if (start[0] == 'I' && start[1] == 'D' && start[2] == '3') {
				if (remain<10)
					return GF_OK;

				ctx->tag_size = ((start[9] & 0x7f) + ((start[8] & 0x7f) << 7) + ((start[7] & 0x7f) << 14) + ((start[6] & 0x7f) << 21));

				bytes_to_drop = 10;
				if (ctx->id3_buffer_alloc < ctx->tag_size+10) {
					ctx->id3_buffer = gf_realloc(ctx->id3_buffer, ctx->tag_size+10);
					ctx->id3_buffer_alloc = ctx->tag_size+10;
				}
				memcpy(ctx->id3_buffer, start, 10);
				ctx->id3_buffer_size = 10;
				goto drop_byte;
			}
		}
		if (ctx->tag_size) {
			if (ctx->tag_size>remain) {
				bytes_to_drop = remain;
				ctx->tag_size-=remain;
			} else {
				bytes_to_drop = ctx->tag_size;
				ctx->tag_size = 0;
			}
			memcpy(ctx->id3_buffer + ctx->id3_buffer_size, start, bytes_to_drop);
			ctx->id3_buffer_size += bytes_to_drop;

			if (!ctx->tag_size && ctx->opid) {
				id3dmx_flush(filter, ctx->id3_buffer, ctx->id3_buffer_size, ctx->opid, ctx->expart ? &ctx->vpid : NULL);
				ctx->id3_buffer_size = 0;
			}
			goto drop_byte;

		}

		sync = memchr(start, 0xFF, remain);
		sync_pos = (u32) (sync ? sync - start : remain);

		//couldn't find sync byte in this packet
		if (remain - sync_pos < 7) {
			break;
		}

		//not sync !
		if ((sync[1] & 0xF0) != 0xF0) {
			GF_LOG(ctx->nb_frames ? GF_LOG_WARNING : GF_LOG_DEBUG, GF_LOG_PARSER, ("[ADTSDmx] invalid ADTS sync bytes, resyncing\n"));
			ctx->nb_frames = 0;
			goto drop_byte;
		}
		if (!ctx->bs) {
			ctx->bs = gf_bs_new(sync + 1, remain - sync_pos - 1, GF_BITSTREAM_READ);
		} else {
			gf_bs_reassign_buffer(ctx->bs, sync + 1, remain - sync_pos - 1);
		}

		//ok parse header
		gf_bs_read_int(ctx->bs, 4);

		ctx->hdr.is_mp2 = (Bool)gf_bs_read_int(ctx->bs, 1);
		//if (ctx->mpeg4)
		//we deprecate old MPEG-2 signaling for AAC in ISOBMFF, as it is not well supported anyway and we don't write adif_header as
		//supposed to be for these types
		ctx->hdr.is_mp2 = 0;

		gf_bs_read_int(ctx->bs, 2);
		ctx->hdr.no_crc = (Bool)gf_bs_read_int(ctx->bs, 1);

		ctx->hdr.profile = 1 + gf_bs_read_int(ctx->bs, 2);
		ctx->hdr.sr_idx = gf_bs_read_int(ctx->bs, 4);
		gf_bs_read_int(ctx->bs, 1);
		ctx->hdr.nb_ch = gf_bs_read_int(ctx->bs, 3);

		gf_bs_read_int(ctx->bs, 4);
		ctx->hdr.frame_size = gf_bs_read_int(ctx->bs, 13);
		gf_bs_read_int(ctx->bs, 11);
		nb_blocks_per_frame = gf_bs_read_int(ctx->bs, 2);
		ctx->hdr.hdr_size = 7;

		if (!ctx->hdr.no_crc) {
			u32 skip;
			if (!nb_blocks_per_frame) {
				skip = 2;
			} else {
				skip = 2 + 2*nb_blocks_per_frame; //and we have 2 bytes per raw_data_block
			}
			ctx->hdr.hdr_size += skip;
			gf_bs_skip_bytes(ctx->bs, skip);
		}

		if (!ctx->hdr.frame_size || !GF_M4ASampleRates[ctx->hdr.sr_idx]) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_PARSER, ("[ADTSDmx] Invalid ADTS frame header, resyncing\n"));
			ctx->nb_frames = 0;
			goto drop_byte;
		}
		if ((nb_blocks_per_frame>2) || (nb_blocks_per_frame && ctx->hdr.nb_ch)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_PARSER, ("[ADTSDmx] Unsupported multi-block ADTS frame header - patch welcome\n"));
			ctx->nb_frames = 0;
			goto drop_byte;
		} else if (!nb_blocks_per_frame) {
			if (ctx->aacchcfg<0)
				ctx->hdr.nb_ch = -ctx->aacchcfg;
			else if (!ctx->hdr.nb_ch)
				ctx->hdr.nb_ch = ctx->aacchcfg;

			if (!ctx->hdr.nb_ch) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] Missing channel configuration in ADTS frame header, defaulting to stereo - use `--aacchcfg` to force config\n"));
				ctx->hdr.nb_ch = ctx->aacchcfg = 2;
			}
		}

		if (nb_blocks_per_frame==2) {
			u32 pos = (u32) gf_bs_get_position(ctx->bs);
			gf_m4a_parse_program_config_element(ctx->bs, &ctx->acfg);
			if (!ctx->hdr.no_crc)
				gf_bs_skip_bytes(ctx->bs, 2);  //per block CRC

			ctx->hdr.hdr_size += (u32) gf_bs_get_position(ctx->bs) - pos;
		}
		//value 1->6 match channel number, value 7 is 7.1
		if (ctx->hdr.nb_ch==7)
			ctx->hdr.nb_ch = 8;


		//ready to send packet
		if (ctx->hdr.frame_size + 1 < remain) {
			u32 next_frame = ctx->hdr.frame_size;
			//make sure we are sync!
			if ((sync[next_frame] !=0xFF) || ((sync[next_frame+1] & 0xF0) !=0xF0) ) {
				GF_LOG(ctx->nb_frames ? GF_LOG_WARNING : GF_LOG_DEBUG, GF_LOG_PARSER, ("[ADTSDmx] invalid next ADTS frame sync, resyncing\n"));
				ctx->nb_frames = 0;
				goto drop_byte;
			}
		}
		//otherwise wait for next frame, unless if end of stream
		else if (pck) {
			if (ctx->timescale && !prev_pck_size &&  (cts != GF_FILTER_NO_TS) ) {
				ctx->cts = cts;
			}
			break;
		}

		adts_dmx_check_pid(filter, ctx);

		if (!ctx->is_playing) {
			ctx->resume_from = 1 + ctx->adts_buffer_size - remain;
			return GF_OK;
		}

		ctx->nb_frames++;
		size = ctx->hdr.frame_size - ctx->hdr.hdr_size;
		offset = ctx->hdr.hdr_size;
		//per raw-block CRC
		if ((nb_blocks_per_frame==2) && !ctx->hdr.no_crc)
			size -= 2;

		if (ctx->in_seek) {
			u64 nb_samples_at_seek = (u64) (ctx->start_range * GF_M4ASampleRates[ctx->sr_idx]);
			if (ctx->cts + ctx->dts_inc >= nb_samples_at_seek) {
				//u32 samples_to_discard = (ctx->cts + ctx->dts_inc) - nb_samples_at_seek;
				ctx->in_seek = GF_FALSE;
			}
		}

		bytes_to_drop = ctx->hdr.frame_size;
		if (ctx->timescale && !prev_pck_size &&  (cts != GF_FILTER_NO_TS) ) {
			ctx->cts = cts;
			cts = GF_FILTER_NO_TS;
		}

		if (!ctx->in_seek) {
			dst_pck = gf_filter_pck_new_alloc(ctx->opid, size, &output);
			if (ctx->src_pck) gf_filter_pck_merge_properties(ctx->src_pck, dst_pck);

			memcpy(output, sync + offset, size);

			gf_filter_pck_set_dts(dst_pck, ctx->cts);
			gf_filter_pck_set_cts(dst_pck, ctx->cts);
			gf_filter_pck_set_duration(dst_pck, ctx->dts_inc);
			gf_filter_pck_set_framing(dst_pck, GF_TRUE, GF_TRUE);
			gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);

			if (ctx->byte_offset != GF_FILTER_NO_BO) {
				gf_filter_pck_set_byte_offset(dst_pck, ctx->byte_offset + ctx->hdr.hdr_size);
			}

			gf_filter_pck_send(dst_pck);
		}
		adts_dmx_update_cts(ctx);


		//truncated last frame
		if (bytes_to_drop>remain) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_PARSER, ("[ADTSDmx] truncated ADTS frame!\n"));
			bytes_to_drop=remain;
		}

drop_byte:
		if (!bytes_to_drop) {
			bytes_to_drop = 1;
		}
		start += bytes_to_drop;
		remain -= bytes_to_drop;

		if (prev_pck_size) {
			if (prev_pck_size > bytes_to_drop) prev_pck_size -= bytes_to_drop;
			else {
				prev_pck_size=0;
				if (ctx->src_pck) gf_filter_pck_unref(ctx->src_pck);
				ctx->src_pck = pck;
				if (pck)
					gf_filter_pck_ref_props(&ctx->src_pck);
			}
		}
		if (ctx->byte_offset != GF_FILTER_NO_BO)
			ctx->byte_offset += bytes_to_drop;
	}

	if (!pck) {
		ctx->adts_buffer_size = 0;
		return adts_dmx_process(filter);
	} else {
		if (remain) {
			memmove(ctx->adts_buffer, start, remain);
		}
		ctx->adts_buffer_size = remain;
		gf_filter_pid_drop_packet(ctx->ipid);
	}
	return GF_OK;
}