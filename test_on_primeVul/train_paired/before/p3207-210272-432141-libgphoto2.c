ptp_unpack_OPL (PTPParams *params, unsigned char* data, MTPProperties **pprops, unsigned int len)
{ 
	uint32_t prop_count = dtoh32a(data);
	MTPProperties *props = NULL;
	unsigned int offset = 0, i;

	*pprops = NULL;
	if (prop_count == 0)
		return 0;
	if (prop_count >= INT_MAX/sizeof(MTPProperties)) {
		ptp_debug (params ,"prop_count %d is too large", prop_count);
		return 0;
	}
	ptp_debug (params ,"Unpacking MTP OPL, size %d (prop_count %d)", len, prop_count);
	data += sizeof(uint32_t);
	len -= sizeof(uint32_t);
	props = malloc(prop_count * sizeof(MTPProperties));
	if (!props) return 0;
	for (i = 0; i < prop_count; i++) {
		if (len <= 0) {
			ptp_debug (params ,"short MTP Object Property List at property %d (of %d)", i, prop_count);
			ptp_debug (params ,"device probably needs DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST_ALL");
			ptp_debug (params ,"or even DEVICE_FLAG_BROKEN_MTPGETOBJPROPLIST", i);
			qsort (props, i, sizeof(MTPProperties),_compare_func);
			*pprops = props;
			return i;
		}
		props[i].ObjectHandle = dtoh32a(data);
		data += sizeof(uint32_t);
		len -= sizeof(uint32_t);

		props[i].property = dtoh16a(data);
		data += sizeof(uint16_t);
		len -= sizeof(uint16_t);

		props[i].datatype = dtoh16a(data);
		data += sizeof(uint16_t);
		len -= sizeof(uint16_t);

		offset = 0;
		if (!ptp_unpack_DPV(params, data, &offset, len, &props[i].propval, props[i].datatype)) {
			ptp_debug (params ,"unpacking DPV of property %d encountered insufficient buffer. attack?", i);
			qsort (props, i, sizeof(MTPProperties),_compare_func);
			*pprops = props;
			return i;
		}
		data += offset;
		len -= offset;
	}
	qsort (props, prop_count, sizeof(MTPProperties),_compare_func);
	*pprops = props;
	return prop_count;
}