GF_Err gf_isom_set_extraction_slc(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const GF_SLConfig *slConfig)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_Err e;
	GF_SLConfig **slc;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, StreamDescriptionIndex, &entry, NULL);
	if (e) return e;

	//we must be sure we are not using a remote ESD
	switch (entry->type) {
	case GF_ISOM_BOX_TYPE_MP4S:
		if (((GF_MPEGSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = & ((GF_MPEGSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4A:
		if (((GF_MPEGAudioSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = & ((GF_MPEGAudioSampleEntryBox *)entry)->slc;
		break;
	case GF_ISOM_BOX_TYPE_MP4V:
		if (((GF_MPEGVisualSampleEntryBox *)entry)->esd->desc->slConfig->predefined != SLPredef_MP4) return GF_BAD_PARAM;
		slc = & ((GF_MPEGVisualSampleEntryBox *)entry)->slc;
		break;
	default:
		return GF_BAD_PARAM;
	}

	if (*slc) {
		gf_odf_desc_del((GF_Descriptor *)*slc);
		*slc = NULL;
	}
	if (!slConfig) return GF_OK;
	//finally duplicate the SL
	return gf_odf_desc_copy((GF_Descriptor *) slConfig, (GF_Descriptor **) slc);
}