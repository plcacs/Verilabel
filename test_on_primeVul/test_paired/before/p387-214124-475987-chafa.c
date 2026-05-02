lzw_result lzw_decode(struct lzw_ctx *ctx,
		const uint8_t ** const stack_pos_out)
{
	lzw_result res;
	uint32_t code_new;
	uint32_t code_out;
	uint8_t last_value;
	uint8_t *stack_pos = ctx->stack_base;
	uint32_t clear_code = ctx->clear_code;
	uint32_t current_entry = ctx->current_entry;
	struct lzw_dictionary_entry * const table = ctx->table;

	/* Get a new code from the input */
	res = lzw__next_code(&ctx->input, ctx->current_code_size, &code_new);
	if (res != LZW_OK) {
		return res;
	}

	/* Handle the new code */
	if (code_new == clear_code) {
		/* Got Clear code */
		return lzw__clear_codes(ctx, stack_pos_out);

	} else if (code_new == ctx->eoi_code) {
		/* Got End of Information code */
		return LZW_EOI_CODE;

	} else if (code_new > current_entry) {
		/* Code is invalid */
		return LZW_BAD_CODE;

	} else if (code_new < current_entry) {
		/* Code is in table */
		code_out = code_new;
		last_value = table[code_new].first_value;
	} else {
		/* Code not in table */
		*stack_pos++ = ctx->previous_code_first;
		code_out = ctx->previous_code;
		last_value = ctx->previous_code_first;
	}

	/* Add to the dictionary, only if there's space */
	if (current_entry < (1 << LZW_CODE_MAX)) {
		struct lzw_dictionary_entry *entry = table + current_entry;
		entry->last_value     = last_value;
		entry->first_value    = ctx->previous_code_first;
		entry->previous_entry = ctx->previous_code;
		ctx->current_entry++;
	}

	/* Ensure code size is increased, if needed. */
	if (current_entry == ctx->current_code_size_max) {
		if (ctx->current_code_size < LZW_CODE_MAX) {
			ctx->current_code_size++;
			ctx->current_code_size_max =
					(1 << ctx->current_code_size) - 1;
		}
	}

	/* Store details of this code as "previous code" to the context. */
	ctx->previous_code_first = table[code_new].first_value;
	ctx->previous_code = code_new;

	/* Put rest of data for this code on output stack.
	 * Note, in the case of "code not in table", the last entry of the
	 * current code has already been placed on the stack above. */
	while (code_out > clear_code) {
		struct lzw_dictionary_entry *entry = table + code_out;
		*stack_pos++ = entry->last_value;
		code_out = entry->previous_entry;
	}
	*stack_pos++ = table[code_out].last_value;

	*stack_pos_out = stack_pos;
	return LZW_OK;
}