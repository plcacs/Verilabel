void CLASS parse_minolta(int base)
{
  int save, tag, len, offset, high = 0, wide = 0, i, c;
  short sorder = order;

  fseek(ifp, base, SEEK_SET);
  if (fgetc(ifp) || fgetc(ifp) - 'M' || fgetc(ifp) - 'R')
    return;
  order = fgetc(ifp) * 0x101;
  offset = base + get4() + 8;
  while ((save = ftell(ifp)) < offset)
  {
    for (tag = i = 0; i < 4; i++)
      tag = tag << 8 | fgetc(ifp);
    len = get4();
    switch (tag)
    {
    case 0x505244: /* PRD */
      fseek(ifp, 8, SEEK_CUR);
      high = get2();
      wide = get2();
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.makernotes.sony.prd_ImageHeight = get2();
      imgdata.makernotes.sony.prd_ImageWidth = get2();
      fseek(ifp, 1L, SEEK_CUR);
      imgdata.makernotes.sony.prd_RawBitDepth = (ushort)fgetc(ifp);
      imgdata.makernotes.sony.prd_StorageMethod = (ushort)fgetc(ifp);
      fseek(ifp, 4L, SEEK_CUR);
      imgdata.makernotes.sony.prd_BayerPattern = (ushort)fgetc(ifp);
#endif
      break;
#ifdef LIBRAW_LIBRARY_BUILD
    case 0x524946: /* RIF */
      if (!strncasecmp(model, "DSLR-A100", 9))
      {
        fseek(ifp, 8, SEEK_CUR);
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Daylight][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Daylight][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Cloudy][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Cloudy][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_W][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_W][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][2] = get2();
        get4();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_D][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_D][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_N][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_N][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][2] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Daylight][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Daylight][3] =
            imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Tungsten][3] =
                imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] =
                    imgdata.color.WB_Coeffs[LIBRAW_WBI_Cloudy][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Cloudy][3] =
                        imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Shade][3] =
                            imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_D][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_D][3] =
                                imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_N][1] =
                                    imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_N][3] =
                                        imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_W][1] =
                                            imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_W][3] =
                                                imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][1] =
                                                    imgdata.color.WB_Coeffs[LIBRAW_WBI_FL_WW][3] = 0x100;
      }
      break;
#endif
    case 0x574247: /* WBG */
      get4();
      i = strcmp(model, "DiMAGE A200") ? 0 : 3;
      FORC4 cam_mul[c ^ (c >> 1) ^ i] = get2();
      break;
    case 0x545457: /* TTW */
      parse_tiff(ftell(ifp));
      data_offset = offset;
    }
    fseek(ifp, save + len + 8, SEEK_SET);
  }
  raw_height = high;
  raw_width = wide;
  order = sorder;
}