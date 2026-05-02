GF_Err stbl_GetSampleInfos(GF_SampleTableBox *stbl, u32 sampleNumber, u64 *offset, u32 *chunkNumber, u32 *descIndex, GF_StscEntry **out_ent)
{
	GF_Err e;
	u32 i, k, offsetInChunk, size, chunk_num;
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;
	GF_StscEntry *ent;

	(*offset) = 0;
	(*chunkNumber) = (*descIndex) = 0;
	if (out_ent) (*out_ent) = NULL;
	if (!stbl || !sampleNumber) return GF_BAD_PARAM;
	if (!stbl->ChunkOffset || !stbl->SampleToChunk || !stbl->SampleSize) return GF_ISOM_INVALID_FILE;

	if (stbl->SampleSize && stbl->SampleToChunk->nb_entries == stbl->SampleSize->sampleCount) {
		ent = &stbl->SampleToChunk->entries[sampleNumber-1];
		if (!ent) return GF_BAD_PARAM;
		(*descIndex) = ent->sampleDescriptionIndex;
		(*chunkNumber) = sampleNumber;
		if (out_ent) *out_ent = ent;
		if ( stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
			stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
			if (!stco->offsets) return GF_ISOM_INVALID_FILE;

			(*offset) = (u64) stco->offsets[sampleNumber - 1];
		} else {
			co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
			if (!co64->offsets) return GF_ISOM_INVALID_FILE;

			(*offset) = co64->offsets[sampleNumber - 1];
		}
		return GF_OK;
	}

	//check our cache: if desired sample is at or above current cache entry, start from here
	if (stbl->SampleToChunk->firstSampleInCurrentChunk &&
	        (stbl->SampleToChunk->firstSampleInCurrentChunk <= sampleNumber)) {

		i = stbl->SampleToChunk->currentIndex;
		ent = &stbl->SampleToChunk->entries[stbl->SampleToChunk->currentIndex];
		GetGhostNum(ent, i, stbl->SampleToChunk->nb_entries, stbl);
		k = stbl->SampleToChunk->currentChunk;
	}
	//otherwise start from first entry
	else {
		i = 0;
		stbl->SampleToChunk->currentIndex = 0;
		stbl->SampleToChunk->currentChunk = 1;
		stbl->SampleToChunk->ghostNumber = 1;
		stbl->SampleToChunk->firstSampleInCurrentChunk = 1;
		ent = &stbl->SampleToChunk->entries[0];
		GetGhostNum(ent, 0, stbl->SampleToChunk->nb_entries, stbl);
		k = stbl->SampleToChunk->currentChunk;
	}

	//first get the chunk
	for (; i < stbl->SampleToChunk->nb_entries; i++) {
		assert(stbl->SampleToChunk->firstSampleInCurrentChunk <= sampleNumber);
		//corrupted file (less sample2chunk info than sample count
		if (k > stbl->SampleToChunk->ghostNumber) {
			return GF_ISOM_INVALID_FILE;
		}


		//check if sample is in current chunk
		u32 max_chunks_in_entry = stbl->SampleToChunk->ghostNumber - k;
		u32 nb_chunks_for_sample = sampleNumber - stbl->SampleToChunk->firstSampleInCurrentChunk;
		if (ent->samplesPerChunk) 
			nb_chunks_for_sample /= ent->samplesPerChunk;

		if (
			(nb_chunks_for_sample <= max_chunks_in_entry)
			&& (stbl->SampleToChunk->firstSampleInCurrentChunk + (nb_chunks_for_sample+1) * ent->samplesPerChunk > sampleNumber)
		) {

			stbl->SampleToChunk->firstSampleInCurrentChunk += nb_chunks_for_sample * ent->samplesPerChunk;
			stbl->SampleToChunk->currentChunk += nb_chunks_for_sample;
			goto sample_found;
		}
		max_chunks_in_entry += 1;
		stbl->SampleToChunk->firstSampleInCurrentChunk += max_chunks_in_entry * ent->samplesPerChunk;
		stbl->SampleToChunk->currentChunk += max_chunks_in_entry;

		//not in this entry, get the next entry if not the last one
		if (i+1 != stbl->SampleToChunk->nb_entries) {
			ent = &stbl->SampleToChunk->entries[i+1];
			//update the GhostNumber
			GetGhostNum(ent, i+1, stbl->SampleToChunk->nb_entries, stbl);
			//update the entry in our cache
			stbl->SampleToChunk->currentIndex = i+1;
			stbl->SampleToChunk->currentChunk = 1;
			k = 1;
		}
	}
	//if we get here, gasp, the sample was not found
	return GF_ISOM_INVALID_FILE;

sample_found:

	(*descIndex) = ent->sampleDescriptionIndex;
	(*chunkNumber) = chunk_num = ent->firstChunk + stbl->SampleToChunk->currentChunk - 1;
	if (out_ent) *out_ent = ent;
	if (! *chunkNumber)
		return GF_ISOM_INVALID_FILE;
	
	//ok, get the size of all the previous samples in the chunk
	offsetInChunk = 0;
	//constant size
	if (stbl->SampleSize && stbl->SampleSize->sampleSize) {
		u32 diff = sampleNumber - stbl->SampleToChunk->firstSampleInCurrentChunk;
		offsetInChunk += diff * stbl->SampleSize->sampleSize;
	} else if ((stbl->r_last_chunk_num == chunk_num) && (stbl->r_last_sample_num == sampleNumber)) {
		offsetInChunk = stbl->r_last_offset_in_chunk;
	} else if ((stbl->r_last_chunk_num == chunk_num) && (stbl->r_last_sample_num + 1 == sampleNumber)) {
		e = stbl_GetSampleSize(stbl->SampleSize, stbl->r_last_sample_num, &size);
		if (e) return e;
		stbl->r_last_offset_in_chunk += size;
		stbl->r_last_sample_num = sampleNumber;
		offsetInChunk = stbl->r_last_offset_in_chunk;
	} else {
		//warning, firstSampleInChunk is at least 1 - not 0
		for (i = stbl->SampleToChunk->firstSampleInCurrentChunk; i < sampleNumber; i++) {
			e = stbl_GetSampleSize(stbl->SampleSize, i, &size);
			if (e) return e;
			offsetInChunk += size;
		}
		stbl->r_last_chunk_num = chunk_num;
		stbl->r_last_sample_num = sampleNumber;
		stbl->r_last_offset_in_chunk = offsetInChunk;
	}
	//OK, that's the size of our offset in the chunk
	//now get the chunk
	if ( stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
		stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
		if (stco->nb_entries < (*chunkNumber) ) return GF_ISOM_INVALID_FILE;
		(*offset) = (u64) stco->offsets[(*chunkNumber) - 1] + (u64) offsetInChunk;
	} else {
		co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
		if (co64->nb_entries < (*chunkNumber) ) return GF_ISOM_INVALID_FILE;
		(*offset) = co64->offsets[(*chunkNumber) - 1] + (u64) offsetInChunk;
	}
	return GF_OK;
}