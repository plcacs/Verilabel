bool PxMDecoder::readData( Mat& img )
{
    int color = img.channels() > 1;
    uchar* data = img.ptr();
    PaletteEntry palette[256];
    bool   result = false;
    const int bit_depth = CV_ELEM_SIZE1(m_type)*8;
    const int src_pitch = divUp(m_width*m_bpp*(bit_depth/8), 8);
    int  nch = CV_MAT_CN(m_type);
    int  width3 = m_width*nch;

    if( m_offset < 0 || !m_strm.isOpened())
        return false;

    uchar gray_palette[256] = {0};

    // create LUT for converting colors
    if( bit_depth == 8 )
    {
        CV_Assert(m_maxval < 256);

        for (int i = 0; i <= m_maxval; i++)
            gray_palette[i] = (uchar)((i*255/m_maxval)^(m_bpp == 1 ? 255 : 0));

        FillGrayPalette( palette, m_bpp==1 ? 1 : 8 , m_bpp == 1 );
    }

    CV_TRY
    {
        m_strm.setPos( m_offset );

        switch( m_bpp )
        {
        ////////////////////////// 1 BPP /////////////////////////
        case 1:
            CV_Assert(CV_MAT_DEPTH(m_type) == CV_8U);
            if( !m_binary )
            {
                AutoBuffer<uchar> _src(m_width);
                uchar* src = _src;

                for (int y = 0; y < m_height; y++, data += img.step)
                {
                    for (int x = 0; x < m_width; x++)
                        src[x] = ReadNumber(m_strm, 1) != 0;

                    if( color )
                        FillColorRow8( data, src, m_width, palette );
                    else
                        FillGrayRow8( data, src, m_width, gray_palette );
                }
            }
            else
            {
                AutoBuffer<uchar> _src(src_pitch);
                uchar* src = _src;

                for (int y = 0; y < m_height; y++, data += img.step)
                {
                    m_strm.getBytes( src, src_pitch );

                    if( color )
                        FillColorRow1( data, src, m_width, palette );
                    else
                        FillGrayRow1( data, src, m_width, gray_palette );
                }
            }
            result = true;
            break;

        ////////////////////////// 8 BPP /////////////////////////
        case 8:
        case 24:
        {
            AutoBuffer<uchar> _src(std::max<size_t>(width3*2, src_pitch));
            uchar* src = _src;

            for (int y = 0; y < m_height; y++, data += img.step)
            {
                if( !m_binary )
                {
                    for (int x = 0; x < width3; x++)
                    {
                        int code = ReadNumber(m_strm);
                        if( (unsigned)code > (unsigned)m_maxval ) code = m_maxval;
                        if( bit_depth == 8 )
                            src[x] = gray_palette[code];
                        else
                            ((ushort *)src)[x] = (ushort)code;
                    }
                }
                else
                {
                    m_strm.getBytes( src, src_pitch );
                    if( bit_depth == 16 && !isBigEndian() )
                    {
                        for (int x = 0; x < width3; x++)
                        {
                            uchar v = src[x * 2];
                            src[x * 2] = src[x * 2 + 1];
                            src[x * 2 + 1] = v;
                        }
                    }
                }

                if( img.depth() == CV_8U && bit_depth == 16 )
                {
                    for (int x = 0; x < width3; x++)
                    {
                        int v = ((ushort *)src)[x];
                        src[x] = (uchar)(v >> 8);
                    }
                }

                if( m_bpp == 8 ) // image has one channel
                {
                    if( color )
                    {
                        if( img.depth() == CV_8U ) {
                            uchar *d = data, *s = src, *end = src + m_width;
                            for( ; s < end; d += 3, s++)
                                d[0] = d[1] = d[2] = *s;
                        } else {
                            ushort *d = (ushort *)data, *s = (ushort *)src, *end = ((ushort *)src) + m_width;
                            for( ; s < end; s++, d += 3)
                                d[0] = d[1] = d[2] = *s;
                        }
                    }
                    else
                        memcpy( data, src, CV_ELEM_SIZE1(m_type)*m_width);
                }
                else
                {
                    if( color )
                    {
                        if( img.depth() == CV_8U )
                            icvCvt_RGB2BGR_8u_C3R( src, 0, data, 0, cvSize(m_width,1) );
                        else
                            icvCvt_RGB2BGR_16u_C3R( (ushort *)src, 0, (ushort *)data, 0, cvSize(m_width,1) );
                    }
                    else if( img.depth() == CV_8U )
                        icvCvt_BGR2Gray_8u_C3C1R( src, 0, data, 0, cvSize(m_width,1), 2 );
                    else
                        icvCvt_BGRA2Gray_16u_CnC1R( (ushort *)src, 0, (ushort *)data, 0, cvSize(m_width,1), 3, 2 );
                }
            }
            result = true;
            break;
        }
        default:
            CV_ErrorNoReturn(Error::StsError, "m_bpp is not supported");
        }
    }
    CV_CATCH (cv::Exception, e)
    {
        CV_UNUSED(e);
        CV_RETHROW();
    }
    CV_CATCH_ALL
    {
        std::cerr << "PXM::readData(): unknown exception" << std::endl << std::flush;
        CV_RETHROW();
    }

    return result;
}