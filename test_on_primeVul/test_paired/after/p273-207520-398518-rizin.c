static const ut8 *parse_die(const ut8 *buf, const ut8 *buf_end, RzBinDwarfDebugInfo *info, RzBinDwarfAbbrevDecl *abbrev,
	RzBinDwarfCompUnitHdr *hdr, RzBinDwarfDie *die, const ut8 *debug_str, size_t debug_str_len, bool big_endian) {
	size_t i;
	const char *comp_dir = NULL;
	ut64 line_info_offset = UT64_MAX;
	if (abbrev->count) {
		for (i = 0; i < abbrev->count - 1; i++) {
			memset(&die->attr_values[i], 0, sizeof(die->attr_values[i]));

			buf = parse_attr_value(buf, buf_end - buf, &abbrev->defs[i],
				&die->attr_values[i], hdr, debug_str, debug_str_len, big_endian);

			RzBinDwarfAttrValue *attribute = &die->attr_values[i];

			if (attribute->attr_name == DW_AT_comp_dir && (attribute->attr_form == DW_FORM_strp || attribute->attr_form == DW_FORM_string) && attribute->string.content) {
				comp_dir = attribute->string.content;
			}
			if (attribute->attr_name == DW_AT_stmt_list) {
				if (attribute->kind == DW_AT_KIND_CONSTANT) {
					line_info_offset = attribute->uconstant;
				} else if (attribute->kind == DW_AT_KIND_REFERENCE) {
					line_info_offset = attribute->reference;
				}
			}
			die->count++;
		}
	}

	// If this is a compilation unit dir attribute, we want to cache it so the line info parsing
	// which will need this info can quickly look it up.
	if (comp_dir && line_info_offset != UT64_MAX) {
		char *name = strdup(comp_dir);
		if (name) {
			if (!ht_up_insert(info->line_info_offset_comp_dir, line_info_offset, name)) {
				free(name);
			}
		}
	}

	return buf;
}