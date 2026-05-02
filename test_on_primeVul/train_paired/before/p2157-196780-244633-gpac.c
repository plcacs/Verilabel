static void gf_m2ts_process_pmt(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *pmt, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status)
{
	u32 info_length, pos, desc_len, evt_type, nb_es,i;
	u32 nb_sections;
	u32 data_size;
	u32 nb_hevc, nb_hevc_temp, nb_shvc, nb_shvc_temp, nb_mhvc, nb_mhvc_temp;
	unsigned char *data;
	GF_M2TS_Section *section;
	GF_Err e = GF_OK;

	/*wait for the last section */
	if (!(status&GF_M2TS_TABLE_END)) return;

	nb_es = 0;

	/*skip if already received but no update detected (eg same data) */
	if ((status&GF_M2TS_TABLE_REPEAT) && !(status&GF_M2TS_TABLE_UPDATE))  {
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PMT_REPEAT, pmt->program);
		return;
	}

	if (pmt->sec->demux_restarted) {
		pmt->sec->demux_restarted = 0;
		return;
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] PMT Found or updated\n"));

	nb_sections = gf_list_count(sections);
	if (nb_sections > 1) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("PMT on multiple sections not supported\n"));
	}

	section = (GF_M2TS_Section *)gf_list_get(sections, 0);
	data = section->data;
	data_size = section->data_size;

	pmt->program->pcr_pid = ((data[0] & 0x1f) << 8) | data[1];

	info_length = ((data[2]&0xf)<<8) | data[3];
	if (info_length != 0) {
		/* ...Read Descriptors ... */
		u8 tag, len;
		u32 first_loop_len = 0;
		tag = data[4];
		len = data[5];
		while (info_length > first_loop_len) {
			if (tag == GF_M2TS_MPEG4_IOD_DESCRIPTOR) {
				u32 size;
				GF_BitStream *iod_bs;
				iod_bs = gf_bs_new((char *)data+8, len-2, GF_BITSTREAM_READ);
				if (pmt->program->pmt_iod) gf_odf_desc_del((GF_Descriptor *)pmt->program->pmt_iod);
				e = gf_odf_parse_descriptor(iod_bs , (GF_Descriptor **) &pmt->program->pmt_iod, &size);
				gf_bs_del(iod_bs );
				if (e==GF_OK) {
					/*remember program number for service/program selection*/
					if (pmt->program->pmt_iod) pmt->program->pmt_iod->ServiceID = pmt->program->number;
					/*if empty IOD (freebox case), discard it and use dynamic declaration of object*/
					if (!gf_list_count(pmt->program->pmt_iod->ESDescriptors)) {
						gf_odf_desc_del((GF_Descriptor *)pmt->program->pmt_iod);
						pmt->program->pmt_iod = NULL;
					}
				}
			} else if (tag == GF_M2TS_METADATA_POINTER_DESCRIPTOR) {
				GF_BitStream *metadatapd_bs;
				GF_M2TS_MetadataPointerDescriptor *metapd;
				metadatapd_bs = gf_bs_new((char *)data+6, len, GF_BITSTREAM_READ);
				metapd = gf_m2ts_read_metadata_pointer_descriptor(metadatapd_bs, len);
				gf_bs_del(metadatapd_bs);
				if (metapd->application_format_identifier == GF_M2TS_META_ID3 &&
				        metapd->format_identifier == GF_M2TS_META_ID3 &&
				        metapd->carriage_flag == METADATA_CARRIAGE_SAME_TS) {
					/*HLS ID3 Metadata */
					pmt->program->metadata_pointer_descriptor = metapd;
				} else {
					/* don't know what to do with it for now, delete */
					gf_m2ts_metadata_pointer_descriptor_del(metapd);
				}
			} else {
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] Skipping descriptor (0x%x) and others not supported\n", tag));
			}
			first_loop_len += 2 + len;
		}
	}
	if (data_size <= 4 + info_length) return;
	data += 4 + info_length;
	data_size -= 4 + info_length;
	pos = 0;

	/* count de number of program related PMT received */
	for(i=0; i<gf_list_count(ts->programs); i++) {
		GF_M2TS_Program *prog = (GF_M2TS_Program *)gf_list_get(ts->programs,i);
		if(prog->pmt_pid == pmt->pid) {
			break;
		}
	}

	nb_hevc = nb_hevc_temp = nb_shvc = nb_shvc_temp = nb_mhvc = nb_mhvc_temp = 0;
	while (pos<data_size) {
		GF_M2TS_PES *pes = NULL;
		GF_M2TS_SECTION_ES *ses = NULL;
		GF_M2TS_ES *es = NULL;
		Bool inherit_pcr = 0;
		u32 pid, stream_type, reg_desc_format;

		stream_type = data[0];
		pid = ((data[1] & 0x1f) << 8) | data[2];
		desc_len = ((data[3] & 0xf) << 8) | data[4];

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("stream_type :%d \n",stream_type));
		switch (stream_type) {

		/* PES */
		case GF_M2TS_VIDEO_MPEG1:
		case GF_M2TS_VIDEO_MPEG2:
		case GF_M2TS_VIDEO_DCII:
		case GF_M2TS_VIDEO_MPEG4:
		case GF_M2TS_SYSTEMS_MPEG4_PES:
		case GF_M2TS_VIDEO_H264:
		case GF_M2TS_VIDEO_SVC:
		case GF_M2TS_VIDEO_MVCD:
		case GF_M2TS_VIDEO_HEVC:
		case GF_M2TS_VIDEO_HEVC_MCTS:
		case GF_M2TS_VIDEO_HEVC_TEMPORAL:
		case GF_M2TS_VIDEO_SHVC:
		case GF_M2TS_VIDEO_SHVC_TEMPORAL:
		case GF_M2TS_VIDEO_MHVC:
		case GF_M2TS_VIDEO_MHVC_TEMPORAL:
			inherit_pcr = 1;
		case GF_M2TS_AUDIO_MPEG1:
		case GF_M2TS_AUDIO_MPEG2:
		case GF_M2TS_AUDIO_AAC:
		case GF_M2TS_AUDIO_LATM_AAC:
		case GF_M2TS_AUDIO_AC3:
		case GF_M2TS_AUDIO_DTS:
		case GF_M2TS_MHAS_MAIN:
		case GF_M2TS_MHAS_AUX:
		case GF_M2TS_SUBTITLE_DVB:
		case GF_M2TS_METADATA_PES:
			GF_SAFEALLOC(pes, GF_M2TS_PES);
			if (!pes) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			pes->cc = -1;
			pes->flags = GF_M2TS_ES_IS_PES;
			if (inherit_pcr)
				pes->flags |= GF_M2TS_INHERIT_PCR;
			es = (GF_M2TS_ES *)pes;
			break;
		case GF_M2TS_PRIVATE_DATA:
			GF_SAFEALLOC(pes, GF_M2TS_PES);
			if (!pes) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			pes->cc = -1;
			pes->flags = GF_M2TS_ES_IS_PES;
			es = (GF_M2TS_ES *)pes;
			break;
		/* Sections */
		case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
			GF_SAFEALLOC(ses, GF_M2TS_SECTION_ES);
			if (!ses) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			es = (GF_M2TS_ES *)ses;
			es->flags |= GF_M2TS_ES_IS_SECTION;
			/* carriage of ISO_IEC_14496 data in sections */
			if (stream_type == GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
				/*MPEG-4 sections need to be fully checked: if one section is lost, this means we lost
				one SL packet in the AU so we must wait for the complete section again*/
				ses->sec = gf_m2ts_section_filter_new(gf_m2ts_process_mpeg4section, 0);
				/*create OD container*/
				if (!pmt->program->additional_ods) {
					pmt->program->additional_ods = gf_list_new();
					ts->has_4on2 = 1;
				}
			}
			break;

		case GF_M2TS_13818_6_ANNEX_A:
		case GF_M2TS_13818_6_ANNEX_B:
		case GF_M2TS_13818_6_ANNEX_C:
		case GF_M2TS_13818_6_ANNEX_D:
		case GF_M2TS_PRIVATE_SECTION:
		case GF_M2TS_QUALITY_SEC:
		case GF_M2TS_MORE_SEC:
			GF_SAFEALLOC(ses, GF_M2TS_SECTION_ES);
			if (!ses) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG2TS] Failed to allocate ES for pid %d\n", pid));
				return;
			}
			es = (GF_M2TS_ES *)ses;
			es->flags |= GF_M2TS_ES_IS_SECTION;
			es->pid = pid;
			es->service_id = pmt->program->number;
			if (stream_type == GF_M2TS_PRIVATE_SECTION) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("AIT sections on pid %d\n", pid));
			} else if (stream_type == GF_M2TS_QUALITY_SEC) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Quality metadata sections on pid %d\n", pid));
			} else if (stream_type == GF_M2TS_MORE_SEC) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("MORE sections on pid %d\n", pid));
			} else {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("stream type DSM CC user private sections on pid %d \n", pid));
			}
			/* NULL means: trigger the call to on_event with DVB_GENERAL type and the raw section as payload */
			ses->sec = gf_m2ts_section_filter_new(NULL, 1);
			//ses->sec->service_id = pmt->program->number;
			break;

		case GF_M2TS_MPE_SECTIONS:
			if (! ts->prefix_present) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("stream type MPE found : pid = %d \n", pid));
