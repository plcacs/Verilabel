static void scalar32_min_max_or(struct bpf_reg_state *dst_reg,
				struct bpf_reg_state *src_reg)
{
	bool src_known = tnum_subreg_is_const(src_reg->var_off);
	bool dst_known = tnum_subreg_is_const(dst_reg->var_off);
	struct tnum var32_off = tnum_subreg(dst_reg->var_off);
	s32 smin_val = src_reg->smin_value;
	u32 umin_val = src_reg->umin_value;

	/* Assuming scalar64_min_max_or will be called so it is safe
	 * to skip updating register for known case.
	 */
	if (src_known && dst_known)
		return;

	/* We get our maximum from the var_off, and our minimum is the
	 * maximum of the operands' minima
	 */
	dst_reg->u32_min_value = max(dst_reg->u32_min_value, umin_val);
	dst_reg->u32_max_value = var32_off.value | var32_off.mask;
	if (dst_reg->s32_min_value < 0 || smin_val < 0) {
		/* Lose signed bounds when ORing negative numbers,
		 * ain't nobody got time for that.
		 */
		dst_reg->s32_min_value = S32_MIN;
		dst_reg->s32_max_value = S32_MAX;
	} else {
		/* ORing two positives gives a positive, so safe to
		 * cast result into s64.
		 */
		dst_reg->s32_min_value = dst_reg->umin_value;
		dst_reg->s32_max_value = dst_reg->umax_value;
	}
}