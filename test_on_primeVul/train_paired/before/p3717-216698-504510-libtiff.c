void t2p_read_tiff_data(T2P* t2p, TIFF* input){

	int i=0;
	uint16* r;
	uint16* g;
	uint16* b;
	uint16* a;
	uint16 xuint16;
	uint16* xuint16p;
	float* xfloatp;

	t2p->pdf_transcode = T2P_TRANSCODE_ENCODE;
	t2p->pdf_sample = T2P_SAMPLE_NOTHING;
        t2p->pdf_switchdecode = t2p->pdf_colorspace_invert;
        
	
	TIFFSetDirectory(input, t2p->tiff_pages[t2p->pdf_page].page_directory);

	TIFFGetField(input, TIFFTAG_IMAGEWIDTH, &(t2p->tiff_width));
	if(t2p->tiff_width == 0){
		TIFFError(
			TIFF2PDF_MODULE, 
			"No support for %s with zero width", 
			TIFFFileName(input)	);
		t2p->t2p_error = T2P_ERR_ERROR;
		return;
	}

	TIFFGetField(input, TIFFTAG_IMAGELENGTH, &(t2p->tiff_length));
	if(t2p->tiff_length == 0){
		TIFFError(
			TIFF2PDF_MODULE, 
			"No support for %s with zero length", 
			TIFFFileName(input)	);
		t2p->t2p_error = T2P_ERR_ERROR;
		return;
	}

        if(TIFFGetField(input, TIFFTAG_COMPRESSION, &(t2p->tiff_compression)) == 0){
                TIFFError(
                        TIFF2PDF_MODULE, 
                        "No support for %s with no compression tag", 
                        TIFFFileName(input)     );
                t2p->t2p_error = T2P_ERR_ERROR;
                return;

        }
        if( TIFFIsCODECConfigured(t2p->tiff_compression) == 0){
		TIFFError(
			TIFF2PDF_MODULE, 
			"No support for %s with compression type %u:  not configured", 
			TIFFFileName(input), 
			t2p->tiff_compression	
			);
		t2p->t2p_error = T2P_ERR_ERROR;
		return;
	
	}

	TIFFGetFieldDefaulted(input, TIFFTAG_BITSPERSAMPLE, &(t2p->tiff_bitspersample));
	switch(t2p->tiff_bitspersample){
		case 1:
		case 2:
		case 4:
		case 8:
			break;
		case 0:
			TIFFWarning(
				TIFF2PDF_MODULE, 
				"Image %s has 0 bits per sample, assuming 1",
				TIFFFileName(input));
			t2p->tiff_bitspersample=1;
			break;
		default:
			TIFFError(
				TIFF2PDF_MODULE, 
				"No support for %s with %u bits per sample",
				TIFFFileName(input),
				t2p->tiff_bitspersample);
			t2p->t2p_error = T2P_ERR_ERROR;
			return;
	}

	TIFFGetFieldDefaulted(input, TIFFTAG_SAMPLESPERPIXEL, &(t2p->tiff_samplesperpixel));
	if(t2p->tiff_samplesperpixel>4){
		TIFFError(
			TIFF2PDF_MODULE, 
			"No support for %s with %u samples per pixel",
			TIFFFileName(input),
			t2p->tiff_samplesperpixel);
		t2p->t2p_error = T2P_ERR_ERROR;
		return;
	}
	if(t2p->tiff_samplesperpixel==0){
		TIFFWarning(
			TIFF2PDF_MODULE, 
			"Image %s has 0 samples per pixel, assuming 1",
			TIFFFileName(input));
		t2p->tiff_samplesperpixel=1;
	}
	
	if(TIFFGetField(input, TIFFTAG_SAMPLEFORMAT, &xuint16) != 0 ){
		switch(xuint16){
			case 0:
			case 1:
			case 4:
				break;
			default:
				TIFFError(
					TIFF2PDF_MODULE, 
					"No support for %s with sample format %u",
					TIFFFileName(input),
					xuint16);
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
				break;
		}
	}
	
	TIFFGetFieldDefaulted(input, TIFFTAG_FILLORDER, &(t2p->tiff_fillorder));
	
        if(TIFFGetField(input, TIFFTAG_PHOTOMETRIC, &(t2p->tiff_photometric)) == 0){
                TIFFError(
                        TIFF2PDF_MODULE, 
                        "No support for %s with no photometric interpretation tag", 
                        TIFFFileName(input)     );
                t2p->t2p_error = T2P_ERR_ERROR;
                return;

        }
        
	switch(t2p->tiff_photometric){
		case PHOTOMETRIC_MINISWHITE:
		case PHOTOMETRIC_MINISBLACK: 
			if (t2p->tiff_bitspersample==1){
				t2p->pdf_colorspace=T2P_CS_BILEVEL;
				if(t2p->tiff_photometric==PHOTOMETRIC_MINISWHITE){
					t2p->pdf_switchdecode ^= 1;
				}
			} else {
				t2p->pdf_colorspace=T2P_CS_GRAY;
				if(t2p->tiff_photometric==PHOTOMETRIC_MINISWHITE){
					t2p->pdf_switchdecode ^= 1;
				} 
			}
			break;
		case PHOTOMETRIC_RGB: 
			t2p->pdf_colorspace=T2P_CS_RGB;
			if(t2p->tiff_samplesperpixel == 3){
				break;
			}
			if(TIFFGetField(input, TIFFTAG_INDEXED, &xuint16)){
				if(xuint16==1)
					goto photometric_palette;
			}
			if(t2p->tiff_samplesperpixel > 3) {
				if(t2p->tiff_samplesperpixel == 4) {
					t2p->pdf_colorspace = T2P_CS_RGB;
					if(TIFFGetField(input,
							TIFFTAG_EXTRASAMPLES,
							&xuint16, &xuint16p)
					   && xuint16 == 1) {
						if(xuint16p[0] == EXTRASAMPLE_ASSOCALPHA){
							if( t2p->tiff_bitspersample != 8 )
							{
							    TIFFError(
								    TIFF2PDF_MODULE, 
								    "No support for BitsPerSample=%d for RGBA",
								    t2p->tiff_bitspersample);
							    t2p->t2p_error = T2P_ERR_ERROR;
							    return;
							}
							t2p->pdf_sample=T2P_SAMPLE_RGBAA_TO_RGB;
							break;
						}
						if(xuint16p[0] == EXTRASAMPLE_UNASSALPHA){
							if( t2p->tiff_bitspersample != 8 )
							{
							    TIFFError(
								    TIFF2PDF_MODULE, 
								    "No support for BitsPerSample=%d for RGBA",
								    t2p->tiff_bitspersample);
							    t2p->t2p_error = T2P_ERR_ERROR;
							    return;
							}
							t2p->pdf_sample=T2P_SAMPLE_RGBA_TO_RGB;
							break;
						}
						TIFFWarning(
							TIFF2PDF_MODULE, 
							"RGB image %s has 4 samples per pixel, assuming RGBA",
							TIFFFileName(input));
							break;
					}
					t2p->pdf_colorspace=T2P_CS_CMYK;
					t2p->pdf_switchdecode ^= 1;
					TIFFWarning(
						TIFF2PDF_MODULE, 
						"RGB image %s has 4 samples per pixel, assuming inverse CMYK",
					TIFFFileName(input));
					break;
				} else {
					TIFFError(
						TIFF2PDF_MODULE, 
						"No support for RGB image %s with %u samples per pixel", 
						TIFFFileName(input), 
						t2p->tiff_samplesperpixel);
					t2p->t2p_error = T2P_ERR_ERROR;
					break;
				}
			} else {
				TIFFError(
					TIFF2PDF_MODULE, 
					"No support for RGB image %s with %u samples per pixel", 
					TIFFFileName(input), 
					t2p->tiff_samplesperpixel);
				t2p->t2p_error = T2P_ERR_ERROR;
				break;
			}
		case PHOTOMETRIC_PALETTE: 
			photometric_palette:
			if(t2p->tiff_samplesperpixel!=1){
				TIFFError(
					TIFF2PDF_MODULE, 
					"No support for palettized image %s with not one sample per pixel", 
					TIFFFileName(input));
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			}
			t2p->pdf_colorspace=T2P_CS_RGB | T2P_CS_PALETTE;
			t2p->pdf_palettesize=0x0001<<t2p->tiff_bitspersample;
			if(!TIFFGetField(input, TIFFTAG_COLORMAP, &r, &g, &b)){
				TIFFError(
					TIFF2PDF_MODULE, 
					"Palettized image %s has no color map", 
					TIFFFileName(input));
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			} 
			if(t2p->pdf_palette != NULL){
				_TIFFfree(t2p->pdf_palette);
				t2p->pdf_palette=NULL;
			}
			t2p->pdf_palette = (unsigned char*)
				_TIFFmalloc(TIFFSafeMultiply(tmsize_t,t2p->pdf_palettesize,3));
			if(t2p->pdf_palette==NULL){
				TIFFError(
					TIFF2PDF_MODULE, 
					"Can't allocate %u bytes of memory for t2p_read_tiff_image, %s", 
					t2p->pdf_palettesize, 
					TIFFFileName(input));
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			}
			for(i=0;i<t2p->pdf_palettesize;i++){
				t2p->pdf_palette[(i*3)]  = (unsigned char) (r[i]>>8);
				t2p->pdf_palette[(i*3)+1]= (unsigned char) (g[i]>>8);
				t2p->pdf_palette[(i*3)+2]= (unsigned char) (b[i]>>8);
			}
			t2p->pdf_palettesize *= 3;
			break;
		case PHOTOMETRIC_SEPARATED:
			if(TIFFGetField(input, TIFFTAG_INDEXED, &xuint16)){
				if(xuint16==1){
						goto photometric_palette_cmyk;
				}
			}
			if( TIFFGetField(input, TIFFTAG_INKSET, &xuint16) ){
				if(xuint16 != INKSET_CMYK){
					TIFFError(
						TIFF2PDF_MODULE, 
						"No support for %s because its inkset is not CMYK",
						TIFFFileName(input) );
					t2p->t2p_error = T2P_ERR_ERROR;
					return;
				}
			}
			if(t2p->tiff_samplesperpixel==4){
				t2p->pdf_colorspace=T2P_CS_CMYK;
			} else {
				TIFFError(
					TIFF2PDF_MODULE, 
					"No support for %s because it has %u samples per pixel",
					TIFFFileName(input), 
					t2p->tiff_samplesperpixel);
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			}
			break;
			photometric_palette_cmyk:
			if(t2p->tiff_samplesperpixel!=1){
				TIFFError(
					TIFF2PDF_MODULE, 
					"No support for palettized CMYK image %s with not one sample per pixel", 
					TIFFFileName(input));
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			}
			t2p->pdf_colorspace=T2P_CS_CMYK | T2P_CS_PALETTE;
			t2p->pdf_palettesize=0x0001<<t2p->tiff_bitspersample;
			if(!TIFFGetField(input, TIFFTAG_COLORMAP, &r, &g, &b, &a)){
				TIFFError(
					TIFF2PDF_MODULE, 
					"Palettized image %s has no color map", 
					TIFFFileName(input));
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			} 
			if(t2p->pdf_palette != NULL){
				_TIFFfree(t2p->pdf_palette);
				t2p->pdf_palette=NULL;
			}
			t2p->pdf_palette = (unsigned char*) 
				_TIFFmalloc(TIFFSafeMultiply(tmsize_t,t2p->pdf_palettesize,4));
			if(t2p->pdf_palette==NULL){
				TIFFError(
					TIFF2PDF_MODULE, 
					"Can't allocate %u bytes of memory for t2p_read_tiff_image, %s", 
					t2p->pdf_palettesize, 
					TIFFFileName(input));
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
			}
			for(i=0;i<t2p->pdf_palettesize;i++){
				t2p->pdf_palette[(i*4)]  = (unsigned char) (r[i]>>8);
				t2p->pdf_palette[(i*4)+1]= (unsigned char) (g[i]>>8);
				t2p->pdf_palette[(i*4)+2]= (unsigned char) (b[i]>>8);
				t2p->pdf_palette[(i*4)+3]= (unsigned char) (a[i]>>8);
			}
			t2p->pdf_palettesize *= 4;
			break;
		case PHOTOMETRIC_YCBCR:
			t2p->pdf_colorspace=T2P_CS_RGB;
			if(t2p->tiff_samplesperpixel==1){
				t2p->pdf_colorspace=T2P_CS_GRAY;
				t2p->tiff_photometric=PHOTOMETRIC_MINISBLACK;
				break;
			}
			t2p->pdf_sample=T2P_SAMPLE_YCBCR_TO_RGB;
#ifdef JPEG_SUPPORT
			if(t2p->pdf_defaultcompression==T2P_COMPRESS_JPEG){
				t2p->pdf_sample=T2P_SAMPLE_NOTHING;
			}
#endif
			break;
		case PHOTOMETRIC_CIELAB:
            if( t2p->tiff_samplesperpixel != 3){
                TIFFError(
                    TIFF2PDF_MODULE, 
                    "Unsupported samplesperpixel = %d for CIELAB", 
                    t2p->tiff_samplesperpixel);
                t2p->t2p_error = T2P_ERR_ERROR;
                return;
            }
            if( t2p->tiff_bitspersample != 8){
                TIFFError(
                    TIFF2PDF_MODULE, 
                    "Invalid bitspersample = %d for CIELAB", 
                    t2p->tiff_bitspersample);
                t2p->t2p_error = T2P_ERR_ERROR;
                return;
            }
			t2p->pdf_labrange[0]= -127;
			t2p->pdf_labrange[1]= 127;
			t2p->pdf_labrange[2]= -127;
			t2p->pdf_labrange[3]= 127;
			t2p->pdf_sample=T2P_SAMPLE_LAB_SIGNED_TO_UNSIGNED;
			t2p->pdf_colorspace=T2P_CS_LAB;
			break;
		case PHOTOMETRIC_ICCLAB:
			t2p->pdf_labrange[0]= 0;
			t2p->pdf_labrange[1]= 255;
			t2p->pdf_labrange[2]= 0;
			t2p->pdf_labrange[3]= 255;
			t2p->pdf_colorspace=T2P_CS_LAB;
			break;
		case PHOTOMETRIC_ITULAB:
            if( t2p->tiff_samplesperpixel != 3){
                TIFFError(
                    TIFF2PDF_MODULE, 
                    "Unsupported samplesperpixel = %d for ITULAB", 
                    t2p->tiff_samplesperpixel);
                t2p->t2p_error = T2P_ERR_ERROR;
                return;
            }
            if( t2p->tiff_bitspersample != 8){
                TIFFError(
                    TIFF2PDF_MODULE, 
                    "Invalid bitspersample = %d for ITULAB", 
                    t2p->tiff_bitspersample);
                t2p->t2p_error = T2P_ERR_ERROR;
                return;
            }
			t2p->pdf_labrange[0]=-85;
			t2p->pdf_labrange[1]=85;
			t2p->pdf_labrange[2]=-75;
			t2p->pdf_labrange[3]=124;
			t2p->pdf_sample=T2P_SAMPLE_LAB_SIGNED_TO_UNSIGNED;
			t2p->pdf_colorspace=T2P_CS_LAB;
			break;
		case PHOTOMETRIC_LOGL:
		case PHOTOMETRIC_LOGLUV:
			TIFFError(
				TIFF2PDF_MODULE, 
				"No support for %s with photometric interpretation LogL/LogLuv", 
				TIFFFileName(input));
			t2p->t2p_error = T2P_ERR_ERROR;
			return;
		default:
			TIFFError(
				TIFF2PDF_MODULE, 
				"No support for %s with photometric interpretation %u", 
				TIFFFileName(input),
				t2p->tiff_photometric);
			t2p->t2p_error = T2P_ERR_ERROR;
			return;
	}

	if(TIFFGetField(input, TIFFTAG_PLANARCONFIG, &(t2p->tiff_planar))){
		switch(t2p->tiff_planar){
			case 0:
				TIFFWarning(
					TIFF2PDF_MODULE, 
					"Image %s has planar configuration 0, assuming 1", 
					TIFFFileName(input));
				t2p->tiff_planar=PLANARCONFIG_CONTIG;
			case PLANARCONFIG_CONTIG:
				break;
			case PLANARCONFIG_SEPARATE:
				t2p->pdf_sample=T2P_SAMPLE_PLANAR_SEPARATE_TO_CONTIG;
				if(t2p->tiff_bitspersample!=8){
					TIFFError(
						TIFF2PDF_MODULE, 
						"No support for %s with separated planar configuration and %u bits per sample", 
						TIFFFileName(input),
						t2p->tiff_bitspersample);
					t2p->t2p_error = T2P_ERR_ERROR;
					return;
				}
				break;
			default:
				TIFFError(
					TIFF2PDF_MODULE, 
					"No support for %s with planar configuration %u", 
					TIFFFileName(input),
					t2p->tiff_planar);
				t2p->t2p_error = T2P_ERR_ERROR;
				return;
		}
	}

        TIFFGetFieldDefaulted(input, TIFFTAG_ORIENTATION,
                              &(t2p->tiff_orientation));
        if(t2p->tiff_orientation>8){
                TIFFWarning(TIFF2PDF_MODULE,
                            "Image %s has orientation %u, assuming 0",
                            TIFFFileName(input), t2p->tiff_orientation);
                t2p->tiff_orientation=0;
        }

        if(TIFFGetField(input, TIFFTAG_XRESOLUTION, &(t2p->tiff_xres) ) == 0){
                t2p->tiff_xres=0.0;
        }
        if(TIFFGetField(input, TIFFTAG_YRESOLUTION, &(t2p->tiff_yres) ) == 0){
                t2p->tiff_yres=0.0;
        }
	TIFFGetFieldDefaulted(input, TIFFTAG_RESOLUTIONUNIT,
			      &(t2p->tiff_resunit));
	if(t2p->tiff_resunit == RESUNIT_CENTIMETER) {
		t2p->tiff_xres *= 2.54F;
		t2p->tiff_yres *= 2.54F;
	} else if (t2p->tiff_resunit != RESUNIT_INCH
		   && t2p->pdf_centimeters != 0) {
		t2p->tiff_xres *= 2.54F;
		t2p->tiff_yres *= 2.54F;
	}

	t2p_compose_pdf_page(t2p);
        if( t2p->t2p_error == T2P_ERR_ERROR )
	    return;

	t2p->pdf_transcode = T2P_TRANSCODE_ENCODE;
	if(t2p->pdf_nopassthrough==0){
#ifdef CCITT_SUPPORT
		if(t2p->tiff_compression==COMPRESSION_CCITTFAX4  
			){
			if(TIFFIsTiled(input) || (TIFFNumberOfStrips(input)==1) ){
				t2p->pdf_transcode = T2P_TRANSCODE_RAW;
				t2p->pdf_compression=T2P_COMPRESS_G4;
			}
		}
#endif
#ifdef ZIP_SUPPORT
		if(t2p->tiff_compression== COMPRESSION_ADOBE_DEFLATE 
			|| t2p->tiff_compression==COMPRESSION_DEFLATE){
			if(TIFFIsTiled(input) || (TIFFNumberOfStrips(input)==1) ){
				t2p->pdf_transcode = T2P_TRANSCODE_RAW;
				t2p->pdf_compression=T2P_COMPRESS_ZIP;
			}
		}
#endif
#ifdef OJPEG_SUPPORT
		if(t2p->tiff_compression==COMPRESSION_OJPEG){
			t2p->pdf_transcode = T2P_TRANSCODE_RAW;
			t2p->pdf_compression=T2P_COMPRESS_JPEG;
			t2p_process_ojpeg_tables(t2p, input);
		}
#endif
#ifdef JPEG_SUPPORT
		if(t2p->tiff_compression==COMPRESSION_JPEG){
			t2p->pdf_transcode = T2P_TRANSCODE_RAW;
			t2p->pdf_compression=T2P_COMPRESS_JPEG;
		}
#endif
		(void)0;
	}

	if(t2p->pdf_transcode!=T2P_TRANSCODE_RAW){
		t2p->pdf_compression = t2p->pdf_defaultcompression;
	}

#ifdef JPEG_SUPPORT
	if(t2p->pdf_defaultcompression==T2P_COMPRESS_JPEG){
		if(t2p->pdf_colorspace & T2P_CS_PALETTE){
			t2p->pdf_sample|=T2P_SAMPLE_REALIZE_PALETTE;
			t2p->pdf_colorspace ^= T2P_CS_PALETTE;
			t2p->tiff_pages[t2p->pdf_page].page_extra--;
		}
	}
	if(t2p->tiff_compression==COMPRESSION_JPEG){
		if(t2p->tiff_planar==PLANARCONFIG_SEPARATE){
			TIFFError(
				TIFF2PDF_MODULE, 
				"No support for %s with JPEG compression and separated planar configuration", 
				TIFFFileName(input));
				t2p->t2p_error=T2P_ERR_ERROR;
			return;
		}
	}
#endif
#ifdef OJPEG_SUPPORT
	if(t2p->tiff_compression==COMPRESSION_OJPEG){
		if(t2p->tiff_planar==PLANARCONFIG_SEPARATE){
			TIFFError(
				TIFF2PDF_MODULE, 
				"No support for %s with OJPEG compression and separated planar configuration", 
				TIFFFileName(input));
				t2p->t2p_error=T2P_ERR_ERROR;
			return;
		}
	}
#endif

	if(t2p->pdf_sample & T2P_SAMPLE_REALIZE_PALETTE){
		if(t2p->pdf_colorspace & T2P_CS_CMYK){
			t2p->tiff_samplesperpixel=4;
			t2p->tiff_photometric=PHOTOMETRIC_SEPARATED;
		} else {
			t2p->tiff_samplesperpixel=3;
			t2p->tiff_photometric=PHOTOMETRIC_RGB;
		}
	}

	if (TIFFGetField(input, TIFFTAG_TRANSFERFUNCTION,
			 &(t2p->tiff_transferfunction[0]),
			 &(t2p->tiff_transferfunction[1]),
			 &(t2p->tiff_transferfunction[2]))) {
		if((t2p->tiff_transferfunction[1] != (float*) NULL) &&
                   (t2p->tiff_transferfunction[2] != (float*) NULL) &&
                   (t2p->tiff_transferfunction[1] !=
                    t2p->tiff_transferfunction[0])) {
			t2p->tiff_transferfunctioncount=3;
		} else {
			t2p->tiff_transferfunctioncount=1;
		}
	} else {
		t2p->tiff_transferfunctioncount=0;
	}
	if(TIFFGetField(input, TIFFTAG_WHITEPOINT, &xfloatp)!=0){
		t2p->tiff_whitechromaticities[0]=xfloatp[0];
		t2p->tiff_whitechromaticities[1]=xfloatp[1];
		if(t2p->pdf_colorspace & T2P_CS_GRAY){
			t2p->pdf_colorspace |= T2P_CS_CALGRAY;
		}
		if(t2p->pdf_colorspace & T2P_CS_RGB){
			t2p->pdf_colorspace |= T2P_CS_CALRGB;
		}
	}
	if(TIFFGetField(input, TIFFTAG_PRIMARYCHROMATICITIES, &xfloatp)!=0){
		t2p->tiff_primarychromaticities[0]=xfloatp[0];
		t2p->tiff_primarychromaticities[1]=xfloatp[1];
		t2p->tiff_primarychromaticities[2]=xfloatp[2];
		t2p->tiff_primarychromaticities[3]=xfloatp[3];
		t2p->tiff_primarychromaticities[4]=xfloatp[4];
		t2p->tiff_primarychromaticities[5]=xfloatp[5];
		if(t2p->pdf_colorspace & T2P_CS_RGB){
			t2p->pdf_colorspace |= T2P_CS_CALRGB;
		}
	}
	if(t2p->pdf_colorspace & T2P_CS_LAB){
		if(TIFFGetField(input, TIFFTAG_WHITEPOINT, &xfloatp) != 0){
			t2p->tiff_whitechromaticities[0]=xfloatp[0];
			t2p->tiff_whitechromaticities[1]=xfloatp[1];
		} else {
			t2p->tiff_whitechromaticities[0]=0.3457F; /* 0.3127F; */
			t2p->tiff_whitechromaticities[1]=0.3585F; /* 0.3290F; */
		}
	}
	if(TIFFGetField(input, 
		TIFFTAG_ICCPROFILE, 
		&(t2p->tiff_iccprofilelength), 
		&(t2p->tiff_iccprofile))!=0){
		t2p->pdf_colorspace |= T2P_CS_ICCBASED;
	} else {
		t2p->tiff_iccprofilelength=0;
		t2p->tiff_iccprofile=NULL;
	}
	
#ifdef CCITT_SUPPORT
	if( t2p->tiff_bitspersample==1 &&
		t2p->tiff_samplesperpixel==1){
		t2p->pdf_compression = T2P_COMPRESS_G4;
	}
#endif


	return;
}