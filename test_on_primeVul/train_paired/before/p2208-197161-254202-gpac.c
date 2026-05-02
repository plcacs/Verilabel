int avi_parse_input_file(avi_t *AVI, int getIndex)
{
	int i, rate, scale, idx_type;
	s64 n;
	unsigned char *hdrl_data;
	u64 header_offset=0;
	int hdrl_len=0;
	int nvi, nai[AVI_MAX_TRACKS], ioff;
	u64 tot[AVI_MAX_TRACKS];
	u32 j;
	int lasttag = 0;
	int vids_strh_seen = 0;
	int vids_strf_seen = 0;
	int auds_strh_seen = 0;
	//  int auds_strf_seen = 0;
	int num_stream = 0;
	char data[256];
	s64 oldpos=-1, newpos=-1;

	int aud_chunks = 0;
	if (!AVI) {
	   AVI_errno = AVI_ERR_OPEN;
	   return 0;
	}

	/* Read first 12 bytes and check that this is an AVI file */
	if (avi_read(AVI->fdes,data,12) != 12 )
		ERR_EXIT(AVI_ERR_READ)

	if (strnicmp(data  ,"RIFF",4) !=0 || strnicmp(data+8,"AVI ",4) !=0 )
		ERR_EXIT(AVI_ERR_NO_AVI)

	/* Go through the AVI file and extract the header list,
	   the start position of the 'movi' list and an optionally
	   present idx1 tag */

	hdrl_data = 0;

	while(1)
	{
		if( avi_read(AVI->fdes,data,8) != 8 ) break; /* We assume it's EOF */
		newpos = gf_ftell(AVI->fdes);
		if(oldpos==newpos) {
			/* This is a broken AVI stream... */
			return -1;
		}
		oldpos=newpos;

		n = str2ulong((unsigned char *)data+4);
		n = PAD_EVEN(n);

		if(strnicmp(data,"LIST",4) == 0)
		{
			if( avi_read(AVI->fdes,data,4) != 4 ) ERR_EXIT(AVI_ERR_READ)
				n -= 4;
			if(strnicmp(data,"hdrl",4) == 0)
			{
				hdrl_len = (u32) n;
				hdrl_data = (unsigned char *) gf_malloc((u32)n);
				if(hdrl_data==0) ERR_EXIT(AVI_ERR_NO_MEM);

				// offset of header

				header_offset = gf_ftell(AVI->fdes);

				if( avi_read(AVI->fdes,(char *)hdrl_data, (u32) n) != n ) ERR_EXIT(AVI_ERR_READ)
				}
			else if(strnicmp(data,"movi",4) == 0)
			{
				AVI->movi_start = gf_ftell(AVI->fdes);
				if (gf_fseek(AVI->fdes,n,SEEK_CUR)==(u64)-1) break;
			}
			else if (gf_fseek(AVI->fdes,n,SEEK_CUR)==(u64)-1) break;
		}
		else if(strnicmp(data,"idx1",4) == 0)
		{
			/* n must be a multiple of 16, but the reading does not
			   break if this is not the case */

			AVI->n_idx = AVI->max_idx = (u32) (n/16);
			AVI->idx = (unsigned  char((*)[16]) ) gf_malloc((u32)n);
			if(AVI->idx==0) ERR_EXIT(AVI_ERR_NO_MEM)
				if(avi_read(AVI->fdes, (char *) AVI->idx, (u32) n) != n ) {
					gf_free( AVI->idx);
					AVI->idx=NULL;
					AVI->n_idx = 0;
				}
		}
		else
			gf_fseek(AVI->fdes,n,SEEK_CUR);
	}

	if(!hdrl_data      ) ERR_EXIT(AVI_ERR_NO_HDRL)
		if(!AVI->movi_start) ERR_EXIT(AVI_ERR_NO_MOVI)

			/* Interpret the header list */

			for(i=0; i<hdrl_len;)
			{
				/* List tags are completly ignored */

#ifdef DEBUG_ODML
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] TAG %c%c%c%c\n", (hdrl_data+i)[0], (hdrl_data+i)[1], (hdrl_data+i)[2], (hdrl_data+i)[3]));
#endif

				if(strnicmp((char *)hdrl_data+i,"LIST",4)==0) {
					i+= 12;
					continue;
				}

				n = str2ulong(hdrl_data+i+4);
				n = PAD_EVEN(n);


				/* Interpret the tag and its args */

				if(strnicmp((char *)hdrl_data+i,"strh",4)==0)
				{
					i += 8;
#ifdef DEBUG_ODML
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] TAG   %c%c%c%c\n", (hdrl_data+i)[0], (hdrl_data+i)[1], (hdrl_data+i)[2], (hdrl_data+i)[3]));
#endif
					if(strnicmp((char *)hdrl_data+i,"vids",4) == 0 && !vids_strh_seen)
					{
						memcpy(AVI->compressor,hdrl_data+i+4,4);
						AVI->compressor[4] = 0;

						// ThOe
						AVI->v_codech_off = header_offset + i+4;

						scale = str2ulong(hdrl_data+i+20);
						rate  = str2ulong(hdrl_data+i+24);
						if(scale!=0) AVI->fps = (double)rate/(double)scale;
						AVI->video_frames = str2ulong(hdrl_data+i+32);
						AVI->video_strn = num_stream;
						AVI->max_len = 0;
						vids_strh_seen = 1;
						lasttag = 1; /* vids */
						memcpy(&AVI->video_stream_header, hdrl_data + i,
						       sizeof(alAVISTREAMHEADER));
					}
					else if (strnicmp ((char *)hdrl_data+i,"auds",4) ==0 && ! auds_strh_seen)
					{

						//inc audio tracks
						AVI->aptr=AVI->anum;
						++AVI->anum;

						if(AVI->anum > AVI_MAX_TRACKS) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] error - only %d audio tracks supported\n", AVI_MAX_TRACKS));
							return(-1);
						}

						AVI->track[AVI->aptr].audio_bytes = str2ulong(hdrl_data+i+32)*avi_sampsize(AVI, 0);
						AVI->track[AVI->aptr].audio_strn = num_stream;

						// if samplesize==0 -> vbr
						AVI->track[AVI->aptr].a_vbr = !str2ulong(hdrl_data+i+44);

						AVI->track[AVI->aptr].padrate = str2ulong(hdrl_data+i+24);
						memcpy(&AVI->stream_headers[AVI->aptr], hdrl_data + i,
						       sizeof(alAVISTREAMHEADER));

						//	   auds_strh_seen = 1;
						lasttag = 2; /* auds */

						// ThOe
						AVI->track[AVI->aptr].a_codech_off = header_offset + i;

					}
					else if (strnicmp ((char*)hdrl_data+i,"iavs",4) ==0 && ! auds_strh_seen) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] AVILIB: error - DV AVI Type 1 no supported\n"));
						return (-1);
					}
					else
						lasttag = 0;
					num_stream++;
				}
				else if(strnicmp((char*)hdrl_data+i,"dmlh",4) == 0) {
					AVI->total_frames = str2ulong(hdrl_data+i+8);
#ifdef DEBUG_ODML
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] real number of frames %d\n", AVI->total_frames));
#endif
					i += 8;
				}
				else if(strnicmp((char *)hdrl_data+i,"strf",4)==0)
				{
					i += 8;
					if(lasttag == 1)
					{
						alBITMAPINFOHEADER bih;

						memcpy(&bih, hdrl_data + i, sizeof(alBITMAPINFOHEADER));
						AVI->bitmap_info_header = (alBITMAPINFOHEADER *)
						                          gf_malloc(str2ulong((unsigned char *)&bih.bi_size));
						if (AVI->bitmap_info_header != NULL)
							memcpy(AVI->bitmap_info_header, hdrl_data + i,
							       str2ulong((unsigned char *)&bih.bi_size));

						AVI->width  = str2ulong(hdrl_data+i+4);
						AVI->height = str2ulong(hdrl_data+i+8);
						vids_strf_seen = 1;
						//ThOe
						AVI->v_codecf_off = header_offset + i+16;

						memcpy(AVI->compressor2, hdrl_data+i+16, 4);
						AVI->compressor2[4] = 0;

						if (n>40) {
							AVI->extradata_size = (u32) (n - 40);
							AVI->extradata = gf_malloc(sizeof(u8)* AVI->extradata_size);
							memcpy(AVI->extradata, hdrl_data + i + 40, AVI->extradata_size);
						}

					}
					else if(lasttag == 2)
					{
						alWAVEFORMATEX *wfe;
						char *nwfe;
						int wfes;

						if ((u32) (hdrl_len - i) < sizeof(alWAVEFORMATEX))
							wfes = hdrl_len - i;
						else
							wfes = sizeof(alWAVEFORMATEX);
						wfe = (alWAVEFORMATEX *)gf_malloc(sizeof(alWAVEFORMATEX));
						if (wfe != NULL) {
							memset(wfe, 0, sizeof(alWAVEFORMATEX));
							memcpy(wfe, hdrl_data + i, wfes);
							if (str2ushort((unsigned char *)&wfe->cb_size) != 0) {
								nwfe = (char *)
								       gf_realloc(wfe, sizeof(alWAVEFORMATEX) +
								                  str2ushort((unsigned char *)&wfe->cb_size));
								if (nwfe != 0) {
									s64 lpos = gf_ftell(AVI->fdes);
									gf_fseek(AVI->fdes, header_offset + i + sizeof(alWAVEFORMATEX),
									         SEEK_SET);
									wfe = (alWAVEFORMATEX *)nwfe;
									nwfe = &nwfe[sizeof(alWAVEFORMATEX)];
									avi_read(AVI->fdes, nwfe,
									         str2ushort((unsigned char *)&wfe->cb_size));
									gf_fseek(AVI->fdes, lpos, SEEK_SET);
								}
							}
							AVI->wave_format_ex[AVI->aptr] = wfe;
						}

						AVI->track[AVI->aptr].a_fmt   = str2ushort(hdrl_data+i  );

						//ThOe
						AVI->track[AVI->aptr].a_codecf_off = header_offset + i;

						AVI->track[AVI->aptr].a_chans = str2ushort(hdrl_data+i+2);
						AVI->track[AVI->aptr].a_rate  = str2ulong (hdrl_data+i+4);
						//ThOe: read mp3bitrate
						AVI->track[AVI->aptr].mp3rate = 8*str2ulong(hdrl_data+i+8)/1000;
						//:ThOe
						AVI->track[AVI->aptr].a_bits  = str2ushort(hdrl_data+i+14);
						//            auds_strf_seen = 1;
					}
				}
				else if(strnicmp((char*)hdrl_data+i,"indx",4) == 0) {
					char *a;

					if(lasttag == 1) // V I D E O
					{

						a = (char*)hdrl_data+i;

						AVI->video_superindex = (avisuperindex_chunk *) gf_malloc (sizeof (avisuperindex_chunk));
						memset(AVI->video_superindex, 0, sizeof (avisuperindex_chunk));
						memcpy (AVI->video_superindex->fcc, a, 4);
						a += 4;
						AVI->video_superindex->dwSize = str2ulong((unsigned char *)a);
						a += 4;
						AVI->video_superindex->wLongsPerEntry = str2ushort((unsigned char *)a);
						a += 2;
						AVI->video_superindex->bIndexSubType = *a;
						a += 1;
						AVI->video_superindex->bIndexType = *a;
						a += 1;
						AVI->video_superindex->nEntriesInUse = str2ulong((unsigned char *)a);
						a += 4;
						memcpy (AVI->video_superindex->dwChunkId, a, 4);
						a += 4;

						// 3 * reserved
						a += 4;
						a += 4;
						a += 4;

						if (AVI->video_superindex->bIndexSubType != 0) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Invalid Header, bIndexSubType != 0\n"));
						}

						AVI->video_superindex->aIndex = (avisuperindex_entry*)
						                                gf_malloc (AVI->video_superindex->wLongsPerEntry * AVI->video_superindex->nEntriesInUse * sizeof (u32));

						// position of ix## chunks
						for (j=0; j<AVI->video_superindex->nEntriesInUse; ++j) {
							AVI->video_superindex->aIndex[j].qwOffset = str2ullong ((unsigned char*)a);
							a += 8;
							AVI->video_superindex->aIndex[j].dwSize = str2ulong ((unsigned char*)a);
							a += 4;
							AVI->video_superindex->aIndex[j].dwDuration = str2ulong ((unsigned char*)a);
							a += 4;

#ifdef DEBUG_ODML
							GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d] 0x%llx 0x%lx %lu\n", j,
							                                        (unsigned int long)AVI->video_superindex->aIndex[j].qwOffset,
							                                        (unsigned long)AVI->video_superindex->aIndex[j].dwSize,
							                                        (unsigned long)AVI->video_superindex->aIndex[j].dwDuration));
