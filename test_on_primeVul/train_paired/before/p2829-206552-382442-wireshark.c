_proto_tree_add_bits_ret_val(proto_tree *tree, const int hfindex, tvbuff_t *tvb,
			    const guint bit_offset, const gint no_of_bits,
			    guint64 *return_value, const guint encoding)
{
	gint     offset;
	guint    length;
	guint8   tot_no_bits;
	char    *bf_str;
	char     lbl_str[ITEM_LABEL_LENGTH];
	guint64  value = 0;
	guint8  *bytes = NULL;
	size_t bytes_length = 0;

	proto_item        *pi;
	header_field_info *hf_field;

	const true_false_string *tfstring;

	/* We can't fake it just yet. We have to fill in the 'return_value' parameter */
	PROTO_REGISTRAR_GET_NTH(hfindex, hf_field);

	if (hf_field->bitmask != 0) {
		REPORT_DISSECTOR_BUG("Incompatible use of proto_tree_add_bits_ret_val"
				     " with field '%s' (%s) with bitmask != 0",
				     hf_field->abbrev, hf_field->name);
	}

	if (no_of_bits == 0) {
		REPORT_DISSECTOR_BUG("field %s passed to proto_tree_add_bits_ret_val() has a bit width of 0",
				     hf_field->abbrev);
	}

	/* Byte align offset */
	offset = bit_offset>>3;

	/*
	 * Calculate the number of octets used to hold the bits
	 */
	tot_no_bits = ((bit_offset&0x7) + no_of_bits);
	length = (tot_no_bits + 7) >> 3;

	if (no_of_bits < 65) {
		value = tvb_get_bits64(tvb, bit_offset, no_of_bits, encoding);
	} else if (hf_field->type != FT_BYTES) {
		REPORT_DISSECTOR_BUG("field %s passed to proto_tree_add_bits_ret_val() has a bit width of %u > 65",
				     hf_field->abbrev, no_of_bits);
		return NULL;
	}

	/* Sign extend for signed types */
	switch (hf_field->type) {
		case FT_INT8:
		case FT_INT16:
		case FT_INT24:
		case FT_INT32:
		case FT_INT40:
		case FT_INT48:
		case FT_INT56:
		case FT_INT64:
			value = ws_sign_ext64(value, no_of_bits);
			break;

		default:
			break;
	}

	if (return_value) {
		*return_value = value;
	}

	/* Coast clear. Try and fake it */
	CHECK_FOR_NULL_TREE(tree);
	TRY_TO_FAKE_THIS_ITEM(tree, hfindex, hf_field);

	bf_str = decode_bits_in_field(bit_offset, no_of_bits, value);

	switch (hf_field->type) {
	case FT_BOOLEAN:
		/* Boolean field */
		tfstring = &tfs_true_false;
		if (hf_field->strings)
			tfstring = (const true_false_string *)hf_field->strings;
		return proto_tree_add_boolean_format(tree, hfindex, tvb, offset, length, (guint32)value,
			"%s = %s: %s",
			bf_str, hf_field->name, tfs_get_string(!!value, tfstring));
		break;

	case FT_CHAR:
		pi = proto_tree_add_uint(tree, hfindex, tvb, offset, length, (guint32)value);
		fill_label_char(PITEM_FINFO(pi), lbl_str);
		break;

	case FT_UINT8:
	case FT_UINT16:
	case FT_UINT24:
	case FT_UINT32:
		pi = proto_tree_add_uint(tree, hfindex, tvb, offset, length, (guint32)value);
		fill_label_number(PITEM_FINFO(pi), lbl_str, FALSE);
		break;

	case FT_INT8:
	case FT_INT16:
	case FT_INT24:
	case FT_INT32:
		pi = proto_tree_add_int(tree, hfindex, tvb, offset, length, (gint32)value);
		fill_label_number(PITEM_FINFO(pi), lbl_str, TRUE);
		break;

	case FT_UINT40:
	case FT_UINT48:
	case FT_UINT56:
	case FT_UINT64:
		pi = proto_tree_add_uint64(tree, hfindex, tvb, offset, length, value);
		fill_label_number64(PITEM_FINFO(pi), lbl_str, FALSE);
		break;

	case FT_INT40:
	case FT_INT48:
	case FT_INT56:
	case FT_INT64:
		pi = proto_tree_add_int64(tree, hfindex, tvb, offset, length, (gint64)value);
		fill_label_number64(PITEM_FINFO(pi), lbl_str, TRUE);
		break;

	case FT_BYTES:
		bytes = tvb_get_bits_array(NULL, tvb, bit_offset, no_of_bits, &bytes_length);
		pi = proto_tree_add_bytes_with_length(tree, hfindex, tvb, offset, length, bytes, (gint) bytes_length);
		proto_item_fill_label(PITEM_FINFO(pi), lbl_str);
		proto_item_set_text(pi, "%s", lbl_str);
		return pi;
		break;

	default:
		REPORT_DISSECTOR_BUG("field %s has type %d (%s) not handled in proto_tree_add_bits_ret_val()",
				     hf_field->abbrev,
				     hf_field->type,
				     ftype_name(hf_field->type));
		return NULL;
		break;
	}

	proto_item_set_text(pi, "%s = %s", bf_str, lbl_str);
	return pi;
}