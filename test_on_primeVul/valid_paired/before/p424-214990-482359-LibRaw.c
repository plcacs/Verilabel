void LibRaw::identify_process_dng_fields()
{
	if (!dng_version) return;
	int c;
	{
		/* copy DNG data from per-IFD field to color.dng */
		int iifd = find_ifd_by_offset(data_offset);
		int pifd = find_ifd_by_offset(thumb_offset);

#define CFAROUND(value, filters)                                               \
  filters ? (filters >= 1000 ? ((value + 1) / 2) * 2 : ((value + 5) / 6) * 6)  \
          : value

#define IFDCOLORINDEX(ifd, subset, bit)                                        \
  (tiff_ifd[ifd].dng_color[subset].parsedfields & bit)                         \
      ? ifd                                                                    \
      : ((tiff_ifd[0].dng_color[subset].parsedfields & bit) ? 0 : -1)

#define IFDLEVELINDEX(ifd, bit)                                                \
  (tiff_ifd[ifd].dng_levels.parsedfields & bit)                                \
      ? ifd                                                                    \
      : ((tiff_ifd[0].dng_levels.parsedfields & bit) ? 0 : -1)

#define COPYARR(to, from) memmove(&to, &from, sizeof(from))

		if (iifd < (int)tiff_nifds && iifd >= 0)
		{
			int sidx;
			// Per field, not per structure
			if (!(imgdata.params.raw_processing_options &
				LIBRAW_PROCESSING_DONT_CHECK_DNG_ILLUMINANT))
			{
				int illidx[2], cmidx[2], calidx[2], abidx;
				for (int i = 0; i < 2; i++)
				{
					illidx[i] = IFDCOLORINDEX(iifd, i, LIBRAW_DNGFM_ILLUMINANT);
					cmidx[i] = IFDCOLORINDEX(iifd, i, LIBRAW_DNGFM_COLORMATRIX);
					calidx[i] = IFDCOLORINDEX(iifd, i, LIBRAW_DNGFM_CALIBRATION);
				}
				abidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_ANALOGBALANCE);
				// Data found, all in same ifd, illuminants are inited
				if (illidx[0] >= 0 && illidx[0] < (int)tiff_nifds &&
					illidx[0] == illidx[1] && illidx[0] == cmidx[0] &&
					illidx[0] == cmidx[1] &&
					tiff_ifd[illidx[0]].dng_color[0].illuminant > 0 &&
					tiff_ifd[illidx[0]].dng_color[1].illuminant > 0)
				{
					sidx = illidx[0]; // => selected IFD
					double cc[4][4], cm[4][3], cam_xyz[4][3];
					// CM -> Color Matrix
					// CC -> Camera calibration
					for (int j = 0; j < 4; j++)
						for (int i = 0; i < 4; i++)
							cc[j][i] = i == j;
					int colidx = -1;

					// IS D65 here?
					for (int i = 0; i < 2; i++)
					{
						if (tiff_ifd[sidx].dng_color[i].illuminant == LIBRAW_WBI_D65)
						{
							colidx = i;
							break;
						}
					}

					// Other daylight-type ill
					if (colidx < 0)
						for (int i = 0; i < 2; i++)
						{
							int ill = tiff_ifd[sidx].dng_color[i].illuminant;
							if (ill == LIBRAW_WBI_Daylight || ill == LIBRAW_WBI_D55 ||
								ill == LIBRAW_WBI_D75 || ill == LIBRAW_WBI_D50 ||
								ill == LIBRAW_WBI_Flash)
							{
								colidx = i;
								break;
							}
						}
					if (colidx >= 0) // Selected
					{
						// Init camera matrix from DNG
						FORCC for (int j = 0; j < 3; j++) cm[c][j] =
							tiff_ifd[sidx].dng_color[colidx].colormatrix[c][j];

						if (calidx[colidx] == sidx)
						{
							for (int i = 0; i < colors; i++)
								FORCC
								cc[i][c] = tiff_ifd[sidx].dng_color[colidx].calibration[i][c];
						}

						if (abidx == sidx)
							for (int i = 0; i < colors; i++)
								FORCC cc[i][c] *= tiff_ifd[sidx].dng_levels.analogbalance[i];
						int j;
						FORCC for (int i = 0; i < 3; i++) for (cam_xyz[c][i] = j = 0;
							j < colors; j++)
							cam_xyz[c][i] +=
							cc[c][j] * cm[j][i]; // add AsShotXY later * xyz[i];
						cam_xyz_coeff(cmatrix, cam_xyz);
					}
				}
			}

			bool noFujiDNGCrop = makeIs(LIBRAW_CAMERAMAKER_Fujifilm)
				&& (!strcmp(normalized_model, "S3Pro")
					|| !strcmp(normalized_model, "S5Pro")
					|| !strcmp(normalized_model, "S2Pro"));

			if (!noFujiDNGCrop &&
				(imgdata.params.raw_processing_options &LIBRAW_PROCESSING_USE_DNG_DEFAULT_CROP))
			{
				sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_CROPORIGIN);
				int sidx2 = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_CROPSIZE);
				if (sidx >= 0 && sidx == sidx2 &&
					tiff_ifd[sidx].dng_levels.default_crop[2] > 0 &&
					tiff_ifd[sidx].dng_levels.default_crop[3] > 0)
				{
					int lm = tiff_ifd[sidx].dng_levels.default_crop[0];
					int lmm = CFAROUND(lm, filters);
					int tm = tiff_ifd[sidx].dng_levels.default_crop[1];
					int tmm = CFAROUND(tm, filters);
					int ww = tiff_ifd[sidx].dng_levels.default_crop[2];
					int hh = tiff_ifd[sidx].dng_levels.default_crop[3];
					if (lmm > lm)
						ww -= (lmm - lm);
					if (tmm > tm)
						hh -= (tmm - tm);
					if (left_margin + lm + ww <= raw_width &&
						top_margin + tm + hh <= raw_height)
					{
						left_margin += lmm;
						top_margin += tmm;
						width = ww;
						height = hh;
					}
				}
			}
			if (!(imgdata.color.dng_color[0].parsedfields &
				LIBRAW_DNGFM_FORWARDMATRIX)) // Not set already (Leica makernotes)
			{
				sidx = IFDCOLORINDEX(iifd, 0, LIBRAW_DNGFM_FORWARDMATRIX);
				if (sidx >= 0)
					COPYARR(imgdata.color.dng_color[0].forwardmatrix,
						tiff_ifd[sidx].dng_color[0].forwardmatrix);
			}
			if (!(imgdata.color.dng_color[1].parsedfields &
				LIBRAW_DNGFM_FORWARDMATRIX)) // Not set already (Leica makernotes)
			{
				sidx = IFDCOLORINDEX(iifd, 1, LIBRAW_DNGFM_FORWARDMATRIX);
				if (sidx >= 0)
					COPYARR(imgdata.color.dng_color[1].forwardmatrix,
						tiff_ifd[sidx].dng_color[1].forwardmatrix);
			}
			for (int ss = 0; ss < 2; ss++)
			{
				sidx = IFDCOLORINDEX(iifd, ss, LIBRAW_DNGFM_COLORMATRIX);
				if (sidx >= 0)
					COPYARR(imgdata.color.dng_color[ss].colormatrix,
						tiff_ifd[sidx].dng_color[ss].colormatrix);

				sidx = IFDCOLORINDEX(iifd, ss, LIBRAW_DNGFM_CALIBRATION);
				if (sidx >= 0)
					COPYARR(imgdata.color.dng_color[ss].calibration,
						tiff_ifd[sidx].dng_color[ss].calibration);

				sidx = IFDCOLORINDEX(iifd, ss, LIBRAW_DNGFM_ILLUMINANT);
				if (sidx >= 0)
					imgdata.color.dng_color[ss].illuminant =
					tiff_ifd[sidx].dng_color[ss].illuminant;
			}
			// Levels
			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_ANALOGBALANCE);
			if (sidx >= 0)
				COPYARR(imgdata.color.dng_levels.analogbalance,
					tiff_ifd[sidx].dng_levels.analogbalance);

			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_BASELINEEXPOSURE);
			if (sidx >= 0)
				imgdata.color.dng_levels.baseline_exposure =
				tiff_ifd[sidx].dng_levels.baseline_exposure;

			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_WHITE);
			if (sidx >= 0 && tiff_ifd[sidx].dng_levels.dng_whitelevel[0])
				COPYARR(imgdata.color.dng_levels.dng_whitelevel,
					tiff_ifd[sidx].dng_levels.dng_whitelevel);
			else if (tiff_ifd[iifd].sample_format <= 2 && tiff_ifd[iifd].bps > 0 && tiff_ifd[iifd].bps < 32)
				FORC4
				imgdata.color.dng_levels.dng_whitelevel[c] = (1 << tiff_ifd[iifd].bps) - 1;



			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_ASSHOTNEUTRAL);
			if (sidx >= 0)
			{
				COPYARR(imgdata.color.dng_levels.asshotneutral,
					tiff_ifd[sidx].dng_levels.asshotneutral);
				if (imgdata.color.dng_levels.asshotneutral[0])
				{
					cam_mul[3] = 0;
					FORCC
						if (fabs(imgdata.color.dng_levels.asshotneutral[c]) > 0.0001)
							cam_mul[c] = 1 / imgdata.color.dng_levels.asshotneutral[c];
				}
			}
			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_BLACK);
			if (sidx >= 0)
			{
				imgdata.color.dng_levels.dng_fblack =
					tiff_ifd[sidx].dng_levels.dng_fblack;
				imgdata.color.dng_levels.dng_black =
					tiff_ifd[sidx].dng_levels.dng_black;
				COPYARR(imgdata.color.dng_levels.dng_cblack,
					tiff_ifd[sidx].dng_levels.dng_cblack);
				COPYARR(imgdata.color.dng_levels.dng_fcblack,
					tiff_ifd[sidx].dng_levels.dng_fcblack);
			}


			if (pifd >= 0)
			{
				sidx = IFDLEVELINDEX(pifd, LIBRAW_DNGFM_PREVIEWCS);
				if (sidx >= 0)
					imgdata.color.dng_levels.preview_colorspace =
					tiff_ifd[sidx].dng_levels.preview_colorspace;
			}
			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_OPCODE2);
			if (sidx >= 0)
				meta_offset = tiff_ifd[sidx].opcode2_offset;

			sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_LINTABLE);
			INT64 linoff = -1;
			int linlen = 0;
			if (sidx >= 0)
			{
				linoff = tiff_ifd[sidx].lineartable_offset;
				linlen = tiff_ifd[sidx].lineartable_len;
			}

			if (linoff >= 0 && linlen > 0)
			{
				INT64 pos = ftell(ifp);
				fseek(ifp, linoff, SEEK_SET);
				linear_table(linlen);
				fseek(ifp, pos, SEEK_SET);
			}
			// Need to add curve too
		}
		/* Copy DNG black level to LibRaw's */
		if (load_raw == &LibRaw::lossy_dng_load_raw)
		{
			maximum = 0xffff;
			FORC4 imgdata.color.linear_max[c] = imgdata.color.dng_levels.dng_whitelevel[c] = 0xffff;
		}
		else
		{
			maximum = imgdata.color.dng_levels.dng_whitelevel[0];
		}
		black = imgdata.color.dng_levels.dng_black;

		if (tiff_samples == 2 && imgdata.color.dng_levels.dng_cblack[4] * imgdata.color.dng_levels.dng_cblack[5] * tiff_samples
			== imgdata.color.dng_levels.dng_cblack[LIBRAW_CBLACK_SIZE - 1])
		{
			unsigned ff = filters;
			if (filters > 999 && colors == 3)
				filters |= ((filters >> 2 & 0x22222222) | (filters << 2 & 0x88888888)) &
				filters << 1;

			/* Special case, Fuji SuperCCD dng */
			int csum[4] = { 0,0,0,0 }, ccount[4] = { 0,0,0,0 };
			int i = 6 + shot_select;
			for (unsigned row = 0; row < imgdata.color.dng_levels.dng_cblack[4]; row++)
				for (unsigned col = 0; col < imgdata.color.dng_levels.dng_cblack[5]; col++)
				{
					csum[FC(row, col)] += imgdata.color.dng_levels.dng_cblack[i];
					ccount[FC(row, col)]++;
					i += tiff_samples;
				}
			for (int c = 0; c < 4; c++)
				if (ccount[c])
					imgdata.color.dng_levels.dng_cblack[c] += csum[c] / ccount[c];
			imgdata.color.dng_levels.dng_cblack[4] = imgdata.color.dng_levels.dng_cblack[5] = 0;
			filters = ff;
		}
		else if (tiff_samples > 2 && tiff_samples <= 4 && imgdata.color.dng_levels.dng_cblack[4] * imgdata.color.dng_levels.dng_cblack[5] * tiff_samples
			== imgdata.color.dng_levels.dng_cblack[LIBRAW_CBLACK_SIZE - 1])
		{
			/* Special case, per_channel blacks in RepeatDim, average for per-channel */
			int csum[4] = { 0,0,0,0 }, ccount[4] = { 0,0,0,0 };
			int i = 6;
			for (unsigned row = 0; row < imgdata.color.dng_levels.dng_cblack[4]; row++)
				for (unsigned col = 0; col < imgdata.color.dng_levels.dng_cblack[5]; col++)
					for (unsigned c = 0; c < tiff_samples; c++)
					{
						csum[c] += imgdata.color.dng_levels.dng_cblack[i];
						ccount[c]++;
						i++;
					}
			for (int c = 0; c < 4; c++)
				if (ccount[c])
					imgdata.color.dng_levels.dng_cblack[c] += csum[c] / ccount[c];
			imgdata.color.dng_levels.dng_cblack[4] = imgdata.color.dng_levels.dng_cblack[5] = 0;
		}

		memmove(cblack, imgdata.color.dng_levels.dng_cblack, sizeof(cblack));

		if (iifd < (int)tiff_nifds && iifd >= 0)
		{
			int sidx = IFDLEVELINDEX(iifd, LIBRAW_DNGFM_LINEARRESPONSELIMIT);
			if (sidx >= 0)
			{
				imgdata.color.dng_levels.LinearResponseLimit =
					tiff_ifd[sidx].dng_levels.LinearResponseLimit;
				if (imgdata.color.dng_levels.LinearResponseLimit > 0.1 &&
					imgdata.color.dng_levels.LinearResponseLimit <= 1.0)
				{
					// And approx promote it to linear_max:
					int bl4 = 0, bl64 = 0;
					for (int chan = 0; chan < colors && chan < 4; chan++)
						bl4 += cblack[chan];
					bl4 /= LIM(colors, 1, 4);

					if (cblack[4] * cblack[5] > 0)
					{
						unsigned cnt = 0;
						for (unsigned c = 0; c < 4096 && c < cblack[4] * cblack[5]; c++)
						{
							bl64 += cblack[c + 6];
							cnt++;
						}
						bl64 /= LIM(cnt, 1, 4096);
					}
					int rblack = black + bl4 + bl64;
					for (int chan = 0; chan < colors && chan < 4; chan++)
						imgdata.color.linear_max[chan] =
						(maximum - rblack) *
						imgdata.color.dng_levels.LinearResponseLimit +
						rblack;
				}
			}
		}
	}
}