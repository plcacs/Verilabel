static s32 gf_avc_read_sps_bs_internal(GF_BitStream *bs, AVCState *avc, u32 subseq_sps, u32 *vui_flag_pos, u32 nal_hdr)
{
	AVC_SPS *sps;
	s32 mb_width, mb_height, sps_id = -1;
	u32 profile_idc, level_idc, pcomp, i, chroma_format_idc, cl = 0, cr = 0, ct = 0, cb = 0, luma_bd, chroma_bd;
	u8 separate_colour_plane_flag = 0;

	if (!vui_flag_pos) {
		gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);
	}

	if (!bs) {
		return -1;
	}

	if (!nal_hdr) {
		gf_bs_read_int_log(bs, 1, "forbidden_zero_bit");
		gf_bs_read_int_log(bs, 2, "nal_ref_idc");
		gf_bs_read_int_log(bs, 5, "nal_unit_type");
	}
	profile_idc = gf_bs_read_int_log(bs, 8, "profile_idc");

	pcomp = gf_bs_read_int_log(bs, 8, "profile_compatibility");
	/*sanity checks*/
	if (pcomp & 0x3)
		return -1;

	level_idc = gf_bs_read_int_log(bs, 8, "level_idc");

	/*SubsetSps is used to be sure that AVC SPS are not going to be scratched
	by subset SPS. According to the SVC standard, subset SPS can have the same sps_id
	than its base layer, but it does not refer to the same SPS. */
	sps_id = gf_bs_read_ue_log(bs, "sps_id") + GF_SVC_SSPS_ID_SHIFT * subseq_sps;
	if ((sps_id < 0) || (sps_id >= 32)) {
		return -1;
	}

	luma_bd = chroma_bd = 0;
	sps = &avc->sps[sps_id];
	chroma_format_idc = sps->ChromaArrayType = 1;
	sps->state |= subseq_sps ? AVC_SUBSPS_PARSED : AVC_SPS_PARSED;

	/*High Profile and SVC*/
	switch (profile_idc) {
	case 100:
	case 110:
	case 122:
	case 244:
	case 44:
		/*sanity checks: note1 from 7.4.2.1.1 of iso/iec 14496-10-N11084*/
		if (pcomp & 0xE0)
			return -1;
	case 83:
	case 86:
	case 118:
	case 128:
		chroma_format_idc = gf_bs_read_ue_log(bs, "chroma_format_idc");
		sps->ChromaArrayType = chroma_format_idc;
		if (chroma_format_idc == 3) {
			separate_colour_plane_flag = gf_bs_read_int_log(bs, 1, "separate_colour_plane_flag");
			/*
			Depending on the value of separate_colour_plane_flag, the value of the variable ChromaArrayType is assigned as follows.
			\96	If separate_colour_plane_flag is equal to 0, ChromaArrayType is set equal to chroma_format_idc.
			\96	Otherwise (separate_colour_plane_flag is equal to 1), ChromaArrayType is set equal to 0.
			*/
			if (separate_colour_plane_flag) sps->ChromaArrayType = 0;
		}
		luma_bd = gf_bs_read_ue_log(bs, "luma_bit_depth");
		chroma_bd = gf_bs_read_ue_log(bs, "chroma_bit_depth");
		/*qpprime_y_zero_transform_bypass_flag = */ gf_bs_read_int_log(bs, 1, "qpprime_y_zero_transform_bypass_flag");
		/*seq_scaling_matrix_present_flag*/
		if (gf_bs_read_int_log(bs, 1, "seq_scaling_matrix_present_flag")) {
			u32 k;
			for (k = 0; k < 8; k++) {
				if (gf_bs_read_int_log_idx(bs, 1, "seq_scaling_list_present_flag", k)) {
					u32 z, last = 8, next = 8;
					u32 sl = k < 6 ? 16 : 64;
					for (z = 0; z < sl; z++) {
						if (next) {
							s32 delta = gf_bs_read_se(bs);
							next = (last + delta + 256) % 256;
						}
						last = next ? next : last;
					}
				}
			}
		}
		break;
	}

	sps->profile_idc = profile_idc;
	sps->level_idc = level_idc;
	sps->prof_compat = pcomp;
	sps->log2_max_frame_num = gf_bs_read_ue_log(bs, "log2_max_frame_num") + 4;
	sps->poc_type = gf_bs_read_ue_log(bs, "poc_type");
	sps->chroma_format = chroma_format_idc;
	sps->luma_bit_depth_m8 = luma_bd;
	sps->chroma_bit_depth_m8 = chroma_bd;

	if (sps->poc_type == 0) {
		sps->log2_max_poc_lsb = gf_bs_read_ue_log(bs, "log2_max_poc_lsb") + 4;
	}
	else if (sps->poc_type == 1) {
		sps->delta_pic_order_always_zero_flag = gf_bs_read_int_log(bs, 1, "delta_pic_order_always_zero_flag");
		sps->offset_for_non_ref_pic = gf_bs_read_se_log(bs, "offset_for_non_ref_pic");
		sps->offset_for_top_to_bottom_field = gf_bs_read_se_log(bs, "offset_for_top_to_bottom_field");
		sps->poc_cycle_length = gf_bs_read_ue_log(bs, "poc_cycle_length");
		if (sps->poc_cycle_length > GF_ARRAY_LENGTH(sps->offset_for_ref_frame)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[avc-h264] offset_for_ref_frame overflow from poc_cycle_length\n"));
			return -1;
		}
		for (i = 0; i < sps->poc_cycle_length; i++)
			sps->offset_for_ref_frame[i] = gf_bs_read_se_log_idx(bs, "offset_for_ref_frame", i);
	}
	if (sps->poc_type > 2) {
		return -1;
	}
	sps->max_num_ref_frames = gf_bs_read_ue_log(bs, "max_num_ref_frames");
	sps->gaps_in_frame_num_value_allowed_flag = gf_bs_read_int_log(bs, 1, "gaps_in_frame_num_value_allowed_flag");
	mb_width = gf_bs_read_ue_log(bs, "pic_width_in_mbs_minus1") + 1;
	mb_height = gf_bs_read_ue_log(bs, "pic_height_in_map_units_minus1") + 1;

	sps->frame_mbs_only_flag = gf_bs_read_int_log(bs, 1, "frame_mbs_only_flag");

	sps->width = mb_width * 16;
	sps->height = (2 - sps->frame_mbs_only_flag) * mb_height * 16;

	if (!sps->frame_mbs_only_flag) sps->mb_adaptive_frame_field_flag = gf_bs_read_int_log(bs, 1, "mb_adaptive_frame_field_flag");
	gf_bs_read_int_log(bs, 1, "direct_8x8_inference_flag");

	if (gf_bs_read_int_log(bs, 1, "frame_cropping_flag")) {
		int CropUnitX, CropUnitY, SubWidthC = -1, SubHeightC = -1;

		if (chroma_format_idc == 1) {
			SubWidthC = 2; SubHeightC = 2;
		}
		else if (chroma_format_idc == 2) {
			SubWidthC = 2; SubHeightC = 1;
		}
		else if ((chroma_format_idc == 3) && (separate_colour_plane_flag == 0)) {
			SubWidthC = 1; SubHeightC = 1;
		}

		if (sps->ChromaArrayType == 0) {
			assert(SubWidthC == -1);
			CropUnitX = 1;
			CropUnitY = 2 - sps->frame_mbs_only_flag;
		}
		else {
			CropUnitX = SubWidthC;
			CropUnitY = SubHeightC * (2 - sps->frame_mbs_only_flag);
		}

		cl = gf_bs_read_ue_log(bs, "frame_crop_left_offset");
		cr = gf_bs_read_ue_log(bs, "frame_crop_right_offset");
		ct = gf_bs_read_ue_log(bs, "frame_crop_top_offset");
		cb = gf_bs_read_ue_log(bs, "frame_crop_bottom_offset");

		sps->width -= CropUnitX * (cl + cr);
		sps->height -= CropUnitY * (ct + cb);
		cl *= CropUnitX;
		cr *= CropUnitX;
		ct *= CropUnitY;
		cb *= CropUnitY;
	}
	sps->crop.left = cl;
	sps->crop.right = cr;
	sps->crop.top = ct;
	sps->crop.bottom = cb;

	if (vui_flag_pos) {
		*vui_flag_pos = (u32)gf_bs_get_bit_offset(bs);
	}
	/*vui_parameters_present_flag*/
	sps->vui_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_parameters_present_flag");
	if (sps->vui_parameters_present_flag) {
		sps->vui.aspect_ratio_info_present_flag = gf_bs_read_int_log(bs, 1, "aspect_ratio_info_present_flag");
		if (sps->vui.aspect_ratio_info_present_flag) {
			s32 aspect_ratio_idc = gf_bs_read_int_log(bs, 8, "aspect_ratio_idc");
			if (aspect_ratio_idc == 255) {
				sps->vui.par_num = gf_bs_read_int_log(bs, 16, "aspect_ratio_num");
				sps->vui.par_den = gf_bs_read_int_log(bs, 16, "aspect_ratio_den");
			}
			else if (aspect_ratio_idc < GF_ARRAY_LENGTH(avc_hevc_sar) ) {
				sps->vui.par_num = avc_hevc_sar[aspect_ratio_idc].w;
				sps->vui.par_den = avc_hevc_sar[aspect_ratio_idc].h;
			}
			else {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] Unknown aspect_ratio_idc: your video may have a wrong aspect ratio. Contact the GPAC team!\n"));
			}
		}
		sps->vui.overscan_info_present_flag = gf_bs_read_int_log(bs, 1, "overscan_info_present_flag");
		if (sps->vui.overscan_info_present_flag)
			gf_bs_read_int_log(bs, 1, "overscan_appropriate_flag");

		/* default values */
		sps->vui.video_format = 5;
		sps->vui.colour_primaries = 2;
		sps->vui.transfer_characteristics = 2;
		sps->vui.matrix_coefficients = 2;
		/* now read values if possible */
		sps->vui.video_signal_type_present_flag = gf_bs_read_int_log(bs, 1, "video_signal_type_present_flag");
		if (sps->vui.video_signal_type_present_flag) {
			sps->vui.video_format = gf_bs_read_int_log(bs, 3, "video_format");
			sps->vui.video_full_range_flag = gf_bs_read_int_log(bs, 1, "video_full_range_flag");
			sps->vui.colour_description_present_flag = gf_bs_read_int_log(bs, 1, "colour_description_present_flag");
			if (sps->vui.colour_description_present_flag) {
				sps->vui.colour_primaries = gf_bs_read_int_log(bs, 8, "colour_primaries");
				sps->vui.transfer_characteristics = gf_bs_read_int_log(bs, 8, "transfer_characteristics");
				sps->vui.matrix_coefficients = gf_bs_read_int_log(bs, 8, "matrix_coefficients");
			}
		}

		if (gf_bs_read_int_log(bs, 1, "chroma_location_info_present_flag")) {
			gf_bs_read_ue_log(bs, "chroma_sample_location_type_top_field");
			gf_bs_read_ue_log(bs, "chroma_sample_location_type_bottom_field");
		}

		sps->vui.timing_info_present_flag = gf_bs_read_int_log(bs, 1, "timing_info_present_flag");
		if (sps->vui.timing_info_present_flag) {
			sps->vui.num_units_in_tick = gf_bs_read_int_log(bs, 32, "num_units_in_tick");
			sps->vui.time_scale = gf_bs_read_int_log(bs, 32, "time_scale");
			sps->vui.fixed_frame_rate_flag = gf_bs_read_int_log(bs, 1, "fixed_frame_rate_flag");
		}

		sps->vui.nal_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "nal_hrd_parameters_present_flag");
		if (sps->vui.nal_hrd_parameters_present_flag)
			avc_parse_hrd_parameters(bs, &sps->vui.hrd);

		sps->vui.vcl_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vcl_hrd_parameters_present_flag");
		if (sps->vui.vcl_hrd_parameters_present_flag)
			avc_parse_hrd_parameters(bs, &sps->vui.hrd);

		if (sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag)
			sps->vui.low_delay_hrd_flag = gf_bs_read_int_log(bs, 1, "low_delay_hrd_flag");

		sps->vui.pic_struct_present_flag = gf_bs_read_int_log(bs, 1, "pic_struct_present_flag");
	}
	/*end of seq_parameter_set_data*/

	if (subseq_sps) {
		if ((profile_idc == 83) || (profile_idc == 86)) {
			u8 extended_spatial_scalability_idc;
			/*parsing seq_parameter_set_svc_extension*/

			gf_bs_read_int_log(bs, 1, "inter_layer_deblocking_filter_control_present_flag");
			extended_spatial_scalability_idc = gf_bs_read_int_log(bs, 2, "extended_spatial_scalability_idc");
			if (sps->ChromaArrayType == 1 || sps->ChromaArrayType == 2) {
				gf_bs_read_int_log(bs, 1, "chroma_phase_x_plus1_flag");
			}
			if (sps->ChromaArrayType == 1) {
				gf_bs_read_int_log(bs, 2, "chroma_phase_y_plus1");
			}
			if (extended_spatial_scalability_idc == 1) {
				if (sps->ChromaArrayType > 0) {
					gf_bs_read_int_log(bs, 1, "seq_ref_layer_chroma_phase_x_plus1_flag");
					gf_bs_read_int_log(bs, 2, "seq_ref_layer_chroma_phase_y_plus1");
				}
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_left_offset");
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_top_offset");
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_right_offset");
				gf_bs_read_se_log(bs, "seq_scaled_ref_layer_bottom_offset");
			}
			if (gf_bs_read_int_log(bs, 1, "seq_tcoeff_level_prediction_flag")) {
				gf_bs_read_int_log(bs, 1, "adaptive_tcoeff_level_prediction_flag");
			}
			gf_bs_read_int_log(bs, 1, "slice_header_restriction_flag");

			if (gf_bs_read_int_log(bs, 1, "svc_vui_parameters_present")) {
				u32 vui_ext_num_entries_minus1 = gf_bs_read_ue_log(bs, "vui_ext_num_entries_minus1");

				for (i = 0; i <= vui_ext_num_entries_minus1; i++) {
					u8 vui_ext_nal_hrd_parameters_present_flag, vui_ext_vcl_hrd_parameters_present_flag, vui_ext_timing_info_present_flag;
					gf_bs_read_int_log(bs, 3, "vui_ext_dependency_id");
					gf_bs_read_int_log(bs, 4, "vui_ext_quality_id");
					gf_bs_read_int_log(bs, 3, "vui_ext_temporal_id");
					vui_ext_timing_info_present_flag = gf_bs_read_int_log(bs, 1, "vui_ext_timing_info_present_flag");
					if (vui_ext_timing_info_present_flag) {
						gf_bs_read_int_log(bs, 32, "vui_ext_num_units_in_tick");
						gf_bs_read_int_log(bs, 32, "vui_ext_time_scale");
						gf_bs_read_int_log(bs, 1, "vui_ext_fixed_frame_rate_flag");
					}
					vui_ext_nal_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_ext_nal_hrd_parameters_present_flag");
					if (vui_ext_nal_hrd_parameters_present_flag) {
						//hrd_parameters( )
					}
					vui_ext_vcl_hrd_parameters_present_flag = gf_bs_read_int_log(bs, 1, "vui_ext_vcl_hrd_parameters_present_flag");
					if (vui_ext_vcl_hrd_parameters_present_flag) {
						//hrd_parameters( )
					}
					if (vui_ext_nal_hrd_parameters_present_flag || vui_ext_vcl_hrd_parameters_present_flag) {
						gf_bs_read_int_log(bs, 1, "vui_ext_low_delay_hrd_flag");
					}
					gf_bs_read_int_log(bs, 1, "vui_ext_pic_struct_present_flag");
				}
			}
		}
		else if ((profile_idc == 118) || (profile_idc == 128)) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CODING, ("[avc-h264] MVC parsing not implemented - skipping parsing end of Subset SPS\n"));
			return sps_id;
		}

		if (gf_bs_read_int_log(bs, 1, "additional_extension2")) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODING, ("[avc-h264] skipping parsing end of Subset SPS (additional_extension2)\n"));
			return sps_id;
		}
	}
	return sps_id;
}