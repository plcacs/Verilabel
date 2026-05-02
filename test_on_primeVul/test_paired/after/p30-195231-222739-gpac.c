s32 gf_avc_parse_nalu(GF_BitStream *bs, AVCState *avc)
{
	u8 idr_flag;
	s32 slice, ret;
	u32 nal_hdr;
	AVCSliceInfo n_state;

	gf_bs_enable_emulation_byte_removal(bs, GF_TRUE);

	nal_hdr = gf_bs_read_u8(bs);

	slice = 0;
	memcpy(&n_state, &avc->s_info, sizeof(AVCSliceInfo));
	avc->last_nal_type_parsed = n_state.nal_unit_type = nal_hdr & 0x1F;
	n_state.nal_ref_idc = (nal_hdr >> 5) & 0x3;

	idr_flag = 0;

	switch (n_state.nal_unit_type) {
	case GF_AVC_NALU_ACCESS_UNIT:
	case GF_AVC_NALU_END_OF_SEQ:
	case GF_AVC_NALU_END_OF_STREAM:
		ret = 1;
		break;

	case GF_AVC_NALU_SVC_SLICE:
		SVC_ReadNal_header_extension(bs, &n_state.NalHeader);
		// slice buffer - read the info and compare.
		/*ret = */svc_parse_slice(bs, avc, &n_state);
		if (avc->s_info.nal_ref_idc) {
			n_state.poc_lsb_prev = avc->s_info.poc_lsb;
			n_state.poc_msb_prev = avc->s_info.poc_msb;
		}
		avc_compute_poc(&n_state);

		if (avc->s_info.poc != n_state.poc) {
			memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
			return 1;
		}
		memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
		return 0;

	case GF_AVC_NALU_SVC_PREFIX_NALU:
		SVC_ReadNal_header_extension(bs, &n_state.NalHeader);
		return 0;

	case GF_AVC_NALU_IDR_SLICE:
	case GF_AVC_NALU_NON_IDR_SLICE:
	case GF_AVC_NALU_DP_A_SLICE:
	case GF_AVC_NALU_DP_B_SLICE:
	case GF_AVC_NALU_DP_C_SLICE:
		slice = 1;
		/* slice buffer - read the info and compare.*/
		ret = avc_parse_slice(bs, avc, idr_flag, &n_state);
		if (ret < 0) return ret;
		ret = 0;
		if (
			((avc->s_info.nal_unit_type > GF_AVC_NALU_IDR_SLICE) || (avc->s_info.nal_unit_type < GF_AVC_NALU_NON_IDR_SLICE))
			&& (avc->s_info.nal_unit_type != GF_AVC_NALU_SVC_SLICE)
			) {
			break;
		}
		if (avc->s_info.frame_num != n_state.frame_num) {
			ret = 1;
			break;
		}

		if (avc->s_info.field_pic_flag != n_state.field_pic_flag) {
			ret = 1;
			break;
		}
		if ((avc->s_info.nal_ref_idc != n_state.nal_ref_idc) &&
			(!avc->s_info.nal_ref_idc || !n_state.nal_ref_idc)) {
			ret = 1;
			break;
		}
		if (!avc->s_info.sps)
			return -1;

		if (avc->s_info.sps->poc_type == n_state.sps->poc_type) {
			if (!avc->s_info.sps->poc_type) {
				if (!n_state.bottom_field_flag && (avc->s_info.poc_lsb != n_state.poc_lsb)) {
					ret = 1;
					break;
				}
				if (avc->s_info.delta_poc_bottom != n_state.delta_poc_bottom) {
					ret = 1;
					break;
				}
			}
			else if (avc->s_info.sps->poc_type == 1) {
				if (avc->s_info.delta_poc[0] != n_state.delta_poc[0]) {
					ret = 1;
					break;
				}
				if (avc->s_info.delta_poc[1] != n_state.delta_poc[1]) {
					ret = 1;
					break;
				}
			}
		}

		if (n_state.nal_unit_type == GF_AVC_NALU_IDR_SLICE) {
			if (avc->s_info.nal_unit_type != GF_AVC_NALU_IDR_SLICE) { /*IdrPicFlag differs in value*/
				ret = 1;
				break;
			}
			else if (avc->s_info.idr_pic_id != n_state.idr_pic_id) { /*both IDR and idr_pic_id differs*/
				ret = 1;
				break;
			}
		}
		break;
	case GF_AVC_NALU_SEQ_PARAM:
		avc->last_ps_idx = gf_avc_read_sps_bs_internal(bs, avc, 0, NULL, nal_hdr);
		if (avc->last_ps_idx < 0) return -1;
		return 0;

	case GF_AVC_NALU_PIC_PARAM:
		avc->last_ps_idx = gf_avc_read_pps_bs_internal(bs, avc, nal_hdr);
		if (avc->last_ps_idx < 0) return -1;
		return 0;
	case GF_AVC_NALU_SVC_SUBSEQ_PARAM:
		avc->last_ps_idx = gf_avc_read_sps_bs_internal(bs, avc, 1, NULL, nal_hdr);
		if (avc->last_ps_idx < 0) return -1;
		return 0;
	case GF_AVC_NALU_SEQ_PARAM_EXT:
		avc->last_ps_idx = (s32) gf_bs_read_ue(bs);
		if (avc->last_ps_idx < 0) return -1;
		return 0;

	case GF_AVC_NALU_SEI:
	case GF_AVC_NALU_FILLER_DATA:
		return 0;

	default:
		if (avc->s_info.nal_unit_type <= GF_AVC_NALU_IDR_SLICE) ret = 1;
		//To detect change of AU when multiple sps and pps in stream
		else if ((nal_hdr & 0x1F) == GF_AVC_NALU_SEI && avc->s_info.nal_unit_type == GF_AVC_NALU_SVC_SLICE)
			ret = 1;
		else if ((nal_hdr & 0x1F) == GF_AVC_NALU_SEQ_PARAM && avc->s_info.nal_unit_type == GF_AVC_NALU_SVC_SLICE)
			ret = 1;
		else
			ret = 0;
		break;
	}

	/* save _prev values */
	if (ret && avc->s_info.sps) {
		n_state.frame_num_offset_prev = avc->s_info.frame_num_offset;
		if ((avc->s_info.sps->poc_type != 2) || (avc->s_info.nal_ref_idc != 0))
			n_state.frame_num_prev = avc->s_info.frame_num;
		if (avc->s_info.nal_ref_idc) {
			n_state.poc_lsb_prev = avc->s_info.poc_lsb;
			n_state.poc_msb_prev = avc->s_info.poc_msb;
		}
	}
	if (slice)
		avc_compute_poc(&n_state);
	memcpy(&avc->s_info, &n_state, sizeof(AVCSliceInfo));
	return ret;
}