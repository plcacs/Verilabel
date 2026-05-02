static RzDyldRebaseInfos *get_rebase_infos(RzDyldCache *cache) {
	RzDyldRebaseInfos *result = RZ_NEW0(RzDyldRebaseInfos);
	if (!result) {
		return NULL;
	}

	if (!cache->hdr->slideInfoOffset || !cache->hdr->slideInfoSize) {
		ut32 total_slide_infos = 0;
		ut32 n_slide_infos[MAX_N_HDR];

		ut32 i;
		for (i = 0; i < cache->n_hdr && i < MAX_N_HDR; i++) {
			ut64 hdr_offset = cache->hdr_offset[i];
			if (!rz_buf_read_le32_at(cache->buf, 0x13c + hdr_offset, &n_slide_infos[i])) {
				goto beach;
			}
			total_slide_infos += n_slide_infos[i];
		}

		if (!total_slide_infos) {
			goto beach;
		}

		RzDyldRebaseInfosEntry *infos = RZ_NEWS0(RzDyldRebaseInfosEntry, total_slide_infos);
		if (!infos) {
			goto beach;
		}

		ut32 k = 0;
		for (i = 0; i < cache->n_hdr && i < MAX_N_HDR; i++) {
			ut64 hdr_offset = cache->hdr_offset[i];
			if (!n_slide_infos[i]) {
				continue;
			}
			ut32 sio;
			if (!rz_buf_read_le32_at(cache->buf, 0x138 + hdr_offset, &sio)) {
				continue;
			}
			ut64 slide_infos_offset = sio;
			if (!slide_infos_offset) {
				continue;
			}
			slide_infos_offset += hdr_offset;

			ut32 j;
			RzDyldRebaseInfo *prev_info = NULL;
			for (j = 0; j < n_slide_infos[i]; j++) {
				ut64 offset = slide_infos_offset + j * sizeof(cache_mapping_slide);
				cache_mapping_slide entry;
				if (rz_buf_fread_at(cache->buf, offset, (ut8 *)&entry, "6lii", 1) != sizeof(cache_mapping_slide)) {
					break;
				}

				if (entry.slideInfoOffset && entry.slideInfoSize) {
					infos[k].start = entry.fileOffset + hdr_offset;
					infos[k].end = infos[k].start + entry.size;
					ut64 slide = prev_info ? prev_info->slide : UT64_MAX;
					infos[k].info = get_rebase_info(cache, entry.slideInfoOffset + hdr_offset, entry.slideInfoSize, entry.fileOffset + hdr_offset, slide);
					prev_info = infos[k].info;
					k++;
				}
			}
		}

		if (!k) {
			free(infos);
			goto beach;
		}

		if (k < total_slide_infos) {
			RzDyldRebaseInfosEntry *pruned_infos = RZ_NEWS0(RzDyldRebaseInfosEntry, k);
			if (!pruned_infos) {
				free(infos);
				goto beach;
			}

			memcpy(pruned_infos, infos, sizeof(RzDyldRebaseInfosEntry) * k);
			free(infos);
			infos = pruned_infos;
		}

		result->entries = infos;
		result->length = k;
		return result;
	}

	if (cache->hdr->mappingCount > 1) {
		RzDyldRebaseInfosEntry *infos = RZ_NEWS0(RzDyldRebaseInfosEntry, 1);
		if (!infos) {
			goto beach;
		}

		infos[0].start = cache->maps[1].fileOffset;
		infos[0].end = infos[0].start + cache->maps[1].size;
		infos[0].info = get_rebase_info(cache, cache->hdr->slideInfoOffset, cache->hdr->slideInfoSize, infos[0].start, UT64_MAX);

		result->entries = infos;
		result->length = 1;
		return result;
	}

beach:
	free(result);
	return NULL;
}