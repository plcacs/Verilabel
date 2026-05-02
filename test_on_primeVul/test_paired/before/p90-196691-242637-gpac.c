static GF_Err isoffin_process(GF_Filter *filter)
{
	ISOMReader *read = gf_filter_get_udta(filter);
	u32 i, count = gf_list_count(read->channels);
	Bool is_active = GF_FALSE;
	Bool in_is_eos = GF_FALSE;
	Bool check_forced_end = GF_FALSE;
	Bool has_new_data = GF_FALSE;
	u64 min_offset_plus_one = 0;
	u32 nb_forced_end=0;
	if (read->in_error)
		return read->in_error;

	if (read->pid) {
		Bool fetch_input = GF_TRUE;

		//we failed at loading the init segment during a dash switch, retry
		if (!read->is_partial_download && !read->mem_load_mode && (read->moov_not_loaded==2) ) {
			isoffin_configure_pid(filter, read->pid, GF_FALSE);
			if (read->moov_not_loaded) return GF_OK;
		}
		if (read->mem_load_mode==2) {
			if (!read->force_fetch && read->mem_blob.size > read->mstore_size) {
				fetch_input = GF_FALSE;
			}
			read->force_fetch = GF_FALSE;
		}
		while (fetch_input) {
			GF_FilterPacket *pck = gf_filter_pid_get_packet(read->pid);
			if (!pck) {
				//we issued a seek, wait for the first packet to be received before fetching channels
				//otherwise we could end up reading from the wrong cache
				if (read->wait_for_source) {
					//something went wrong during the seek request
					if (gf_filter_pid_is_eos(read->pid))
						return GF_EOS;
					return GF_OK;
				}
				break;
			}
			read->wait_for_source = GF_FALSE;

			if (read->mem_load_mode) {
				u32 data_size;
				const u8 *pck_data = gf_filter_pck_get_data(pck, &data_size);
				isoffin_push_buffer(filter, read, pck_data, data_size);
			}
			//we just had a switch but init seg is not completely done: input packet is only a part of the init, drop it
			else if (read->moov_not_loaded==2) {
				gf_filter_pid_drop_packet(read->pid);
				return GF_OK;
			}
			gf_filter_pid_drop_packet(read->pid);
			has_new_data = GF_TRUE;
			if (read->in_error)
				return read->in_error;
		}
		if (gf_filter_pid_is_eos(read->pid)) {
			read->input_loaded = GF_TRUE;
			in_is_eos = GF_TRUE;
		}
		if (read->input_is_stop) {
			read->input_loaded = GF_TRUE;
			in_is_eos = GF_TRUE;
			read->input_is_stop = GF_FALSE;
		}
		if (!read->frag_type && read->input_loaded) {
			in_is_eos = GF_TRUE;
		}
        //segment is invalid, wait for eos on input an send eos on all channels
        if (read->invalid_segment) {
            if (!in_is_eos) return GF_OK;
            read->invalid_segment = GF_FALSE;

            for (i=0; i<count; i++) {
                ISOMChannel *ch = gf_list_get(read->channels, i);
                if (!ch->playing) {
                    continue;
                }
                if (!ch->eos_sent) {
                    ch->eos_sent = GF_TRUE;
                    gf_filter_pid_set_eos(ch->pid);
                }
            }
            read->eos_signaled = GF_TRUE;
            return GF_EOS;
        }
	} else if (read->extern_mov) {
		in_is_eos = GF_TRUE;
		read->input_loaded = GF_TRUE;
	}
	if (read->moov_not_loaded==1) {
		if (read->mem_load_mode)
			return GF_OK;
		read->moov_not_loaded = GF_FALSE;
		return isoffin_setup(filter, read);
	}

	if (read->refresh_fragmented) {
		const GF_PropertyValue *prop;

		if (in_is_eos) {
			read->refresh_fragmented = GF_FALSE;
		} else {
			prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILE_CACHED);
			if (prop && prop->value.boolean)
				read->refresh_fragmented = GF_FALSE;
		}

		if (has_new_data) {
			u64 bytesMissing=0;
			GF_Err e;
			const char *new_url = NULL;
			prop = gf_filter_pid_get_property(read->pid, GF_PROP_PID_FILEPATH);
			if (prop) new_url = prop->value.string;

			e = gf_isom_refresh_fragmented(read->mov, &bytesMissing, new_url);

			if (e && (e!= GF_ISOM_INCOMPLETE_FILE)) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("[IsoMedia] Failed to refresh current segment: %s\n", gf_error_to_string(e) ));
				read->refresh_fragmented = GF_FALSE;
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] Refreshing current segment at UTC "LLU" - "LLU" bytes still missing - input is EOS %d\n", gf_net_get_utc(), bytesMissing, in_is_eos));
			}

			if (!read->refresh_fragmented && (e==GF_ISOM_INCOMPLETE_FILE)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_DASH, ("[IsoMedia] Incomplete Segment received - "LLU" bytes missing but EOF found\n", bytesMissing ));
			}

