static s32 gf_hevc_read_pps_bs_internal(GF_BitStream *bs, HEVCState *hevc)
{
	u32 i;
	s32 pps_id;
	HEVC_PPS *pps;

	//NAL header already read
	pps_id = gf_bs_read_ue_log(bs, "pps_id");

	if ((pps_id < 0) || (pps_id >= 64)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] wrong PPS ID %d in PPS\n", pps_id));
		return -1;
	}
	pps = &hevc->pps[pps_id];

	if (!pps->state) {
		pps->id = pps_id;
		pps->state = 1;
	}
	pps->sps_id = gf_bs_read_ue_log(bs, "sps_id");
	if (pps->sps_id >= 16) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODING, ("[HEVC] wrong SPS ID %d in PPS\n", pps->sps_id));
		pps->sps_id=0;
		return -1;
	}
	hevc->sps_active_idx = pps->sps_id; /*set active sps*/
	pps->dependent_slice_segments_enabled_flag = gf_bs_read_int_log(bs, 1, "dependent_slice_segments_enabled_flag");

	pps->output_flag_present_flag = gf_bs_read_int_log(bs, 1, "output_flag_present_flag");
	pps->num_extra_slice_header_bits = gf_bs_read_int_log(bs, 3, "num_extra_slice_header_bits");
	pps->sign_data_hiding_flag = gf_bs_read_int_log(bs, 1, "sign_data_hiding_flag");
	pps->cabac_init_present_flag = gf_bs_read_int_log(bs, 1, "cabac_init_present_flag");
	pps->num_ref_idx_l0_default_active = 1 + gf_bs_read_ue_log(bs, "num_ref_idx_l0_default_active");
	pps->num_ref_idx_l1_default_active = 1 + gf_bs_read_ue_log(bs, "num_ref_idx_l1_default_active");
	pps->pic_init_qp_minus26 = gf_bs_read_se_log(bs, "pic_init_qp_minus26");
	pps->constrained_intra_pred_flag = gf_bs_read_int_log(bs, 1, "constrained_intra_pred_flag");
	pps->transform_skip_enabled_flag = gf_bs_read_int_log(bs, 1, "transform_skip_enabled_flag");
	if ((pps->cu_qp_delta_enabled_flag = gf_bs_read_int_log(bs, 1, "cu_qp_delta_enabled_flag")))
		pps->diff_cu_qp_delta_depth = gf_bs_read_ue_log(bs, "diff_cu_qp_delta_depth");

	pps->pic_cb_qp_offset = gf_bs_read_se_log(bs, "pic_cb_qp_offset");
	pps->pic_cr_qp_offset = gf_bs_read_se_log(bs, "pic_cr_qp_offset");
	pps->slice_chroma_qp_offsets_present_flag = gf_bs_read_int_log(bs, 1, "slice_chroma_qp_offsets_present_flag");
	pps->weighted_pred_flag = gf_bs_read_int_log(bs, 1, "weighted_pred_flag");
	pps->weighted_bipred_flag = gf_bs_read_int_log(bs, 1, "weighted_bipred_flag");
	pps->transquant_bypass_enable_flag = gf_bs_read_int_log(bs, 1, "transquant_bypass_enable_flag");
	pps->tiles_enabled_flag = gf_bs_read_int_log(bs, 1, "tiles_enabled_flag");
	pps->entropy_coding_sync_enabled_flag = gf_bs_read_int_log(bs, 1, "entropy_coding_sync_enabled_flag");
	if (pps->tiles_enabled_flag) {
		pps->num_tile_columns = 1 + gf_bs_read_ue_log(bs, "num_tile_columns_minus1");
		pps->num_tile_rows = 1 + gf_bs_read_ue_log(bs, "num_tile_rows_minus1");
		pps->uniform_spacing_flag = gf_bs_read_int_log(bs, 1, "uniform_spacing_flag");
		if (!pps->uniform_spacing_flag) {
			for (i = 0; i < pps->num_tile_columns - 1; i++) {
				pps->column_width[i] = 1 + gf_bs_read_ue_log_idx(bs, "column_width_minus1", i);
			}
			for (i = 0; i < pps->num_tile_rows - 1; i++) {
				pps->row_height[i] = 1 + gf_bs_read_ue_log_idx(bs, "row_height_minus1", i);
			}
		}
		pps->loop_filter_across_tiles_enabled_flag = gf_bs_read_int_log(bs, 1, "loop_filter_across_tiles_enabled_flag");
	}
	pps->loop_filter_across_slices_enabled_flag = gf_bs_read_int_log(bs, 1, "loop_filter_across_slices_enabled_flag");
	if ((pps->deblocking_filter_control_present_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_control_present_flag"))) {
		pps->deblocking_filter_override_enabled_flag = gf_bs_read_int_log(bs, 1, "deblocking_filter_override_enabled_flag");
		if (! (pps->pic_disable_deblocking_filter_flag = gf_bs_read_int_log(bs, 1, "pic_disable_deblocking_filter_flag"))) {
			pps->beta_offset_div2 = gf_bs_read_se_log(bs, "beta_offset_div2");
			pps->tc_offset_div2 = gf_bs_read_se_log(bs, "tc_offset_div2");
		}
	}
	if ((pps->pic_scaling_list_data_present_flag = gf_bs_read_int_log(bs, 1, "pic_scaling_list_data_present_flag"))) {
		hevc_scaling_list_data(bs);
	}
	pps->lists_modification_present_flag = gf_bs_read_int_log(bs, 1, "lists_modification_present_flag");
	pps->log2_parallel_merge_level_minus2 = gf_bs_read_ue_log(bs, "log2_parallel_merge_level_minus2");
	pps->slice_segment_header_extension_present_flag = gf_bs_read_int_log(bs, 1, "slice_segment_header_extension_present_flag");
	if (gf_bs_read_int_log(bs, 1, "pps_extension_flag")) {
#if 0
		while (gf_bs_available(bs)) {
			/*pps_extension_data_flag */ gf_bs_read_int(bs, 1);
		}
#endif

	}
	return pps_id;
}