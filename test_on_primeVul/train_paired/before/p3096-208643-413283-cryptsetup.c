static int hdr_validate_segments(struct crypt_device *cd, json_object *hdr_jobj)
{
	json_object *jobj_segments, *jobj_digests, *jobj_offset, *jobj_size, *jobj_type, *jobj_flags, *jobj;
	struct interval *intervals;
	uint64_t offset, size;
	int i, r, count, first_backup = -1;

	if (!json_object_object_get_ex(hdr_jobj, "segments", &jobj_segments)) {
		log_dbg(cd, "Missing segments section.");
		return 1;
	}

	count = json_object_object_length(jobj_segments);
	if (count < 1) {
		log_dbg(cd, "Empty segments section.");
		return 1;
	}

	/* digests should already be validated */
	if (!json_object_object_get_ex(hdr_jobj, "digests", &jobj_digests))
		return 1;

	json_object_object_foreach(jobj_segments, key, val) {
		if (!numbered(cd, "Segment", key))
			return 1;

		/* those fields are mandatory for all segment types */
		if (!(jobj_type =   json_contains(cd, val, key, "Segment", "type",   json_type_string)) ||
		    !(jobj_offset = json_contains(cd, val, key, "Segment", "offset", json_type_string)) ||
		    !(jobj_size =   json_contains(cd, val, key, "Segment", "size",   json_type_string)))
			return 1;

		if (!numbered(cd, "offset", json_object_get_string(jobj_offset)) ||
		    !json_str_to_uint64(jobj_offset, &offset))
			return 1;

		/* size "dynamic" means whole device starting at 'offset' */
		if (strcmp(json_object_get_string(jobj_size), "dynamic")) {
			if (!numbered(cd, "size", json_object_get_string(jobj_size)) ||
			    !json_str_to_uint64(jobj_size, &size) || !size)
				return 1;
		} else
			size = 0;

		/* all device-mapper devices are aligned to 512 sector size */
		if (MISALIGNED_512(offset)) {
			log_dbg(cd, "Offset field has to be aligned to sector size: %" PRIu32, SECTOR_SIZE);
			return 1;
		}
		if (MISALIGNED_512(size)) {
			log_dbg(cd, "Size field has to be aligned to sector size: %" PRIu32, SECTOR_SIZE);
			return 1;
		}

		/* flags array is optional and must contain strings */
		if (json_object_object_get_ex(val, "flags", NULL)) {
			if (!(jobj_flags = json_contains(cd, val, key, "Segment", "flags", json_type_array)))
				return 1;
			for (i = 0; i < (int) json_object_array_length(jobj_flags); i++)
				if (!json_object_is_type(json_object_array_get_idx(jobj_flags, i), json_type_string))
					return 1;
		}

		i = atoi(key);
		if (json_segment_is_backup(val)) {
			if (first_backup < 0 || i < first_backup)
				first_backup = i;
		} else {
			if ((first_backup >= 0) && i >= first_backup) {
				log_dbg(cd, "Regular segment at %d is behind backup segment at %d", i, first_backup);
				return 1;
			}
		}

		/* crypt */
		if (!strcmp(json_object_get_string(jobj_type), "crypt") &&
		    hdr_validate_crypt_segment(cd, val, key, jobj_digests, offset, size))
			return 1;
	}

	if (first_backup == 0) {
		log_dbg(cd, "No regular segment.");
		return 1;
	}

	if (first_backup < 0)
		first_backup = count;

	intervals = malloc(first_backup * sizeof(*intervals));
	if (!intervals) {
		log_dbg(cd, "Not enough memory.");
		return 1;
	}

	for (i = 0; i < first_backup; i++) {
		jobj = json_segments_get_segment(jobj_segments, i);
		if (!jobj) {
			log_dbg(cd, "Gap at key %d in segments object.", i);
			free(intervals);
			return 1;
		}
		intervals[i].offset = json_segment_get_offset(jobj, 0);
		intervals[i].length = json_segment_get_size(jobj, 0) ?: UINT64_MAX;
	}

	r = !validate_segment_intervals(cd, first_backup, intervals);
	free(intervals);

	if (r)
		return 1;

	for (; i < count; i++) {
		if (!json_segments_get_segment(jobj_segments, i)) {
			log_dbg(cd, "Gap at key %d in segments object.", i);
			return 1;
		}
	}

	return 0;
}