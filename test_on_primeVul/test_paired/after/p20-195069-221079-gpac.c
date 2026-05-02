static s32 svc_parse_slice(GF_BitStream *bs, AVCState *avc, AVCSliceInfo *si)
{
	s32 pps_id;

	/*s->current_picture.reference= h->nal_ref_idc != 0;*/
	gf_bs_read_ue_log(bs, "first_mb_in_slice");
	si->slice_type = gf_bs_read_ue_log(bs, "slice_type");
	if (si->slice_type > 9) return -1;

	pps_id = gf_bs_read_ue_log(bs, "pps_id");
	if ((pps_id<0) || (pps_id > 255))
		return -1;
	si->pps = &avc->pps[pps_id];
	si->pps->id = pps_id;
	if (!si->pps->slice_group_count)
		return -2;
	si->sps = &avc->sps[si->pps->sps_id + GF_SVC_SSPS_ID_SHIFT];
	if (!si->sps->log2_max_frame_num)
		return -2;

	si->frame_num = gf_bs_read_int_log(bs, si->sps->log2_max_frame_num, "frame_num");

	si->field_pic_flag = 0;
	if (si->sps->frame_mbs_only_flag) {
		/*s->picture_structure= PICT_FRAME;*/
	}
	else {
		si->field_pic_flag = gf_bs_read_int_log(bs, 1, "field_pic_flag");
		if (si->field_pic_flag) si->bottom_field_flag = gf_bs_read_int_log(bs, 1, "bottom_field_flag");
	}
	if (si->nal_unit_type == GF_AVC_NALU_IDR_SLICE || si->NalHeader.idr_pic_flag)
		si->idr_pic_id = gf_bs_read_ue_log(bs, "idr_pic_id");

	if (si->sps->poc_type == 0) {
		si->poc_lsb = gf_bs_read_int_log(bs, si->sps->log2_max_poc_lsb, "poc_lsb");
		if (si->pps->pic_order_present && !si->field_pic_flag) {
			si->delta_poc_bottom = gf_bs_read_se_log(bs, "delta_poc_bottom");
		}
	}
	else if ((si->sps->poc_type == 1) && !si->sps->delta_pic_order_always_zero_flag) {
		si->delta_poc[0] = gf_bs_read_se_log(bs, "delta_poc0");
		if ((si->pps->pic_order_present == 1) && !si->field_pic_flag)
			si->delta_poc[1] = gf_bs_read_se_log(bs, "delta_poc1");
	}
	if (si->pps->redundant_pic_cnt_present) {
		si->redundant_pic_cnt = gf_bs_read_ue_log(bs, "redundant_pic_cnt");
	}
	return 0;
}