#endif
						}


#ifdef DEBUG_ODML
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] FOURCC \"%c%c%c%c\"\n", AVI->video_superindex->fcc[0], AVI->video_superindex->fcc[1],
						                                        AVI->video_superindex->fcc[2], AVI->video_superindex->fcc[3]));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] LEN \"%ld\"\n", (long)AVI->video_superindex->dwSize));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] wLongsPerEntry \"%d\"\n", AVI->video_superindex->wLongsPerEntry));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexSubType \"%d\"\n", AVI->video_superindex->bIndexSubType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexType \"%d\"\n", AVI->video_superindex->bIndexType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] nEntriesInUse \"%ld\"\n", (long)AVI->video_superindex->nEntriesInUse));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] dwChunkId \"%c%c%c%c\"\n", AVI->video_superindex->dwChunkId[0], AVI->video_superindex->dwChunkId[1],
						                                        AVI->video_superindex->dwChunkId[2], AVI->video_superindex->dwChunkId[3]));
#endif

						AVI->is_opendml = 1;

					}
					else if(lasttag == 2) // A U D I O
					{

						a = (char*) hdrl_data+i;

						AVI->track[AVI->aptr].audio_superindex = (avisuperindex_chunk *) gf_malloc (sizeof (avisuperindex_chunk));
						memcpy (AVI->track[AVI->aptr].audio_superindex->fcc, a, 4);
						a += 4;
						AVI->track[AVI->aptr].audio_superindex->dwSize = str2ulong((unsigned char*)a);
						a += 4;
						AVI->track[AVI->aptr].audio_superindex->wLongsPerEntry = str2ushort((unsigned char*)a);
						a += 2;
						AVI->track[AVI->aptr].audio_superindex->bIndexSubType = *a;
						a += 1;
						AVI->track[AVI->aptr].audio_superindex->bIndexType = *a;
						a += 1;
						AVI->track[AVI->aptr].audio_superindex->nEntriesInUse = str2ulong((unsigned char*)a);
						a += 4;
						memcpy (AVI->track[AVI->aptr].audio_superindex->dwChunkId, a, 4);
						a += 4;

						// 3 * reserved
						a += 4;
						a += 4;
						a += 4;

						if (AVI->track[AVI->aptr].audio_superindex->bIndexSubType != 0) {
							GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Invalid Header, bIndexSubType != 0\n"));
						}

						AVI->track[AVI->aptr].audio_superindex->aIndex = (avisuperindex_entry*)
						        gf_malloc (AVI->track[AVI->aptr].audio_superindex->wLongsPerEntry *
						                   AVI->track[AVI->aptr].audio_superindex->nEntriesInUse * sizeof (u32));

						// position of ix## chunks
						for (j=0; j<AVI->track[AVI->aptr].audio_superindex->nEntriesInUse; ++j) {
							AVI->track[AVI->aptr].audio_superindex->aIndex[j].qwOffset = str2ullong ((unsigned char*)a);
							a += 8;
							AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwSize = str2ulong ((unsigned char*)a);
							a += 4;
							AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwDuration = str2ulong ((unsigned char*)a);
							a += 4;

#ifdef DEBUG_ODML
							GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d] 0x%llx 0x%lx %lu\n", j,
							                                        (unsigned int long)AVI->track[AVI->aptr].audio_superindex->aIndex[j].qwOffset,
							                                        (unsigned long)AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwSize,
							                                        (unsigned long)AVI->track[AVI->aptr].audio_superindex->aIndex[j].dwDuration));
