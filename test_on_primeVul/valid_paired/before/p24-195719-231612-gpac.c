GF_Err Media_CheckDataEntry(GF_MediaBox *mdia, u32 dataEntryIndex)
{

	GF_DataEntryURLBox *entry;
	GF_DataMap *map;
	GF_Err e;
	if (!mdia || !dataEntryIndex || dataEntryIndex > gf_list_count(mdia->information->dataInformation->dref->child_boxes)) return GF_BAD_PARAM;

	entry = (GF_DataEntryURLBox*)gf_list_get(mdia->information->dataInformation->dref->child_boxes, dataEntryIndex - 1);
	if (!entry) return GF_ISOM_INVALID_FILE;
	if (entry->flags == 1) return GF_OK;

	//ok, not self contained, let's go for it...
	//we don't know what's a URN yet
	if (entry->type == GF_ISOM_BOX_TYPE_URN) return GF_NOT_SUPPORTED;
	if (mdia->mediaTrack->moov->mov->openMode == GF_ISOM_OPEN_WRITE) {
		e = gf_isom_datamap_new(entry->location, NULL, GF_ISOM_DATA_MAP_READ, &map);
	} else {
		e = gf_isom_datamap_new(entry->location, mdia->mediaTrack->moov->mov->fileName, GF_ISOM_DATA_MAP_READ, &map);
	}
	if (e) return e;
	gf_isom_datamap_del(map);
	return GF_OK;
}