int LibRaw::unpack(void)
{
    CHECK_ORDER_HIGH(LIBRAW_PROGRESS_LOAD_RAW);
    CHECK_ORDER_LOW(LIBRAW_PROGRESS_IDENTIFY);
    try {

		if(!libraw_internal_data.internal_data.input)
			return LIBRAW_INPUT_CLOSED;

        RUN_CALLBACK(LIBRAW_PROGRESS_LOAD_RAW,0,2);
        if (O.shot_select >= P1.raw_count)
            return LIBRAW_REQUEST_FOR_NONEXISTENT_IMAGE;
        
        if(!load_raw)
            return LIBRAW_UNSPECIFIED_ERROR;
        
        if (O.use_camera_matrix && C.cmatrix[0][0] > 0.25) 
            {
                memcpy (C.rgb_cam, C.cmatrix, sizeof (C.cmatrix));
                IO.raw_color = 0;
            }
        // already allocated ?
        if(imgdata.image)
            {
                free(imgdata.image);
                imgdata.image = 0;
            }
        if(imgdata.rawdata.raw_alloc)
          {
            free(imgdata.rawdata.raw_alloc);
            imgdata.rawdata.raw_alloc = 0;
          }
        if (libraw_internal_data.unpacker_data.meta_length) 
            {
                libraw_internal_data.internal_data.meta_data = 
                    (char *) malloc (libraw_internal_data.unpacker_data.meta_length);
                merror (libraw_internal_data.internal_data.meta_data, "LibRaw::unpack()");
            }

        libraw_decoder_info_t decoder_info;
        get_decoder_info(&decoder_info);

        int save_iwidth = S.iwidth, save_iheight = S.iheight, save_shrink = IO.shrink;

        int rwidth = S.raw_width, rheight = S.raw_height;
        if( !IO.fuji_width)
            {
                // adjust non-Fuji allocation
                if(rwidth < S.width + S.left_margin)
                    rwidth = S.width + S.left_margin;
                if(rheight < S.height + S.top_margin)
                    rheight = S.height + S.top_margin;
            }
        S.raw_pitch = S.raw_width*2;
        imgdata.rawdata.raw_image = 0;
        imgdata.rawdata.color4_image = 0;
	imgdata.rawdata.color3_image = 0;
#ifdef USE_RAWSPEED
        // RawSpeed Supported, 
        if(O.use_rawspeed && (decoder_info.decoder_flags & LIBRAW_DECODER_TRYRAWSPEED) && _rawspeed_camerameta)
          {
            INT64 spos = ID.input->tell();
            try 
              {
                //                printf("Using rawspeed\n");
                ID.input->seek(0,SEEK_SET);
                INT64 _rawspeed_buffer_sz = ID.input->size()+32;
                void *_rawspeed_buffer = malloc(_rawspeed_buffer_sz);
                if(!_rawspeed_buffer) throw LIBRAW_EXCEPTION_ALLOC;
                ID.input->read(_rawspeed_buffer,_rawspeed_buffer_sz,1);
                FileMap map((uchar8*)_rawspeed_buffer,_rawspeed_buffer_sz);
                RawParser t(&map);
                RawDecoder *d = 0;
                CameraMetaDataLR *meta = static_cast<CameraMetaDataLR*>(_rawspeed_camerameta);
                d = t.getDecoder();
                try {
                  d->checkSupport(meta);
                }
                catch (const RawDecoderException& e)
                  {
                    imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED_UNSUPPORTED;
                    throw e;
                  }
                d->decodeRaw();
                d->decodeMetaData(meta);
                RawImage r = d->mRaw;
                if (r->isCFA) {
                  // Save pointer to decoder
                  _rawspeed_decoder = static_cast<void*>(d);
                  imgdata.rawdata.raw_image = (ushort*) r->getDataUncropped(0,0);
                  S.raw_pitch = r->pitch;
                  fix_after_rawspeed(r->blackLevel);
                } else if(r->getCpp()==4) {
                  _rawspeed_decoder = static_cast<void*>(d);
                  imgdata.rawdata.color4_image = (ushort(*)[4]) r->getDataUncropped(0,0);
                  S.raw_pitch = r->pitch;
                  C.maximum = r->whitePoint;
                  fix_after_rawspeed(r->blackLevel);
                } else if(r->getCpp() == 3)
                  {
                    _rawspeed_decoder = static_cast<void*>(d);
                    imgdata.rawdata.color3_image = (ushort(*)[3]) r->getDataUncropped(0,0);
                    S.raw_pitch = r->pitch;
                    C.maximum = r->whitePoint;
                    fix_after_rawspeed(r->blackLevel);
                  }
                else
                  {
                    delete d;
                  }
                free(_rawspeed_buffer);
              imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED_PROCESSED;
              } catch (...) {
              imgdata.process_warnings |= LIBRAW_WARN_RAWSPEED_PROBLEM;
              // no other actions: if raw_image is not set we'll try usual load_raw call
            }
            ID.input->seek(spos,SEEK_SET);
          }
#endif
        if(!imgdata.rawdata.raw_image && !imgdata.rawdata.color4_image && !imgdata.rawdata.color3_image) // RawSpeed failed!
          {
            // Not allocated on RawSpeed call, try call LibRaw
            if(decoder_info.decoder_flags &  LIBRAW_DECODER_FLATFIELD)
              {
                imgdata.rawdata.raw_alloc = malloc(rwidth*(rheight+7)*sizeof(imgdata.rawdata.raw_image[0]));
                imgdata.rawdata.raw_image = (ushort*) imgdata.rawdata.raw_alloc;
              }
            else if (decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY)
              {
                // sRAW and Foveon only, so extra buffer size is just 1/4
                // Legacy converters does not supports half mode!
                S.iwidth = S.width;
                S.iheight= S.height;
                IO.shrink = 0;
				S.raw_pitch = S.width*8;
                // allocate image as temporary buffer, size 
                imgdata.rawdata.raw_alloc = 0;
                imgdata.image = (ushort (*)[4]) calloc(S.iwidth*S.iheight,sizeof(*imgdata.image));
              }
            ID.input->seek(libraw_internal_data.unpacker_data.data_offset, SEEK_SET);

			unsigned m_save = C.maximum;
			if(load_raw == &LibRaw::unpacked_load_raw && !strcasecmp(imgdata.idata.make,"Nikon"))
				C.maximum=65535;
            (this->*load_raw)();
			if(load_raw == &LibRaw::unpacked_load_raw && !strcasecmp(imgdata.idata.make,"Nikon"))
				C.maximum = m_save;
			if (decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY)
			{
				// successfully decoded legacy image, attach image to raw_alloc
				imgdata.rawdata.raw_alloc = imgdata.image;
				imgdata.image = 0; 
			}
          }

        if(imgdata.rawdata.raw_image)
          crop_masked_pixels(); // calculate black levels

        // recover saved
        if( (decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY) && !imgdata.rawdata.color4_image)
            {
                imgdata.image = 0; 
                imgdata.rawdata.color4_image = (ushort (*)[4]) imgdata.rawdata.raw_alloc;
            }

        // recover image sizes
        S.iwidth = save_iwidth;
        S.iheight = save_iheight;
        IO.shrink = save_shrink;

        // adjust black to possible maximum
        unsigned int i = C.cblack[3];
        unsigned int c;
        for(c=0;c<3;c++)
            if (i > C.cblack[c]) i = C.cblack[c];
        for (c=0;c<4;c++)
            C.cblack[c] -= i;
        C.black += i;

        // Save color,sizes and internal data into raw_image fields
        memmove(&imgdata.rawdata.color,&imgdata.color,sizeof(imgdata.color));
        memmove(&imgdata.rawdata.sizes,&imgdata.sizes,sizeof(imgdata.sizes));
        memmove(&imgdata.rawdata.iparams,&imgdata.idata,sizeof(imgdata.idata));
        memmove(&imgdata.rawdata.ioparams,&libraw_internal_data.internal_output_params,sizeof(libraw_internal_data.internal_output_params));

        SET_PROC_FLAG(LIBRAW_PROGRESS_LOAD_RAW);
        RUN_CALLBACK(LIBRAW_PROGRESS_LOAD_RAW,1,2);
        
        return 0;
    }
    catch ( LibRaw_exceptions err) {
        EXCEPTION_HANDLER(err);
    }
    catch (std::exception ee) {
        EXCEPTION_HANDLER(LIBRAW_EXCEPTION_IO_CORRUPT);
    }
}