#endif
						}

						AVI->track[AVI->aptr].audio_superindex->stdindex = NULL;

#ifdef DEBUG_ODML
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] FOURCC \"%.4s\"\n", AVI->track[AVI->aptr].audio_superindex->fcc));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] LEN \"%ld\"\n", (long)AVI->track[AVI->aptr].audio_superindex->dwSize));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] wLongsPerEntry \"%d\"\n", AVI->track[AVI->aptr].audio_superindex->wLongsPerEntry));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexSubType \"%d\"\n", AVI->track[AVI->aptr].audio_superindex->bIndexSubType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] bIndexType \"%d\"\n", AVI->track[AVI->aptr].audio_superindex->bIndexType));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] nEntriesInUse \"%ld\"\n", (long)AVI->track[AVI->aptr].audio_superindex->nEntriesInUse));
						GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] dwChunkId \"%.4s\"\n", AVI->track[AVI->aptr].audio_superindex->dwChunkId[0]));
#endif

					}
					i += 8;
				}
				else if((strnicmp((char*)hdrl_data+i,"JUNK",4) == 0) ||
				        (strnicmp((char*)hdrl_data+i,"strn",4) == 0) ||
				        (strnicmp((char*)hdrl_data+i,"vprp",4) == 0)) {
					i += 8;
					// do not reset lasttag
				} else
				{
					i += 8;
					lasttag = 0;
				}
				//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] adding %ld bytes\n", (int int)n));

				i += (u32) n;
			}

	gf_free(hdrl_data);

	if(!vids_strh_seen || !vids_strf_seen) ERR_EXIT(AVI_ERR_NO_VIDS)

		AVI->video_tag[0] = AVI->video_strn/10 + '0';
	AVI->video_tag[1] = AVI->video_strn%10 + '0';
	AVI->video_tag[2] = 'd';
	AVI->video_tag[3] = 'b';

	/* Audio tag is set to "99wb" if no audio present */
	if(!AVI->track[0].a_chans) AVI->track[0].audio_strn = 99;

	{
		int tk=0;
		for(j=0; j<AVI->anum+1; ++j) {
			if (j == AVI->video_strn) continue;
			AVI->track[tk].audio_tag[0] = j/10 + '0';
			AVI->track[tk].audio_tag[1] = j%10 + '0';
			AVI->track[tk].audio_tag[2] = 'w';
			AVI->track[tk].audio_tag[3] = 'b';
			++tk;
		}
	}

	gf_fseek(AVI->fdes,AVI->movi_start,SEEK_SET);

	if(!getIndex) return(0);

	/* if the file has an idx1, check if this is relative
	   to the start of the file or to the start of the movi list */

	idx_type = 0;

	if(AVI->idx)
	{
		s64 pos, len;

		/* Search the first videoframe in the idx1 and look where
		   it is in the file */

		for(i=0; i<AVI->n_idx; i++)
			if( strnicmp((char *)AVI->idx[i],(char *)AVI->video_tag,3)==0 ) break;
		if(i>=AVI->n_idx) ERR_EXIT(AVI_ERR_NO_VIDS)

			pos = str2ulong(AVI->idx[i]+ 8);
		len = str2ulong(AVI->idx[i]+12);

		gf_fseek(AVI->fdes,pos,SEEK_SET);
		if(avi_read(AVI->fdes,data,8)!=8) ERR_EXIT(AVI_ERR_READ)
			if( strnicmp(data,(char *)AVI->idx[i],4)==0 && str2ulong((unsigned char *)data+4)==len )
			{
				idx_type = 1; /* Index from start of file */
			}
			else
			{
				gf_fseek(AVI->fdes,pos+AVI->movi_start-4,SEEK_SET);
				if(avi_read(AVI->fdes,data,8)!=8) ERR_EXIT(AVI_ERR_READ)
					if( strnicmp(data,(char *)AVI->idx[i],4)==0 && str2ulong((unsigned char *)data+4)==len )
					{
						idx_type = 2; /* Index from start of movi list */
					}
			}
		/* idx_type remains 0 if neither of the two tests above succeeds */
	}


	if(idx_type == 0 && !AVI->is_opendml && !AVI->total_frames)
	{
		/* we must search through the file to get the index */

		gf_fseek(AVI->fdes, AVI->movi_start, SEEK_SET);

		AVI->n_idx = 0;

		while(1)
		{
			if( avi_read(AVI->fdes,data,8) != 8 ) break;
			n = str2ulong((unsigned char *)data+4);

			/* The movi list may contain sub-lists, ignore them */

			if(strnicmp(data,"LIST",4)==0)
			{
				gf_fseek(AVI->fdes,4,SEEK_CUR);
				continue;
			}

			/* Check if we got a tag ##db, ##dc or ##wb */

			if( ( (data[2]=='d' || data[2]=='D') &&
			        (data[3]=='b' || data[3]=='B' || data[3]=='c' || data[3]=='C') )
			        || ( (data[2]=='w' || data[2]=='W') &&
			             (data[3]=='b' || data[3]=='B') ) )
			{
				u64 __pos = gf_ftell(AVI->fdes) - 8;
				avi_add_index_entry(AVI,(unsigned char *)data,0,__pos,n);
			}

			gf_fseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);
		}
		idx_type = 1;
	}

	// ************************
	// OPENDML
	// ************************

	// read extended index chunks
	if (AVI->is_opendml) {
		u64 offset = 0;
		hdrl_len = 4+4+2+1+1+4+4+8+4;
		char *en, *chunk_start;
		int k = 0;
		u32 audtr = 0;
		u32 nrEntries = 0;

		AVI->video_index = NULL;

		nvi = 0;
		for(audtr=0; audtr<AVI->anum; ++audtr) {
			nai[audtr] = 0;
			tot[audtr] = 0;
		}

		// ************************
		// VIDEO
		// ************************

		for (j=0; j<AVI->video_superindex->nEntriesInUse; j++) {

			// read from file
			chunk_start = en = (char*) gf_malloc ((u32) (AVI->video_superindex->aIndex[j].dwSize+hdrl_len) );

			if (gf_fseek(AVI->fdes, AVI->video_superindex->aIndex[j].qwOffset, SEEK_SET) == (u64)-1) {
				gf_free(chunk_start);
				continue;
			}

			if (avi_read(AVI->fdes, en, (u32) (AVI->video_superindex->aIndex[j].dwSize+hdrl_len) ) <= 0) {
				gf_free(chunk_start);
				continue;
			}

			nrEntries = str2ulong((unsigned char*)en + 12);
#ifdef DEBUG_ODML
			//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d:0] Video nrEntries %ld\n", j, nrEntries));
#endif
			offset = str2ullong((unsigned char*)en + 20);

			// skip header
			en += hdrl_len;
			nvi += nrEntries;
			AVI->video_index = (video_index_entry *) gf_realloc (AVI->video_index, nvi * sizeof (video_index_entry));
			if (!AVI->video_index) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] out of mem (size = %ld)\n", nvi * sizeof (video_index_entry)));
				exit(1);
			}

			while (k < nvi) {

				AVI->video_index[k].pos = offset + str2ulong((unsigned char*)en);
				en += 4;
				AVI->video_index[k].len = str2ulong_len((unsigned char*)en);
				AVI->video_index[k].key = str2ulong_key((unsigned char*)en);
				en += 4;

				// completely empty chunk
				if (AVI->video_index[k].pos-offset == 0 && AVI->video_index[k].len == 0) {
					k--;
					nvi--;
				}

#ifdef DEBUG_ODML
				/*
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d] POS 0x%llX len=%d key=%s offset (%llx) (%ld)\n", k,
				  AVI->video_index[k].pos,
				  (int)AVI->video_index[k].len,
				  AVI->video_index[k].key?"yes":"no ", offset,
				  AVI->video_superindex->aIndex[j].dwSize));
				  */
#endif

				k++;
			}

			gf_free(chunk_start);
		}

		AVI->video_frames = nvi;
		// this should deal with broken 'rec ' odml files.
		if (AVI->video_frames == 0) {
			AVI->is_opendml=0;
			goto multiple_riff;
		}

		// ************************
		// AUDIO
		// ************************

		for(audtr=0; audtr<AVI->anum; ++audtr) {

			k = 0;
			if (!AVI->track[audtr].audio_superindex) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] (%s) cannot read audio index for track %d\n", __FILE__, audtr));
				continue;
			}
			for (j=0; j<AVI->track[audtr].audio_superindex->nEntriesInUse; j++) {

				// read from file
				chunk_start = en = (char*)gf_malloc ((u32) (AVI->track[audtr].audio_superindex->aIndex[j].dwSize+hdrl_len));

				if (gf_fseek(AVI->fdes, AVI->track[audtr].audio_superindex->aIndex[j].qwOffset, SEEK_SET) == (u64)-1) {
					gf_free(chunk_start);
					continue;
				}

				if (avi_read(AVI->fdes, en, (u32) (AVI->track[audtr].audio_superindex->aIndex[j].dwSize+hdrl_len)) <= 0) {
					gf_free(chunk_start);
					continue;
				}

				nrEntries = str2ulong((unsigned char*)en + 12);
				//if (nrEntries > 50) nrEntries = 2; // XXX
#ifdef DEBUG_ODML
				//GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d:%d] Audio nrEntries %ld\n", j, audtr, nrEntries));
#endif
				offset = str2ullong((unsigned char*)en + 20);

				// skip header
				en += hdrl_len;
				nai[audtr] += nrEntries;
				AVI->track[audtr].audio_index = (audio_index_entry *) gf_realloc (AVI->track[audtr].audio_index, nai[audtr] * sizeof (audio_index_entry));

				while (k < nai[audtr]) {

					AVI->track[audtr].audio_index[k].pos = offset + str2ulong((unsigned char*)en);
					en += 4;
					AVI->track[audtr].audio_index[k].len = str2ulong_len((unsigned char*)en);
					en += 4;
					AVI->track[audtr].audio_index[k].tot = tot[audtr];
					tot[audtr] += AVI->track[audtr].audio_index[k].len;

#ifdef DEBUG_ODML
					/*
					GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] [%d:%d] POS 0x%llX len=%d offset (%llx) (%ld)\n", k, audtr,
					  AVI->track[audtr].audio_index[k].pos,
					  (int)AVI->track[audtr].audio_index[k].len,
					  offset, AVI->track[audtr].audio_superindex->aIndex[j].dwSize));
					  */
#endif

					++k;
				}

				gf_free(chunk_start);
			}

			AVI->track[audtr].audio_chunks = nai[audtr];
			AVI->track[audtr].audio_bytes = tot[audtr];
		}
	} // is opendml

	else if (AVI->total_frames && !AVI->is_opendml && idx_type==0) {

		// *********************
		// MULTIPLE RIFF CHUNKS (and no index)
		// *********************

multiple_riff:

		gf_fseek(AVI->fdes, AVI->movi_start, SEEK_SET);

		AVI->n_idx = 0;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] Reconstructing index..."));

		// Number of frames; only one audio track supported
		nvi = AVI->video_frames = AVI->total_frames;
		nai[0] = AVI->track[0].audio_chunks = AVI->total_frames;
		for(j=1; j<AVI->anum; ++j) AVI->track[j].audio_chunks = 0;

		AVI->video_index = (video_index_entry *) gf_malloc(nvi*sizeof(video_index_entry));

		if(AVI->video_index==0) ERR_EXIT(AVI_ERR_NO_MEM);

		for(j=0; j<AVI->anum; ++j) {
			if(AVI->track[j].audio_chunks) {
				AVI->track[j].audio_index = (audio_index_entry *) gf_malloc((nai[j]+1)*sizeof(audio_index_entry));
				memset(AVI->track[j].audio_index, 0, (nai[j]+1)*(sizeof(audio_index_entry)));
				if(AVI->track[j].audio_index==0) ERR_EXIT(AVI_ERR_NO_MEM);
			}
		}

		nvi = 0;
		for(j=0; j<AVI->anum; ++j) {
			nai[j] = 0;
			tot[j] = 0;
		}

		aud_chunks = AVI->total_frames;

		while(1)
		{
			if (nvi >= AVI->total_frames) break;

			if( avi_read(AVI->fdes,data,8) != 8 ) break;
			n = str2ulong((unsigned char *)data+4);


			j=0;

			if (aud_chunks - nai[j] -1 <= 0) {
				aud_chunks += AVI->total_frames;
				AVI->track[j].audio_index = (audio_index_entry *)
				                            gf_realloc( AVI->track[j].audio_index, (aud_chunks+1)*sizeof(audio_index_entry));
				if (!AVI->track[j].audio_index) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[avilib] Internal error in avilib -- no mem\n"));
					AVI_errno = AVI_ERR_NO_MEM;
					return -1;
				}
			}

			/* Check if we got a tag ##db, ##dc or ##wb */

			// VIDEO
			if(
			    (data[0]=='0' || data[1]=='0') &&
			    (data[2]=='d' || data[2]=='D') &&
			    (data[3]=='b' || data[3]=='B' || data[3]=='c' || data[3]=='C') ) {

				AVI->video_index[nvi].key = 0x0;
				AVI->video_index[nvi].pos = gf_ftell(AVI->fdes);
				AVI->video_index[nvi].len = (u32) n;

				/*
				GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] Frame %ld pos %"LLD" len %"LLD" key %ld\n",
				    nvi, AVI->video_index[nvi].pos,  AVI->video_index[nvi].len, (long)AVI->video_index[nvi].key));
				    */
				nvi++;
				gf_fseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);
			}

			//AUDIO
			else if(
			    (data[0]=='0' || data[1]=='1') &&
			    (data[2]=='w' || data[2]=='W') &&
			    (data[3]=='b' || data[3]=='B') ) {


				AVI->track[j].audio_index[nai[j]].pos = gf_ftell(AVI->fdes);
				AVI->track[j].audio_index[nai[j]].len = (u32) n;
				AVI->track[j].audio_index[nai[j]].tot = tot[j];
				tot[j] += AVI->track[j].audio_index[nai[j]].len;
				nai[j]++;

				gf_fseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);
			}
			else {
				gf_fseek(AVI->fdes,-4,SEEK_CUR);
			}

		}
		if (nvi < AVI->total_frames) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[avilib] Uh? Some frames seems missing (%ld/%d)\n",
			        nvi,  AVI->total_frames));
		}


		AVI->video_frames = nvi;
		AVI->track[0].audio_chunks = nai[0];

		for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_bytes = tot[j];
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[avilib] done. nvi=%ld nai=%ld tot=%ld\n", nvi, nai[0], tot[0]));

	} // total_frames but no indx chunk (xawtv does this)

	else

	{
		// ******************
		// NO OPENDML
		// ******************

		/* Now generate the video index and audio index arrays */

		nvi = 0;
		for(j=0; j<AVI->anum; ++j) nai[j] = 0;

		for(i=0; i<AVI->n_idx; i++) {

			if(strnicmp((char *)AVI->idx[i],AVI->video_tag,3) == 0) nvi++;

			for(j=0; j<AVI->anum; ++j) if(strnicmp((char *)AVI->idx[i], AVI->track[j].audio_tag,4) == 0) nai[j]++;
		}

		AVI->video_frames = nvi;
		for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_chunks = nai[j];


		if(AVI->video_frames==0) ERR_EXIT(AVI_ERR_NO_VIDS);
		AVI->video_index = (video_index_entry *) gf_malloc(nvi*sizeof(video_index_entry));
		if(AVI->video_index==0) ERR_EXIT(AVI_ERR_NO_MEM);

		for(j=0; j<AVI->anum; ++j) {
			if(AVI->track[j].audio_chunks) {
				AVI->track[j].audio_index = (audio_index_entry *) gf_malloc((nai[j]+1)*sizeof(audio_index_entry));
				memset(AVI->track[j].audio_index, 0, (nai[j]+1)*(sizeof(audio_index_entry)));
				if(AVI->track[j].audio_index==0) ERR_EXIT(AVI_ERR_NO_MEM);
			}
		}

		nvi = 0;
		for(j=0; j<AVI->anum; ++j) {
			nai[j] = 0;
			tot[j] = 0;
		}

		ioff = idx_type == 1 ? 8 : (u32)AVI->movi_start+4;

		for(i=0; i<AVI->n_idx; i++) {

			//video
			if(strnicmp((char *)AVI->idx[i],AVI->video_tag,3) == 0) {
				AVI->video_index[nvi].key = str2ulong(AVI->idx[i]+ 4);
				AVI->video_index[nvi].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
				AVI->video_index[nvi].len = str2ulong(AVI->idx[i]+12);
				nvi++;
			}

			//audio
			for(j=0; j<AVI->anum; ++j) {

				if(strnicmp((char *)AVI->idx[i],AVI->track[j].audio_tag,4) == 0) {
					AVI->track[j].audio_index[nai[j]].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
					AVI->track[j].audio_index[nai[j]].len = str2ulong(AVI->idx[i]+12);
					AVI->track[j].audio_index[nai[j]].tot = tot[j];
					tot[j] += AVI->track[j].audio_index[nai[j]].len;
					nai[j]++;
				}
			}
		}


		for(j=0; j<AVI->anum; ++j) AVI->track[j].audio_bytes = tot[j];

	} // is no opendml

	/* Reposition the file */

	gf_fseek(AVI->fdes,AVI->movi_start,SEEK_SET);
	AVI->video_pos = 0;

	return(0);
}