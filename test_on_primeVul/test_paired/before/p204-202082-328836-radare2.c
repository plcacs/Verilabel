R_API RBinJavaAttrInfo *r_bin_java_bootstrap_methods_attr_new(RBinJavaObj *bin, ut8 *buffer, ut64 sz, ut64 buf_offset) {
	ut32 i = 0;
	RBinJavaBootStrapMethod *bsm = NULL;
	ut64 offset = 0;
	RBinJavaAttrInfo *attr = r_bin_java_default_attr_new (bin, buffer, sz, buf_offset);
	offset += 6;
	if (attr) {
		attr->type = R_BIN_JAVA_ATTR_TYPE_BOOTSTRAP_METHODS_ATTR;
		attr->info.bootstrap_methods_attr.num_bootstrap_methods = R_BIN_JAVA_USHORT (buffer, offset);
		offset += 2;
		attr->info.bootstrap_methods_attr.bootstrap_methods = r_list_newf (r_bin_java_bootstrap_method_free);
		for (i = 0; i < attr->info.bootstrap_methods_attr.num_bootstrap_methods; i++) {
			// bsm = r_bin_java_bootstrap_method_new (bin, bin->b->cur);
			if (offset >= sz) {
				break;
			}
			bsm = r_bin_java_bootstrap_method_new (buffer + offset, sz - offset, buf_offset + offset);
			if (bsm) {
				offset += bsm->size;
				r_list_append (attr->info.bootstrap_methods_attr.bootstrap_methods, (void *) bsm);
			} else {
				// TODO eprintf Failed to read the %d boot strap method.
			}
		}
		attr->size = offset;
	}
	return attr;
}