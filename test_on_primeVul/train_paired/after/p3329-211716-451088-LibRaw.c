int LibRaw::unpack(void)
{
    CHECK_ORDER_HIGH(LIBRAW_PROGRESS_LOAD_RAW);
    CHECK_ORDER_LOW(LIBRAW_PROGRESS_IDENTIFY);
    try {

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

        if (libraw_internal_data.unpacker_data.meta_length) 
            {
                libraw_internal_data.internal_data.meta_data = 
                    (char *) malloc (libraw_internal_data.unpacker_data.meta_length);
                merror (libraw_internal_data.internal_data.meta_data, "LibRaw::unpack()");
            }
        ID.input->seek(libraw_internal_data.unpacker_data.data_offset, SEEK_SET);
        int save_document_mode = O.document_mode;
        O.document_mode = 0;

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
        
        if(decoder_info.decoder_flags &  LIBRAW_DECODER_FLATFIELD)
            {
                imgdata.rawdata.raw_alloc = malloc(rwidth*rheight*sizeof(imgdata.rawdata.raw_image[0]));
                imgdata.rawdata.raw_image = (ushort*) imgdata.rawdata.raw_alloc;
            }
        else if (decoder_info.decoder_flags &  LIBRAW_DECODER_4COMPONENT)
            {
                S.iwidth = S.width;
                S.iheight= S.height;
                IO.shrink = 0;
                imgdata.rawdata.raw_alloc = calloc(rwidth*rheight,sizeof(*imgdata.rawdata.color_image));
                imgdata.rawdata.color_image = (ushort(*)[4]) imgdata.rawdata.raw_alloc;
            }
        else if (decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY)
            {
                // sRAW and Foveon only, so extra buffer size is just 1/4
                // Legacy converters does not supports half mode!
                S.iwidth = S.width;
                S.iheight= S.height;
                IO.shrink = 0;
                // allocate image as temporary buffer, size 
                imgdata.rawdata.raw_alloc = 0;
                imgdata.image = (ushort (*)[4]) calloc(S.iwidth*S.iheight,sizeof(*imgdata.image));
            }


        (this->*load_raw)();


        // recover saved
        if( decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY)
            {
              imgdata.rawdata.raw_alloc = imgdata.rawdata.color_image = imgdata.image;
              imgdata.image = 0; 
            }

        // calculate channel maximum
        {
            for(int c=0;c<4;c++) C.channel_maximum[c] = 0;
            if(decoder_info.decoder_flags & LIBRAW_DECODER_LEGACY)
                {
                    for(int rc = 0; rc < S.iwidth*S.iheight; rc++)
                        {
                            if(C.channel_maximum[0]<imgdata.rawdata.color_image[rc][0]) 
                                C.channel_maximum[0]=imgdata.rawdata.color_image[rc][0];
                            if(C.channel_maximum[1]<imgdata.rawdata.color_image[rc][1]) 
                                C.channel_maximum[1]=imgdata.rawdata.color_image[rc][1];
                            if(C.channel_maximum[2]<imgdata.rawdata.color_image[rc][2]) 
                                C.channel_maximum[2]=imgdata.rawdata.color_image[rc][2];
                            if(C.channel_maximum[3]<imgdata.rawdata.color_image[rc][3]) 
                                C.channel_maximum[3]=imgdata.rawdata.color_image[rc][3];
                        }
                }
            else if(decoder_info.decoder_flags &  LIBRAW_DECODER_4COMPONENT)
                {
                    for(int row = S.top_margin; row < S.height+S.top_margin; row++)
                        for(int col = S.left_margin; col < S.width+S.left_margin; col++)
                        {
                            int rc = row*S.raw_width+col;
                            if(C.channel_maximum[0]<imgdata.rawdata.color_image[rc][0]) 
                                C.channel_maximum[0]=imgdata.rawdata.color_image[rc][0];
                            if(C.channel_maximum[1]<imgdata.rawdata.color_image[rc][1]) 
                                C.channel_maximum[1]=imgdata.rawdata.color_image[rc][1];
                            if(C.channel_maximum[2]<imgdata.rawdata.color_image[rc][2]) 
                                C.channel_maximum[2]=imgdata.rawdata.color_image[rc][2];
                            if(C.channel_maximum[3]<imgdata.rawdata.color_image[rc][3]) 
                                C.channel_maximum[3]=imgdata.rawdata.color_image[rc][4];
                        }
                }
            else if (decoder_info.decoder_flags &  LIBRAW_DECODER_FLATFIELD)
                {
                        for(int row = 0; row < S.height; row++)
                            {
                                int colors[4];
                                for (int xx=0;xx<4;xx++)
                                    colors[xx] = COLOR(row,xx);
                                for(int col = 0; col < S.width; col++)
                                    {
                                        int cc = colors[col&3];
                                        if(C.channel_maximum[cc] 
                                           < imgdata.rawdata.raw_image[(row+S.top_margin)*S.raw_width
                                                                       +(col+S.left_margin)])
                                            C.channel_maximum[cc] = 
                                                imgdata.rawdata.raw_image[(row+S.top_margin)*S.raw_width
                                                                          +(col+S.left_margin)];
                                    }
                            }
                }
        }
        // recover image sizes
        S.iwidth = save_iwidth;
        S.iheight = save_iheight;
        IO.shrink = save_shrink;

        // phase-one black
        if(imgdata.rawdata.ph1_black)
            C.ph1_black = imgdata.rawdata.ph1_black;
        O.document_mode = save_document_mode;

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