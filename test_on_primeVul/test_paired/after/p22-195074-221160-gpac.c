GF_AV1Config *gf_odf_av1_cfg_read_bs_size(GF_BitStream *bs, u32 size)
{
#ifndef GPAC_DISABLE_AV_PARSERS
	AV1State state;
	u8 reserved;
	GF_AV1Config *cfg;

	if (!size) size = (u32) gf_bs_available(bs);
	if (!size) return NULL;

	cfg = gf_odf_av1_cfg_new();
	gf_av1_init_state(&state);
	state.config = cfg;

	cfg->marker = gf_bs_read_int(bs, 1);
	cfg->version = gf_bs_read_int(bs, 7);
	cfg->seq_profile = gf_bs_read_int(bs, 3);
	cfg->seq_level_idx_0 = gf_bs_read_int(bs, 5);
	cfg->seq_tier_0 = gf_bs_read_int(bs, 1);
	cfg->high_bitdepth = gf_bs_read_int(bs, 1);
	cfg->twelve_bit = gf_bs_read_int(bs, 1);
	cfg->monochrome = gf_bs_read_int(bs, 1);
	cfg->chroma_subsampling_x = gf_bs_read_int(bs, 1);
	cfg->chroma_subsampling_y = gf_bs_read_int(bs, 1);
	cfg->chroma_sample_position = gf_bs_read_int(bs, 2);

	reserved = gf_bs_read_int(bs, 3);
	if (reserved != 0 || cfg->marker != 1 || cfg->version != 1) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1] wrong avcC reserved %d / marker %d / version %d expecting 0 1 1\n", reserved, cfg->marker, cfg->version));
		gf_odf_av1_cfg_del(cfg);
		return NULL;
	}
	cfg->initial_presentation_delay_present = gf_bs_read_int(bs, 1);
	if (cfg->initial_presentation_delay_present) {
		cfg->initial_presentation_delay_minus_one = gf_bs_read_int(bs, 4);
	} else {
		/*reserved = */gf_bs_read_int(bs, 4);
		cfg->initial_presentation_delay_minus_one = 0;
	}
	size -= 4;

	while (size) {
		u64 pos, obu_size;
		ObuType obu_type;
		GF_AV1_OBUArrayEntry *a;

		pos = gf_bs_get_position(bs);
		obu_size = 0;
		if (gf_av1_parse_obu(bs, &obu_type, &obu_size, NULL, &state) != GF_OK) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[AV1] could not parse AV1 OBU at position "LLU". Leaving parsing.\n", pos));
			break;
		}
		assert(obu_size == gf_bs_get_position(bs) - pos);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1] parsed AV1 OBU type=%u size="LLU" at position "LLU".\n", obu_type, obu_size, pos));

		if (!av1_is_obu_header(obu_type)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[AV1] AV1 unexpected OBU type=%u size="LLU" found at position "LLU". Forwarding.\n", pos));
		}
		GF_SAFEALLOC(a, GF_AV1_OBUArrayEntry);
		if (!a) break;
		a->obu = gf_malloc((size_t)obu_size);
		if (!a->obu) {
			gf_free(a);
			break;
		}
		gf_bs_seek(bs, pos);
		gf_bs_read_data(bs, (char *) a->obu, (u32)obu_size);
		a->obu_length = obu_size;
		a->obu_type = obu_type;
		gf_list_add(cfg->obu_array, a);

		if (size<obu_size) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[AV1] AV1 config misses %d bytes to fit the entire OBU\n", obu_size - size));
			break;
		}
		size -= (u32) obu_size;
	}
	gf_av1_reset_state(& state, GF_TRUE);
	gf_bs_align(bs);
	return cfg;
#else
	return NULL;
#endif
}