#ifndef GPAC_DISABLE_LOG
			if (gf_log_tool_level_on(GF_LOG_DASH, GF_LOG_DEBUG)) {
				for (i=0; i<count; i++) {
					ISOMChannel *ch = gf_list_get(read->channels, i);
					GF_LOG(GF_LOG_DEBUG, GF_LOG_DASH, ("[IsoMedia] refresh track %d fragment - cur sample %d - new sample count %d\n", ch->track, ch->sample_num, gf_isom_get_sample_count(ch->owner->mov, ch->track) ));
				}
			}
#endif
			isor_check_producer_ref_time(read);
			if (!read->frag_type)
				read->refresh_fragmented = GF_FALSE;
		}
	}

	for (i=0; i<count; i++) {
		u8 *data;
		u32 nb_pck=50;
		ISOMChannel *ch;
		ch = gf_list_get(read->channels, i);
		if (!ch->playing) {
			nb_forced_end++;
			continue;
		}
		//eos not sent on this channel, we are active
		if (!ch->eos_sent)
			is_active = GF_TRUE;

		while (nb_pck) {
			ch->sample_data_offset = 0;
			if (!read->full_segment_flush && gf_filter_pid_would_block(ch->pid) )
				break;

			if (ch->item_id) {
				isor_reader_get_sample_from_item(ch);
			} else {
				isor_reader_get_sample(ch);
			}

			if (read->stsd && (ch->last_sample_desc_index != read->stsd) && ch->sample) {
				isor_reader_release_sample(ch);
				continue;
			}
			if (ch->sample) {
				u32 sample_dur;
				u8 dep_flags;
				u8 *subs_buf;
				u32 subs_buf_size;
				GF_FilterPacket *pck;
				if (ch->needs_pid_reconfig) {
					isor_update_channel_config(ch);
					ch->needs_pid_reconfig = GF_FALSE;
				}

				//we have at least two samples, update GF_PROP_PID_HAS_SYNC if needed
				if (ch->check_has_rap && (gf_isom_get_sample_count(ch->owner->mov, ch->track)>1) && (gf_isom_has_sync_points(ch->owner->mov, ch->track)==1)) {
					ch->check_has_rap = GF_FALSE;
					ch->has_rap = GF_TRUE;
					gf_filter_pid_set_property(ch->pid, GF_PROP_PID_HAS_SYNC, &PROP_BOOL(ch->has_rap) );
				}

				//strip param sets from payload, trigger reconfig if needed
				isor_reader_check_config(ch);

				if (read->nodata) {
					pck = gf_filter_pck_new_shared(ch->pid, NULL, ch->sample->dataLength, NULL);
					if (!pck) return GF_OUT_OF_MEM;
				} else {
					pck = gf_filter_pck_new_alloc(ch->pid, ch->sample->dataLength, &data);
					if (!pck) return GF_OUT_OF_MEM;

					memcpy(data, ch->sample->data, ch->sample->dataLength);
				}
				gf_filter_pck_set_dts(pck, ch->dts);
				gf_filter_pck_set_cts(pck, ch->cts);
				if (ch->sample->IsRAP==-1) {
					gf_filter_pck_set_sap(pck, GF_FILTER_SAP_1);
					ch->redundant = 1;
				} else {
					gf_filter_pck_set_sap(pck, (GF_FilterSAPType) ch->sample->IsRAP);
				}

				if (ch->sap_3)
					gf_filter_pck_set_sap(pck, GF_FILTER_SAP_3);
				else if (ch->sap_4_type) {
					gf_filter_pck_set_sap(pck, (ch->sap_4_type==GF_ISOM_SAMPLE_PREROLL) ? GF_FILTER_SAP_4_PROL : GF_FILTER_SAP_4);
					gf_filter_pck_set_roll_info(pck, ch->roll);
				}

				sample_dur = ch->au_duration;
				if (ch->sample->nb_pack)
					sample_dur *= ch->sample->nb_pack;
				gf_filter_pck_set_duration(pck, sample_dur);
				gf_filter_pck_set_seek_flag(pck, ch->seek_flag);

				//for now we only signal xPS mask for non-sap
				if (ch->xps_mask && !gf_filter_pck_get_sap(pck) ) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_XPS_MASK, &PROP_UINT(ch->xps_mask) );
				}

				dep_flags = ch->isLeading;
				dep_flags <<= 2;
				dep_flags |= ch->dependsOn;
				dep_flags <<= 2;
				dep_flags |= ch->dependedOn;
				dep_flags <<= 2;
				dep_flags |= ch->redundant;

				if (dep_flags)
					gf_filter_pck_set_dependency_flags(pck, dep_flags);

				gf_filter_pck_set_crypt_flags(pck, ch->pck_encrypted ? GF_FILTER_PCK_CRYPT : 0);
				gf_filter_pck_set_seq_num(pck, ch->sample_num);


				subs_buf = gf_isom_sample_get_subsamples_buffer(read->mov, ch->track, ch->sample_num, &subs_buf_size);
				if (subs_buf) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_SUBS, &PROP_DATA_NO_COPY(subs_buf, subs_buf_size) );
				}

				if (ch->sai_buffer && ch->pck_encrypted) {
					assert(ch->sai_buffer_size);
					gf_filter_pck_set_property(pck, GF_PROP_PCK_CENC_SAI, &PROP_DATA(ch->sai_buffer, ch->sai_buffer_size) );
				}

				if (read->sigfrag) {
					GF_ISOFragmentBoundaryInfo finfo;
					if (gf_isom_sample_is_fragment_start(read->mov, ch->track, ch->sample_num, &finfo) ) {
						u64 start=0;
						u32 traf_start = finfo.seg_start_plus_one ? 2 : 1;

						if (finfo.seg_start_plus_one)
							gf_filter_pck_set_property(pck, GF_PROP_PCK_CUE_START, &PROP_BOOL(GF_TRUE));

						gf_filter_pck_set_property(pck, GF_PROP_PCK_FRAG_START, &PROP_UINT(traf_start));

						start = finfo.frag_start;
						if (finfo.seg_start_plus_one) start = finfo.seg_start_plus_one-1;
						gf_filter_pck_set_property(pck, GF_PROP_PCK_FRAG_RANGE, &PROP_FRAC64_INT(start, finfo.mdat_end));
						if (finfo.moof_template) {
							gf_filter_pck_set_property(pck, GF_PROP_PCK_MOOF_TEMPLATE, &PROP_DATA((u8 *)finfo.moof_template, finfo.moof_template_size));
						}
						if (finfo.sidx_end) {
							gf_filter_pck_set_property(pck, GF_PROP_PCK_SIDX_RANGE, &PROP_FRAC64_INT(finfo.sidx_start , finfo.sidx_end));
						}

						if (read->seg_name_changed) {
							const GF_PropertyValue *p = gf_filter_pid_get_property(read->pid, GF_PROP_PID_URL);
							read->seg_name_changed = GF_FALSE;
							if (p && p->value.string) {
								gf_filter_pck_set_property(pck, GF_PROP_PID_URL, &PROP_STRING(p->value.string));
							}
						}
					}
				}
				if (ch->sender_ntp) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_SENDER_NTP, &PROP_LONGUINT(ch->sender_ntp));
					if (ch->ntp_at_server_ntp) {
						gf_filter_pck_set_property(pck, GF_PROP_PCK_RECEIVER_NTP, &PROP_LONGUINT(ch->ntp_at_server_ntp));
					}
				}
				ch->eos_sent = GF_FALSE;

				//this might not be the true end of stream
				if ((ch->streamType==GF_STREAM_AUDIO) && (ch->sample_num == gf_isom_get_sample_count(read->mov, ch->track))) {
					gf_filter_pck_set_property(pck, GF_PROP_PCK_END_RANGE, &PROP_BOOL(GF_TRUE));
				}

				gf_filter_pck_send(pck);
				isor_reader_release_sample(ch);

				ch->last_valid_sample_data_offset = ch->sample_data_offset;
				nb_pck--;
			} else if (ch->last_state==GF_EOS) {
				if (ch->playing == 2) {
					if (in_is_eos) {
						ch->playing = GF_FALSE;
					} else {
						nb_forced_end++;
						check_forced_end = GF_TRUE;
					}
				}
				if (in_is_eos && !ch->eos_sent) {
					void *tfrf;
					const void *gf_isom_get_tfrf(GF_ISOFile *movie, u32 trackNumber);

					ch->eos_sent = GF_TRUE;
					read->eos_signaled = GF_TRUE;

					tfrf = (void *) gf_isom_get_tfrf(read->mov, ch->track);
					if (tfrf) {
						gf_filter_pid_set_info_str(ch->pid, "smooth_tfrf", &PROP_POINTER(tfrf) );
						ch->last_has_tfrf = GF_TRUE;
					} else if (ch->last_has_tfrf) {
						gf_filter_pid_set_info_str(ch->pid, "smooth_tfrf", NULL);
						ch->last_has_tfrf = GF_FALSE;
					}

					gf_filter_pid_set_eos(ch->pid);
				}
				break;
			} else {
				read->force_fetch = GF_TRUE;
				break;
			}
		}
		if (!min_offset_plus_one || (min_offset_plus_one - 1 > ch->last_valid_sample_data_offset))
			min_offset_plus_one = 1 + ch->last_valid_sample_data_offset;
	}
	if (read->mem_load_mode && min_offset_plus_one) {
		isoffin_purge_mem(read, min_offset_plus_one-1);
	}

	//we reached end of playback due to play range request, we must send eos - however for safety reason with DASH, we first need to cancel the input
	if (read->pid && check_forced_end && (nb_forced_end==count)) {
		//abort input
		GF_FilterEvent evt;
		GF_FEVT_INIT(evt, GF_FEVT_STOP, read->pid);
		gf_filter_pid_send_event(read->pid, &evt);
	}


	if (!is_active) {
		return GF_EOS;
	}
	//if (in_is_eos)
//	gf_filter_ask_rt_reschedule(filter, 1);
	return GF_OK;

}