#ifdef GPAC_ENABLE_MPE
				es = gf_dvb_mpe_section_new();
				if (es->flags & GF_M2TS_ES_IS_SECTION) {
					/* NULL means: trigger the call to on_event with DVB_GENERAL type and the raw section as payload */
					((GF_M2TS_SECTION_ES*)es)->sec = gf_m2ts_section_filter_new(NULL, 1);
				}
#endif
				break;
			}

		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS] Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
			//GF_LOG(/*GF_LOG_WARNING*/GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
			break;
		}

		if (es) {
			es->stream_type = (stream_type==GF_M2TS_PRIVATE_DATA) ? 0 : stream_type;
			es->program = pmt->program;
			es->pid = pid;
			es->component_tag = -1;
		}

		pos += 5;
		data += 5;

		while (desc_len) {
			u8 tag = data[0];
			u32 len = data[1];
			if (es) {
				switch (tag) {
				case GF_M2TS_ISO_639_LANGUAGE_DESCRIPTOR:
					if (pes)
						pes->lang = GF_4CC(' ', data[2], data[3], data[4]);
					break;
				case GF_M2TS_MPEG4_SL_DESCRIPTOR:
					es->mpeg4_es_id = ( (u32) data[2] & 0x1f) << 8  | data[3];
					es->flags |= GF_M2TS_ES_IS_SL;
					break;
				case GF_M2TS_REGISTRATION_DESCRIPTOR:
					reg_desc_format = GF_4CC(data[2], data[3], data[4], data[5]);
					/*cf http://www.smpte-ra.org/mpegreg/mpegreg.html*/
					switch (reg_desc_format) {
					case GF_M2TS_RA_STREAM_AC3:
						es->stream_type = GF_M2TS_AUDIO_AC3;
						break;
					case GF_M2TS_RA_STREAM_VC1:
						es->stream_type = GF_M2TS_VIDEO_VC1;
						break;
					case GF_M2TS_RA_STREAM_GPAC:
						if (len==8) {
							es->stream_type = GF_4CC(data[6], data[7], data[8], data[9]);
							es->flags |= GF_M2TS_GPAC_CODEC_ID;
							break;
						}
					default:
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("Unknown registration descriptor %s\n", gf_4cc_to_str(reg_desc_format) ));
						break;
					}
					break;
				case GF_M2TS_DVB_EAC3_DESCRIPTOR:
					es->stream_type = GF_M2TS_AUDIO_EC3;
					break;
				case GF_M2TS_DVB_DATA_BROADCAST_ID_DESCRIPTOR:
				{
					u32 id = data[2]<<8 | data[3];
					if ((id == 0xB) && ses && !ses->sec) {
						ses->sec = gf_m2ts_section_filter_new(NULL, 1);
					}
				}
				break;
				case GF_M2TS_DVB_SUBTITLING_DESCRIPTOR:
					if (pes) {
						pes->sub.language[0] = data[2];
						pes->sub.language[1] = data[3];
						pes->sub.language[2] = data[4];
						pes->sub.type = data[5];
						pes->sub.composition_page_id = (data[6]<<8) | data[7];
						pes->sub.ancillary_page_id = (data[8]<<8) | data[9];
					}
					es->stream_type = GF_M2TS_DVB_SUBTITLE;
					break;
				case GF_M2TS_DVB_STREAM_IDENTIFIER_DESCRIPTOR:
				{
					es->component_tag = data[2];
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("Component Tag: %d on Program %d\n", es->component_tag, es->program->number));
				}
				break;
				case GF_M2TS_DVB_TELETEXT_DESCRIPTOR:
					es->stream_type = GF_M2TS_DVB_TELETEXT;
					break;
				case GF_M2TS_DVB_VBI_DATA_DESCRIPTOR:
					es->stream_type = GF_M2TS_DVB_VBI;
					break;
				case GF_M2TS_HIERARCHY_DESCRIPTOR:
					if (pes) {
						u8 hierarchy_embedded_layer_index;
						GF_BitStream *hbs = gf_bs_new((const char *)data, data_size, GF_BITSTREAM_READ);
						/*u32 skip = */gf_bs_read_int(hbs, 16);
						/*u8 res1 = */gf_bs_read_int(hbs, 1);
						/*u8 temp_scal = */gf_bs_read_int(hbs, 1);
						/*u8 spatial_scal = */gf_bs_read_int(hbs, 1);
						/*u8 quality_scal = */gf_bs_read_int(hbs, 1);
						/*u8 hierarchy_type = */gf_bs_read_int(hbs, 4);
						/*u8 res2 = */gf_bs_read_int(hbs, 2);
						/*u8 hierarchy_layer_index = */gf_bs_read_int(hbs, 6);
						/*u8 tref_not_present = */gf_bs_read_int(hbs, 1);
						/*u8 res3 = */gf_bs_read_int(hbs, 1);
						hierarchy_embedded_layer_index = gf_bs_read_int(hbs, 6);
						/*u8 res4 = */gf_bs_read_int(hbs, 2);
						/*u8 hierarchy_channel = */gf_bs_read_int(hbs, 6);
						gf_bs_del(hbs);

						pes->depends_on_pid = 1+hierarchy_embedded_layer_index;
					}
					break;
				case GF_M2TS_METADATA_DESCRIPTOR:
				{
					GF_BitStream *metadatad_bs;
					GF_M2TS_MetadataDescriptor *metad;
					metadatad_bs = gf_bs_new((char *)data+2, len, GF_BITSTREAM_READ);
					metad = gf_m2ts_read_metadata_descriptor(metadatad_bs, len);
					gf_bs_del(metadatad_bs);
					if (metad->application_format_identifier == GF_M2TS_META_ID3 &&
					        metad->format_identifier == GF_M2TS_META_ID3) {
						/*HLS ID3 Metadata */
						if (pes) {
							pes->metadata_descriptor = metad;
							pes->stream_type = GF_M2TS_METADATA_ID3_HLS;
						}
					} else {
						/* don't know what to do with it for now, delete */
						gf_m2ts_metadata_descriptor_del(metad);
					}
				}
				break;

				default:
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS] skipping descriptor (0x%x) not supported\n", tag));
					break;
				}
			}

			data += len+2;
			pos += len+2;
			if (desc_len < len+2) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Invalid PMT es descriptor size for PID %d\n", pid ) );
				break;
			}
			desc_len-=len+2;
		}

		if (es && !es->stream_type) {
			gf_free(es);
			es = NULL;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS] Private Stream type (0x%x) for PID %d not supported\n", stream_type, pid ) );
		}

		if (!es) continue;

		if (ts->ess[pid]) {
			//this is component reuse across programs, overwrite the previously declared stream ...
			if (status & GF_M2TS_TABLE_FOUND) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS] PID %d reused across programs %d and %d, not completely supported\n", pid, ts->ess[pid]->program->number, es->program->number ) );

				//add stream to program but don't reassign the pid table until the stream is playing (>GF_M2TS_PES_FRAMING_SKIP)
				gf_list_add(pmt->program->streams, es);
				if (!(es->flags & GF_M2TS_ES_IS_SECTION) ) gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);

				nb_es++;
				//skip assignment below
				es = NULL;
			}
			/*watchout for pmt update - FIXME this likely won't work in most cases*/
			else {

				GF_M2TS_ES *o_es = ts->ess[es->pid];

				if ((o_es->stream_type == es->stream_type)
				        && ((o_es->flags & GF_M2TS_ES_STATIC_FLAGS_MASK) == (es->flags & GF_M2TS_ES_STATIC_FLAGS_MASK))
				        && (o_es->mpeg4_es_id == es->mpeg4_es_id)
				        && ((o_es->flags & GF_M2TS_ES_IS_SECTION) || ((GF_M2TS_PES *)o_es)->lang == ((GF_M2TS_PES *)es)->lang)
				   ) {
					gf_free(es);
					es = NULL;
				} else {
					gf_m2ts_es_del(o_es, ts);
					ts->ess[es->pid] = NULL;
				}
			}
		}

		if (es) {
			ts->ess[es->pid] = es;
			gf_list_add(pmt->program->streams, es);
			if (!(es->flags & GF_M2TS_ES_IS_SECTION) ) gf_m2ts_set_pes_framing(pes, GF_M2TS_PES_FRAMING_SKIP);

			nb_es++;
		}

		if (es->stream_type == GF_M2TS_VIDEO_HEVC) nb_hevc++;
		else if (es->stream_type == GF_M2TS_VIDEO_HEVC_TEMPORAL) nb_hevc_temp++;
		else if (es->stream_type == GF_M2TS_VIDEO_SHVC) nb_shvc++;
		else if (es->stream_type == GF_M2TS_VIDEO_SHVC_TEMPORAL) nb_shvc_temp++;
		else if (es->stream_type == GF_M2TS_VIDEO_MHVC) nb_mhvc++;
		else if (es->stream_type == GF_M2TS_VIDEO_MHVC_TEMPORAL) nb_mhvc_temp++;
	}

	//Table 2-139, implied hierarchy indexes
	if (nb_hevc_temp + nb_shvc + nb_shvc_temp + nb_mhvc+ nb_mhvc_temp) {
		for (i=0; i<gf_list_count(pmt->program->streams); i++) {
			GF_M2TS_PES *es = (GF_M2TS_PES *)gf_list_get(pmt->program->streams, i);
			if ( !(es->flags & GF_M2TS_ES_IS_PES)) continue;
			if (es->depends_on_pid) continue;

			switch (es->stream_type) {
			case GF_M2TS_VIDEO_HEVC_TEMPORAL:
				es->depends_on_pid = 1;
				break;
			case GF_M2TS_VIDEO_SHVC:
				if (!nb_hevc_temp) es->depends_on_pid = 1;
				else es->depends_on_pid = 2;
				break;
			case GF_M2TS_VIDEO_SHVC_TEMPORAL:
				es->depends_on_pid = 3;
				break;
			case GF_M2TS_VIDEO_MHVC:
				if (!nb_hevc_temp) es->depends_on_pid = 1;
				else es->depends_on_pid = 2;
				break;
			case GF_M2TS_VIDEO_MHVC_TEMPORAL:
				if (!nb_hevc_temp) es->depends_on_pid = 2;
				else es->depends_on_pid = 3;
				break;
			}
		}
	}

	if (nb_es) {
		u32 i;

		//translate hierarchy descriptors indexes into PIDs - check whether the PMT-index rules are the same for HEVC
		for (i=0; i<gf_list_count(pmt->program->streams); i++) {
			GF_M2TS_PES *an_es = NULL;
			GF_M2TS_PES *es = (GF_M2TS_PES *)gf_list_get(pmt->program->streams, i);
			if ( !(es->flags & GF_M2TS_ES_IS_PES)) continue;
			if (!es->depends_on_pid) continue;

			//fixeme we are not always assured that hierarchy_layer_index matches the stream index...
			//+1 is because our first stream is the PMT
			an_es =  (GF_M2TS_PES *)gf_list_get(pmt->program->streams, es->depends_on_pid);
			if (an_es) {
				es->depends_on_pid = an_es->pid;
			} else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[M2TS] Wrong dependency index in hierarchy descriptor, assuming non-scalable stream\n"));
				es->depends_on_pid = 0;
			}
		}

		evt_type = (status&GF_M2TS_TABLE_FOUND) ? GF_M2TS_EVT_PMT_FOUND : GF_M2TS_EVT_PMT_UPDATE;
		if (ts->on_event) ts->on_event(ts, evt_type, pmt->program);
	} else {
		/* if we found no new ES it's simply a repeat of the PMT */
		if (ts->on_event) ts->on_event(ts, GF_M2TS_EVT_PMT_REPEAT, pmt->program);
	}
}