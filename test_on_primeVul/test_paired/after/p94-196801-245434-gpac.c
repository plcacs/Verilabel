GF_Err gf_hinter_finalize(GF_ISOFile *file, GF_SDP_IODProfile IOD_Profile, u32 bandwidth)
{
	u32 i, sceneT, odT, descIndex, size, size64;
	GF_InitialObjectDescriptor *iod;
	GF_SLConfig slc;
	GF_ISOSample *samp;
	Bool remove_ocr;
	u8 *buffer;
	char buf64[5000], sdpLine[5100];


	gf_isom_sdp_clean(file);

	if (bandwidth) {
		sprintf(buf64, "b=AS:%d", bandwidth);
		gf_isom_sdp_add_line(file, buf64);
	}
    //xtended attribute for copyright
    if (gf_sys_is_test_mode()) {
        sprintf(buf64, "a=x-copyright: %s", "MP4/3GP File hinted with GPAC - (c) Telecom ParisTech (http://gpac.io)");
    } else {
        sprintf(buf64, "a=x-copyright: MP4/3GP File hinted with GPAC %s - %s", gf_gpac_version(), gf_gpac_copyright() );
    }
	gf_isom_sdp_add_line(file, buf64);

	if (IOD_Profile == GF_SDP_IOD_NONE) return GF_OK;

	odT = sceneT = 0;
	for (i=0; i<gf_isom_get_track_count(file); i++) {
		if (!gf_isom_is_track_in_root_od(file, i+1)) continue;
		switch (gf_isom_get_media_type(file,i+1)) {
		case GF_ISOM_MEDIA_OD:
			odT = i+1;
			break;
		case GF_ISOM_MEDIA_SCENE:
			sceneT = i+1;
			break;
		}
	}
	remove_ocr = 0;
	if (IOD_Profile == GF_SDP_IOD_ISMA_STRICT) {
		IOD_Profile = GF_SDP_IOD_ISMA;
		remove_ocr = 1;
	}

	/*if we want ISMA like iods, we need at least BIFS */
	if ( (IOD_Profile == GF_SDP_IOD_ISMA) && !sceneT ) return GF_BAD_PARAM;

	/*do NOT change PLs, we assume they are correct*/
	iod = (GF_InitialObjectDescriptor *) gf_isom_get_root_od(file);
	if (!iod) return GF_NOT_SUPPORTED;

	/*rewrite an IOD with good SL config - embbed data if possible*/
	if (IOD_Profile == GF_SDP_IOD_ISMA) {
		GF_ESD *esd;
		Bool is_ok = 1;
		while (gf_list_count(iod->ESDescriptors)) {
			esd = (GF_ESD*)gf_list_get(iod->ESDescriptors, 0);
			gf_odf_desc_del((GF_Descriptor *) esd);
			gf_list_rem(iod->ESDescriptors, 0);
		}


		/*get OD esd, and embbed stream data if possible*/
		if (odT) {
			esd = gf_isom_get_esd(file, odT, 1);
			if (gf_isom_get_sample_count(file, odT)==1) {
				samp = gf_isom_get_sample(file, odT, 1, &descIndex);
				if (samp && gf_hinter_can_embbed_data(samp->data, samp->dataLength, GF_STREAM_OD)) {
					InitSL_NULL(&slc);
					slc.predefined = 0;
					slc.hasRandomAccessUnitsOnlyFlag = 1;
					slc.timeScale = slc.timestampResolution = gf_isom_get_media_timescale(file, odT);
					slc.OCRResolution = 1000;
					slc.startCTS = samp->DTS+samp->CTS_Offset;
					slc.startDTS = samp->DTS;
					//set the SL for future extraction
					gf_isom_set_extraction_slc(file, odT, 1, &slc);

					size64 = gf_base64_encode(samp->data, samp->dataLength, buf64, 2000);
					buf64[size64] = 0;
					sprintf(sdpLine, "data:application/mpeg4-od-au;base64,%s", buf64);

					if (esd->decoderConfig) {
						esd->decoderConfig->avgBitrate = 0;
						esd->decoderConfig->bufferSizeDB = samp->dataLength;
						esd->decoderConfig->maxBitrate = 0;
					}
					size64 = (u32) strlen(sdpLine)+1;
					esd->URLString = (char*)gf_malloc(sizeof(char) * size64);
					strcpy(esd->URLString, sdpLine);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_RTP, ("[rtp hinter] OD sample too large to be embedded in IOD - ISMA disabled\n"));
					is_ok = 0;
				}
				gf_isom_sample_del(&samp);
			}
			if (remove_ocr) esd->OCRESID = 0;
			else if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;

			//OK, add this to our IOD
			gf_list_add(iod->ESDescriptors, esd);
		}

		esd = gf_isom_get_esd(file, sceneT, 1);
		if (gf_isom_get_sample_count(file, sceneT)==1) {
			samp = gf_isom_get_sample(file, sceneT, 1, &descIndex);
			if (samp && gf_hinter_can_embbed_data(samp->data, samp->dataLength, GF_STREAM_SCENE)) {
				InitSL_NULL(&slc);
				slc.timeScale = slc.timestampResolution = gf_isom_get_media_timescale(file, sceneT);
				slc.OCRResolution = 1000;
				slc.startCTS = samp->DTS+samp->CTS_Offset;
				slc.startDTS = samp->DTS;
				//set the SL for future extraction
				gf_isom_set_extraction_slc(file, sceneT, 1, &slc);
				//encode in Base64 the sample
				size64 = gf_base64_encode(samp->data, samp->dataLength, buf64, 2000);
				buf64[size64] = 0;
				sprintf(sdpLine, "data:application/mpeg4-bifs-au;base64,%s", buf64);

				if (esd->decoderConfig) {
					esd->decoderConfig->avgBitrate = 0;
					esd->decoderConfig->bufferSizeDB = samp->dataLength;
					esd->decoderConfig->maxBitrate = 0;
				}
				esd->URLString = (char*)gf_malloc(sizeof(char) * (strlen(sdpLine)+1));
				strcpy(esd->URLString, sdpLine);
			} else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_RTP, ("[rtp hinter] Scene description sample too large to be embedded in IOD - ISMA disabled\n"));
				is_ok = 0;
			}
			gf_isom_sample_del(&samp);
		}
		if (remove_ocr) esd->OCRESID = 0;
		else if (esd->OCRESID == esd->ESID) esd->OCRESID = 0;

		gf_list_add(iod->ESDescriptors, esd);

		if (is_ok) {
			u32 has_a, has_v, has_i_a, has_i_v;
			has_a = has_v = has_i_a = has_i_v = 0;
			for (i=0; i<gf_isom_get_track_count(file); i++) {
				esd = gf_isom_get_esd(file, i+1, 1);
				if (!esd) continue;
				if (esd->decoderConfig) {
					if (esd->decoderConfig->streamType==GF_STREAM_VISUAL) {
						if (esd->decoderConfig->objectTypeIndication==GF_CODECID_MPEG4_PART2) has_i_v ++;
						else has_v++;
					} else if (esd->decoderConfig->streamType==GF_STREAM_AUDIO) {
						if (esd->decoderConfig->objectTypeIndication==GF_CODECID_AAC_MPEG4) has_i_a ++;
						else has_a++;
					}
				}
				gf_odf_desc_del((GF_Descriptor *)esd);
			}
			/*only 1 MPEG-4 visual max and 1 MPEG-4 audio max for ISMA compliancy*/
			if (!has_v && !has_a && (has_i_v<=1) && (has_i_a<=1)) {
				sprintf(sdpLine, "a=isma-compliance:1,1.0,1");
				gf_isom_sdp_add_line(file, sdpLine);
			}
		}
	}

	//encode the IOD
	buffer = NULL;
	size = 0;
	gf_odf_desc_write((GF_Descriptor *) iod, &buffer, &size);
	gf_odf_desc_del((GF_Descriptor *)iod);

	//encode in Base64 the iod
	size64 = gf_base64_encode(buffer, size, buf64, 2000);
	buf64[size64] = 0;
	gf_free(buffer);

	sprintf(sdpLine, "a=mpeg4-iod:\"data:application/mpeg4-iod;base64,%s\"", buf64);
	gf_isom_sdp_add_line(file, sdpLine);

	return GF_OK;
}