R_API RBinJavaAttrInfo *r_bin_java_inner_classes_attr_new(RBinJavaObj *bin, ut8 *buffer, ut64 sz, ut64 buf_offset) {
	RBinJavaClassesAttribute *icattr;
	RBinJavaAttrInfo *attr = NULL;
	RBinJavaCPTypeObj *obj;
	ut32 i = 0;
	ut64 offset = 0, curpos;
	attr = r_bin_java_default_attr_new (bin, buffer, sz, buf_offset);
	offset += 6;
	if (attr == NULL) {
		// TODO eprintf
		return attr;
	}
	attr->type = R_BIN_JAVA_ATTR_TYPE_INNER_CLASSES_ATTR;
	attr->info.inner_classes_attr.number_of_classes = R_BIN_JAVA_USHORT (buffer, offset);
	offset += 2;
	attr->info.inner_classes_attr.classes = r_list_newf (r_bin_java_inner_classes_attr_entry_free);
	for (i = 0; i < attr->info.inner_classes_attr.number_of_classes; i++) {
		curpos = buf_offset + offset;
		if (offset + 8 > sz) {
			eprintf ("Invalid amount of inner classes\n");
			break;
		}
		icattr = R_NEW0 (RBinJavaClassesAttribute);
		if (!icattr) {
			break;
		}
		icattr->inner_class_info_idx = R_BIN_JAVA_USHORT (buffer, offset);
		offset += 2;
		icattr->outer_class_info_idx = R_BIN_JAVA_USHORT (buffer, offset);
		offset += 2;
		icattr->inner_name_idx = R_BIN_JAVA_USHORT (buffer, offset);
		offset += 2;
		icattr->inner_class_access_flags = R_BIN_JAVA_USHORT (buffer, offset);
		offset += 2;
		icattr->flags_str = retrieve_class_method_access_string (icattr->inner_class_access_flags);
		icattr->file_offset = curpos;
		icattr->size = 8;

		obj = r_bin_java_get_item_from_bin_cp_list (R_BIN_JAVA_GLOBAL_BIN, icattr->inner_name_idx);
		if (obj == NULL) {
			eprintf ("BINCPLIS IS HULL %d\n", icattr->inner_name_idx);
		}
		icattr->name = r_bin_java_get_item_name_from_bin_cp_list (R_BIN_JAVA_GLOBAL_BIN, obj);
		if (!icattr->name) {
			obj = r_bin_java_get_item_from_bin_cp_list (R_BIN_JAVA_GLOBAL_BIN, icattr->inner_class_info_idx);
			if (!obj) {
				eprintf ("BINCPLIST IS NULL %d\n", icattr->inner_class_info_idx);
			}
			icattr->name = r_bin_java_get_item_name_from_bin_cp_list (R_BIN_JAVA_GLOBAL_BIN, obj);
			if (!icattr->name) {
				icattr->name = r_str_dup (NULL, "NULL");
				eprintf ("r_bin_java_inner_classes_attr: Unable to find the name for %d index.\n", icattr->inner_name_idx);
				free (icattr);
				break;
			}
		}

		IFDBG eprintf("r_bin_java_inner_classes_attr: Inner class name %d is %s.\n", icattr->inner_name_idx, icattr->name);
		r_list_append (attr->info.inner_classes_attr.classes, (void *) icattr);
	}
	attr->size = offset;
	// IFDBG r_bin_java_print_inner_classes_attr_summary(attr);
	return attr;
}