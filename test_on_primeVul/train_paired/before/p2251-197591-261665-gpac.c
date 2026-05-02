static Fixed lsr_translate_coords(GF_LASeRCodec *lsr, u32 val, u32 nb_bits)
{
#ifdef GPAC_FIXED_POINT
	if (val >> (nb_bits-1) ) {
		s32 neg = (s32) val - (1<<nb_bits);
		if (neg < -FIX_ONE / 2)
			return 2 * gf_divfix(INT2FIX(neg/2), lsr->res_factor);
		return gf_divfix(INT2FIX(neg), lsr->res_factor);
	} else {
		if (val > FIX_ONE / 2)
			return 2 * gf_divfix(INT2FIX(val/2), lsr->res_factor);
		return gf_divfix(INT2FIX(val), lsr->res_factor);
	}
#else
	if (val >> (nb_bits-1) ) {
		s32 neg = (s32) val - (1<<nb_bits);
		return gf_divfix(INT2FIX(neg), lsr->res_factor);
	} else {
		return gf_divfix(INT2FIX(val), lsr->res_factor);
	}
#endif
}