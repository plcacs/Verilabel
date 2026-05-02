GF_Err stbl_AppendSize(GF_SampleTableBox *stbl, u32 size, u32 nb_pack)
{
	u32 i;
	if (!nb_pack) nb_pack = 1;

	if (!stbl->SampleSize->sampleCount) {
		stbl->SampleSize->sampleSize = size;
		stbl->SampleSize->sampleCount += nb_pack;
		return GF_OK;
	}
	if (stbl->SampleSize->sampleSize && (stbl->SampleSize->sampleSize==size)) {
		stbl->SampleSize->sampleCount += nb_pack;
		return GF_OK;
	}
	if (!stbl->SampleSize->sizes || (stbl->SampleSize->sampleCount+nb_pack > stbl->SampleSize->alloc_size)) {
		Bool init_table = (stbl->SampleSize->sizes==NULL) ? 1 : 0;
		ALLOC_INC(stbl->SampleSize->alloc_size);
		if (stbl->SampleSize->sampleCount+nb_pack > stbl->SampleSize->alloc_size)
			stbl->SampleSize->alloc_size = stbl->SampleSize->sampleCount+nb_pack;

		stbl->SampleSize->sizes = (u32 *)gf_realloc(stbl->SampleSize->sizes, sizeof(u32)*stbl->SampleSize->alloc_size);
		if (!stbl->SampleSize->sizes) return GF_OUT_OF_MEM;
		memset(&stbl->SampleSize->sizes[stbl->SampleSize->sampleCount], 0, sizeof(u32) * (stbl->SampleSize->alloc_size - stbl->SampleSize->sampleCount) );

		if (init_table) {
			for (i=0; i<stbl->SampleSize->sampleCount; i++)
				stbl->SampleSize->sizes[i] = stbl->SampleSize->sampleSize;
		}
	}
	stbl->SampleSize->sampleSize = 0;
	for (i=0; i<nb_pack; i++) {
		stbl->SampleSize->sizes[stbl->SampleSize->sampleCount+i] = size;
	}
	stbl->SampleSize->sampleCount += nb_pack;
	if (size > stbl->SampleSize->max_size)
		stbl->SampleSize->max_size = size;
	stbl->SampleSize->total_size += size;
	stbl->SampleSize->total_samples += nb_pack;
	return GF_OK;
}