void CLASS parse_makernote(int base, int uptag)
{
  unsigned offset = 0, entries, tag, type, len, save, c;
  unsigned ver97 = 0, serial = 0, i, wbi = 0, wb[4] = {0, 0, 0, 0};
  uchar buf97[324], ci, cj, ck;
  short morder, sorder = order;
  char buf[10];
  unsigned SamsungKey[11];
  uchar NikonKey;

#ifdef LIBRAW_LIBRARY_BUILD
  unsigned custom_serial = 0;
  unsigned NikonLensDataVersion = 0;
  unsigned lenNikonLensData = 0;

  unsigned NikonFlashInfoVersion = 0;

  uchar *CanonCameraInfo;
  unsigned lenCanonCameraInfo = 0;
  unsigned typeCanonCameraInfo = 0;

  uchar *table_buf;
  uchar *table_buf_0x0116;
  ushort table_buf_0x0116_len = 0;
  uchar *table_buf_0x2010;
  ushort table_buf_0x2010_len = 0;
  uchar *table_buf_0x9050;
  ushort table_buf_0x9050_len = 0;
  uchar *table_buf_0x9400;
  ushort table_buf_0x9400_len = 0;
  uchar *table_buf_0x9402;
  ushort table_buf_0x9402_len = 0;
  uchar *table_buf_0x9403;
  ushort table_buf_0x9403_len = 0;
  uchar *table_buf_0x9406;
  ushort table_buf_0x9406_len = 0;
  uchar *table_buf_0x940c;
  ushort table_buf_0x940c_len = 0;
  uchar *table_buf_0x940e;
  ushort table_buf_0x940e_len = 0;

  INT64 fsize = ifp->size();
#endif
  /*
     The MakerNote might have its own TIFF header (possibly with
     its own byte-order!), or it might just be a table.
   */
  if (!strncmp(make, "Nokia", 5))
    return;
  fread(buf, 1, 10, ifp);

  /*
    printf("===>>buf: 0x");
    for (int i = 0; i < sizeof buf; i ++) {
          printf("%02x", buf[i]);
    }
    putchar('\n');
  */

  if (!strncmp(buf, "KDK", 3) || /* these aren't TIFF tables */
      !strncmp(buf, "VER", 3) || !strncmp(buf, "IIII", 4) || !strncmp(buf, "MMMM", 4))
    return;
  if (!strncmp(buf, "KC", 2) || /* Konica KD-400Z, KD-510Z */
      !strncmp(buf, "MLY", 3))
  { /* Minolta DiMAGE G series */
    order = 0x4d4d;
    while ((i = ftell(ifp)) < data_offset && i < 16384)
    {
      wb[0] = wb[2];
      wb[2] = wb[1];
      wb[1] = wb[3];
      wb[3] = get2();
      if (wb[1] == 256 && wb[3] == 256 && wb[0] > 256 && wb[0] < 640 && wb[2] > 256 && wb[2] < 640)
        FORC4 cam_mul[c] = wb[c];
    }
    goto quit;
  }
  if (!strcmp(buf, "Nikon"))
  {
    base = ftell(ifp);
    order = get2();
    if (get2() != 42)
      goto quit;
    offset = get4();
    fseek(ifp, offset - 8, SEEK_CUR);
  }
  else if (!strcmp(buf, "OLYMPUS") || !strcmp(buf, "PENTAX "))
  {
    base = ftell(ifp) - 10;
    fseek(ifp, -2, SEEK_CUR);
    order = get2();
    if (buf[0] == 'O')
      get2();
  }
  else if (!strncmp(buf, "SONY", 4) || !strcmp(buf, "Panasonic"))
  {
    goto nf;
  }
  else if (!strncmp(buf, "FUJIFILM", 8))
  {
    base = ftell(ifp) - 10;
  nf:
    order = 0x4949;
    fseek(ifp, 2, SEEK_CUR);
  }
  else if (!strcmp(buf, "OLYMP") || !strcmp(buf, "LEICA") || !strcmp(buf, "Ricoh") || !strcmp(buf, "EPSON"))
    fseek(ifp, -2, SEEK_CUR);
  else if (!strcmp(buf, "AOC") || !strcmp(buf, "QVC"))
    fseek(ifp, -4, SEEK_CUR);
  else
  {
    fseek(ifp, -10, SEEK_CUR);
    if (!strncmp(make, "SAMSUNG", 7))
      base = ftell(ifp);
  }

  // adjust pos & base for Leica M8/M9/M Mono tags and dir in tag 0x3400
  if (!strncasecmp(make, "LEICA", 5))
  {
    if (!strncmp(model, "M8", 2) || !strncasecmp(model, "Leica M8", 8) || !strncasecmp(model, "LEICA X", 7))
    {
      base = ftell(ifp) - 8;
    }
    else if (!strncasecmp(model, "LEICA M (Typ 240)", 17))
    {
      base = 0;
    }
    else if (!strncmp(model, "M9", 2) || !strncasecmp(model, "Leica M9", 8) || !strncasecmp(model, "M Monochrom", 11) ||
             !strncasecmp(model, "Leica M Monochrom", 11))
    {
      if (!uptag)
      {
        base = ftell(ifp) - 10;
        fseek(ifp, 8, SEEK_CUR);
      }
      else if (uptag == 0x3400)
      {
        fseek(ifp, 10, SEEK_CUR);
        base += 10;
      }
    }
    else if (!strncasecmp(model, "LEICA T", 7))
    {
      base = ftell(ifp) - 8;
#ifdef LIBRAW_LIBRARY_BUILD
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_T;
#endif
    }
#ifdef LIBRAW_LIBRARY_BUILD
    else if (!strncasecmp(model, "LEICA SL", 8))
    {
      imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_SL;
      imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_FF;
    }
#endif
  }

  entries = get2();
  if (entries > 1000)
    return;
  morder = order;

  while (entries--)
  {
    order = morder;
    tiff_get(base, &tag, &type, &len, &save);
    tag |= uptag << 16;

#ifdef LIBRAW_LIBRARY_BUILD
    INT64 _pos = ftell(ifp);
    if (len > 8 && _pos + len > 2 * fsize)
    {
      fseek(ifp, save, SEEK_SET); // Recover tiff-read position!!
      continue;
    }
    if (!strncasecmp(model, "KODAK P880", 10) || !strncasecmp(model, "KODAK P850", 10) ||
        !strncasecmp(model, "KODAK P712", 10))
    {
      if (tag == 0xf90b)
      {
        imgdata.makernotes.kodak.clipBlack = get2();
      }
      else if (tag == 0xf90c)
      {
        imgdata.makernotes.kodak.clipWhite = get2();
      }
    }
    if (!strncmp(make, "Canon", 5))
    {
      if (tag == 0x000d && len < 256000) // camera info
      {
        if (type != 4)
        {
          CanonCameraInfo = (uchar *)malloc(MAX(16, len));
          fread(CanonCameraInfo, len, 1, ifp);
        }
        else
        {
          CanonCameraInfo = (uchar *)malloc(MAX(16, len * 4));
          fread(CanonCameraInfo, len, 4, ifp);
        }
        lenCanonCameraInfo = len;
        typeCanonCameraInfo = type;
      }

      else if (tag == 0x10) // Canon ModelID
      {
        unique_id = get4();
        unique_id = setCanonBodyFeatures(unique_id);
        if (lenCanonCameraInfo)
        {
          processCanonCameraInfo(unique_id, CanonCameraInfo, lenCanonCameraInfo, typeCanonCameraInfo);
          free(CanonCameraInfo);
          CanonCameraInfo = 0;
          lenCanonCameraInfo = 0;
        }
      }

      else
        parseCanonMakernotes(tag, type, len);
    }

    else if (!strncmp(make, "FUJI", 4))
    {
      if (tag == 0x0010)
      {
        char FujiSerial[sizeof(imgdata.shootinginfo.InternalBodySerial)];
        char *words[4];
        char yy[2], mm[3], dd[3], ystr[16], ynum[16];
        int year, nwords, ynum_len;
        unsigned c;
        stmread(FujiSerial, len, ifp);
        nwords = getwords(FujiSerial, words, 4, sizeof(imgdata.shootinginfo.InternalBodySerial));
        for (int i = 0; i < nwords; i++)
        {
          mm[2] = dd[2] = 0;
          if (strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) < 18)
            if (i == 0)
              strncpy(imgdata.shootinginfo.InternalBodySerial, words[0],
                      sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
            else
            {
              char tbuf[sizeof(imgdata.shootinginfo.InternalBodySerial)];
              snprintf(tbuf, sizeof(tbuf), "%s %s", imgdata.shootinginfo.InternalBodySerial, words[i]);
              strncpy(imgdata.shootinginfo.InternalBodySerial, tbuf,
                      sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
            }
          else
          {
            strncpy(dd, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 14, 2);
            strncpy(mm, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 16, 2);
            strncpy(yy, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 18, 2);
            year = (yy[0] - '0') * 10 + (yy[1] - '0');
            if (year < 70)
              year += 2000;
            else
              year += 1900;

            ynum_len = (int)strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 18;
            strncpy(ynum, words[i], ynum_len);
            ynum[ynum_len] = 0;
            for (int j = 0; ynum[j] && ynum[j + 1] && sscanf(ynum + j, "%2x", &c); j += 2)
              ystr[j / 2] = c;
            ystr[ynum_len / 2 + 1] = 0;
            strcpy(model2, ystr);

            if (i == 0)
            {
              char tbuf[sizeof(imgdata.shootinginfo.InternalBodySerial)];

              if (nwords == 1)
                snprintf(tbuf, sizeof(tbuf), "%s %s %d:%s:%s",
                         words[0] + strnlen(words[0], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 12, ystr,
                         year, mm, dd);

              else
                snprintf(tbuf, sizeof(tbuf), "%s %d:%s:%s %s", ystr, year, mm, dd,
                         words[0] + strnlen(words[0], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 12);

              strncpy(imgdata.shootinginfo.InternalBodySerial, tbuf,
                      sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
            }
            else
            {
              char tbuf[sizeof(imgdata.shootinginfo.InternalBodySerial)];
              snprintf(tbuf, sizeof(tbuf), "%s %s %d:%s:%s %s", imgdata.shootinginfo.InternalBodySerial, ystr, year, mm,
                       dd, words[i] + strnlen(words[i], sizeof(imgdata.shootinginfo.InternalBodySerial) - 1) - 12);
              strncpy(imgdata.shootinginfo.InternalBodySerial, tbuf,
                      sizeof(imgdata.shootinginfo.InternalBodySerial) - 1);
            }
          }
        }
      }
      else
        parseFujiMakernotes(tag, type);
    }

    else if (!strncasecmp(model, "Hasselblad X1D", 14) || !strncasecmp(model, "Hasselblad H6D", 14) ||
             !strncasecmp(model, "Hasselblad A6D", 14))
    {
      if (tag == 0x0045)
      {
        imgdata.makernotes.hasselblad.BaseISO = get4();
      }
      else if (tag == 0x0046)
      {
        imgdata.makernotes.hasselblad.Gain = getreal(type);
      }
    }

    else if (!strncasecmp(make, "LEICA", 5))
    {
      if (((tag == 0x035e) || (tag == 0x035f)) && (type == 10) && (len == 9))
      {
        int ind = tag == 0x035e ? 0 : 1;
        for (int j = 0; j < 3; j++)
          FORCC imgdata.color.dng_color[ind].forwardmatrix[j][c] = getreal(type);
        imgdata.color.dng_color[ind].parsedfields |= LIBRAW_DNGFM_FORWARDMATRIX;
      }

      if (tag == 0x34003402)
        imgdata.other.CameraTemperature = getreal(type);

      if ((tag == 0x0320) && (type == 9) && (len == 1) && !strncasecmp(make, "Leica Camera AG", 15) &&
          !strncmp(buf, "LEICA", 5) && (buf[5] == 0) && (buf[6] == 0) && (buf[7] == 0))
        imgdata.other.CameraTemperature = getreal(type);

      if ((tag == 0x0303) && (type != 4))
      {
        stmread(imgdata.lens.makernotes.Lens, len, ifp);
      }

      if ((tag == 0x3405) || (tag == 0x0310) || (tag == 0x34003405))
      {
        imgdata.lens.makernotes.LensID = get4();
        imgdata.lens.makernotes.LensID =
            ((imgdata.lens.makernotes.LensID >> 2) << 8) | (imgdata.lens.makernotes.LensID & 0x3);
        if (imgdata.lens.makernotes.LensID != -1)
        {
          if ((model[0] == 'M') || !strncasecmp(model, "LEICA M", 7))
          {
            imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_M;
            if (imgdata.lens.makernotes.LensID)
              imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Leica_M;
          }
          else if ((model[0] == 'S') || !strncasecmp(model, "LEICA S", 7))
          {
            imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_S;
            if (imgdata.lens.makernotes.Lens[0])
              imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Leica_S;
          }
        }
      }

      else if (((tag == 0x0313) || (tag == 0x34003406)) && (fabs(imgdata.lens.makernotes.CurAp) < 0.17f) &&
               ((type == 10) || (type == 5)))
      {
        imgdata.lens.makernotes.CurAp = getreal(type);
        if (imgdata.lens.makernotes.CurAp > 126.3)
          imgdata.lens.makernotes.CurAp = 0.0f;
      }

      else if (tag == 0x3400)
      {
        parse_makernote(base, 0x3400);
      }
    }

    else if (!strncmp(make, "NIKON", 5))
    {
      if (tag == 0x000a)
      {
        imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
        imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
      }
      else if (tag == 0x0012)
      {
        char a, b, c;
        a = fgetc(ifp);
        b = fgetc(ifp);
        c = fgetc(ifp);
        if (c)
          imgdata.other.FlashEC = (float)(a * b) / (float)c;
      }
      else if (tag == 0x003b) // all 1s for regular exposures
      {
        imgdata.makernotes.nikon.ME_WB[0] = getreal(type);
        imgdata.makernotes.nikon.ME_WB[2] = getreal(type);
        imgdata.makernotes.nikon.ME_WB[1] = getreal(type);
        imgdata.makernotes.nikon.ME_WB[3] = getreal(type);
      }
      else if (tag == 0x0045)
      {
        imgdata.sizes.raw_crop.cleft = get2();
        imgdata.sizes.raw_crop.ctop = get2();
        imgdata.sizes.raw_crop.cwidth = get2();
        imgdata.sizes.raw_crop.cheight = get2();
      }
      else if (tag == 0x0082) // lens attachment
      {
        stmread(imgdata.lens.makernotes.Attachment, len, ifp);
      }
      else if (tag == 0x0083) // lens type
      {
        imgdata.lens.nikon.NikonLensType = fgetc(ifp);
      }
      else if (tag == 0x0084) // lens
      {
        imgdata.lens.makernotes.MinFocal = getreal(type);
        imgdata.lens.makernotes.MaxFocal = getreal(type);
        imgdata.lens.makernotes.MaxAp4MinFocal = getreal(type);
        imgdata.lens.makernotes.MaxAp4MaxFocal = getreal(type);
      }
      else if (tag == 0x008b) // lens f-stops
      {
        uchar a, b, c;
        a = fgetc(ifp);
        b = fgetc(ifp);
        c = fgetc(ifp);
        if (c)
        {
          imgdata.lens.nikon.NikonLensFStops = a * b * (12 / c);
          imgdata.lens.makernotes.LensFStops = (float)imgdata.lens.nikon.NikonLensFStops / 12.0f;
        }
      }
      else if (tag == 0x0093) // Nikon compression
      {
        imgdata.makernotes.nikon.NEFCompression = i = get2();
        if ((i == 7) || (i == 9))
        {
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
        }
      }
      else if (tag == 0x0098) // contains lens data
      {
        for (i = 0; i < 4; i++)
        {
          NikonLensDataVersion = NikonLensDataVersion * 10 + fgetc(ifp) - '0';
        }
        switch (NikonLensDataVersion)
        {
        case 100:
          lenNikonLensData = 9;
          break;
        case 101:
        case 201: // encrypted, starting from v.201
        case 202:
        case 203:
          lenNikonLensData = 15;
          break;
        case 204:
          lenNikonLensData = 16;
          break;
        case 400:
          lenNikonLensData = 459;
          break;
        case 401:
          lenNikonLensData = 590;
          break;
        case 402:
          lenNikonLensData = 509;
          break;
        case 403:
          lenNikonLensData = 879;
          break;
        }
        if (lenNikonLensData > 0)
        {
          table_buf = (uchar *)malloc(lenNikonLensData);
          fread(table_buf, lenNikonLensData, 1, ifp);
          if ((NikonLensDataVersion < 201) && lenNikonLensData)
          {
            processNikonLensData(table_buf, lenNikonLensData);
            free(table_buf);
            lenNikonLensData = 0;
          }
        }
      }
      else if (tag == 0x00a0)
      {
        stmread(imgdata.shootinginfo.BodySerial, len, ifp);
      }
      else if (tag == 0x00a8) // contains flash data
      {
        for (i = 0; i < 4; i++)
        {
          NikonFlashInfoVersion = NikonFlashInfoVersion * 10 + fgetc(ifp) - '0';
        }
      }
      else if (tag == 0x00b0)
      {
        get4(); // ME tag version, 4 symbols
        imgdata.makernotes.nikon.ExposureMode = get4();
        imgdata.makernotes.nikon.nMEshots = get4();
        imgdata.makernotes.nikon.MEgainOn = get4();
      }
      else if (tag == 0x00b9)
      {
        uchar uc;
        int8_t sc;
        fread(&uc, 1, 1, ifp);
        imgdata.makernotes.nikon.AFFineTune = uc;
        fread(&uc, 1, 1, ifp);
        imgdata.makernotes.nikon.AFFineTuneIndex = uc;
        fread(&sc, 1, 1, ifp);
        imgdata.makernotes.nikon.AFFineTuneAdj = sc;
      }
    }

    else if (!strncmp(make, "OLYMPUS", 7))
    {
      switch (tag)
      {
      case 0x0404:
      case 0x101a:
      case 0x20100101:
        if (!imgdata.shootinginfo.BodySerial[0])
          stmread(imgdata.shootinginfo.BodySerial, len, ifp);
        break;
      case 0x20100102:
        if (!imgdata.shootinginfo.InternalBodySerial[0])
          stmread(imgdata.shootinginfo.InternalBodySerial, len, ifp);
        break;
      case 0x0207:
      case 0x20100100:
      {
        uchar sOlyID[8];
        fread(sOlyID, MIN(len, 7), 1, ifp);
        sOlyID[7] = 0;
        OlyID = sOlyID[0];
        i = 1;
        while (i < 7 && sOlyID[i])
        {
          OlyID = OlyID << 8 | sOlyID[i];
          i++;
        }
        setOlympusBodyFeatures(OlyID);
      }
      break;
      case 0x1002:
        imgdata.lens.makernotes.CurAp = libraw_powf64l(2.0f, getreal(type) / 2);
        break;
      case 0x20400612:
      case 0x30000612:
        imgdata.sizes.raw_crop.cleft = get2();
        break;
      case 0x20400613:
      case 0x30000613:
        imgdata.sizes.raw_crop.ctop = get2();
        break;
      case 0x20400614:
      case 0x30000614:
        imgdata.sizes.raw_crop.cwidth = get2();
        break;
      case 0x20400615:
      case 0x30000615:
        imgdata.sizes.raw_crop.cheight = get2();
        break;
      case 0x20401112:
        imgdata.makernotes.olympus.OlympusCropID = get2();
        break;
      case 0x20401113:
        FORC4 imgdata.makernotes.olympus.OlympusFrame[c] = get2();
        break;
      case 0x20100201:
      {
        unsigned long long oly_lensid[3];
        oly_lensid[0] = fgetc(ifp);
        fgetc(ifp);
        oly_lensid[1] = fgetc(ifp);
        oly_lensid[2] = fgetc(ifp);
        imgdata.lens.makernotes.LensID = (oly_lensid[0] << 16) | (oly_lensid[1] << 8) | oly_lensid[2];
      }
        imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FT;
        imgdata.lens.makernotes.LensFormat = LIBRAW_FORMAT_FT;
        if (((imgdata.lens.makernotes.LensID < 0x20000) || (imgdata.lens.makernotes.LensID > 0x4ffff)) &&
            (imgdata.lens.makernotes.LensID & 0x10))
        {
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_mFT;
        }
        break;
      case 0x20100202:
        stmread(imgdata.lens.LensSerial, len, ifp);
        break;
      case 0x20100203:
        stmread(imgdata.lens.makernotes.Lens, len, ifp);
        break;
      case 0x20100205:
        imgdata.lens.makernotes.MaxAp4MinFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
        break;
      case 0x20100206:
        imgdata.lens.makernotes.MaxAp4MaxFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
        break;
      case 0x20100207:
        imgdata.lens.makernotes.MinFocal = (float)get2();
        break;
      case 0x20100208:
        imgdata.lens.makernotes.MaxFocal = (float)get2();
        if (imgdata.lens.makernotes.MaxFocal > 1000.0f)
          imgdata.lens.makernotes.MaxFocal = imgdata.lens.makernotes.MinFocal;
        break;
      case 0x2010020a:
        imgdata.lens.makernotes.MaxAp4CurFocal = libraw_powf64l(sqrt(2.0f), get2() / 256.0f);
        break;
      case 0x20100301:
        imgdata.lens.makernotes.TeleconverterID = fgetc(ifp) << 8;
        fgetc(ifp);
        imgdata.lens.makernotes.TeleconverterID = imgdata.lens.makernotes.TeleconverterID | fgetc(ifp);
        break;
      case 0x20100303:
        stmread(imgdata.lens.makernotes.Teleconverter, len, ifp);
        break;
      case 0x20100403:
        stmread(imgdata.lens.makernotes.Attachment, len, ifp);
        break;
      case 0x1007:
        imgdata.other.SensorTemperature = (float)get2();
        break;
      case 0x1008:
        imgdata.other.LensTemperature = (float)get2();
        break;
      case 0x20401306:
      {
        int temp = get2();
        if ((temp != 0) && (temp != 100))
        {
          if (temp < 61)
            imgdata.other.CameraTemperature = (float)temp;
          else
            imgdata.other.CameraTemperature = (float)(temp - 32) / 1.8f;
          if ((OlyID == 0x4434353933ULL) && // TG-5
              (imgdata.other.exifAmbientTemperature > -273.15f))
            imgdata.other.CameraTemperature += imgdata.other.exifAmbientTemperature;
        }
      }
      break;
      case 0x20501500:
        if (OlyID != 0x0ULL)
        {
          short temp = get2();
          if ((OlyID == 0x4434303430ULL) || // E-1
              (OlyID == 0x5330303336ULL) || // E-M5
              (len != 1))
            imgdata.other.SensorTemperature = (float)temp;
          else if ((temp != -32768) && (temp != 0))
          {
            if (temp > 199)
              imgdata.other.SensorTemperature = 86.474958f - 0.120228f * (float)temp;
            else
              imgdata.other.SensorTemperature = (float)temp;
          }
        }
        break;
      }
    }

    else if ((!strncmp(make, "PENTAX", 6) || !strncmp(make, "RICOH", 5)) && !strncmp(model, "GR", 2))
    {
      if (tag == 0x0005)
      {
        char buffer[17];
        int count = 0;
        fread(buffer, 16, 1, ifp);
        buffer[16] = 0;
        for (int i = 0; i < 16; i++)
        {
          //    	        sprintf(imgdata.shootinginfo.InternalBodySerial+2*i, "%02x", buffer[i]);
          if ((isspace(buffer[i])) || (buffer[i] == 0x2D) || (isalnum(buffer[i])))
            count++;
        }
        if (count == 16)
        {
          sprintf(imgdata.shootinginfo.BodySerial, "%8s", buffer + 8);
          buffer[8] = 0;
          sprintf(imgdata.shootinginfo.InternalBodySerial, "%8s", buffer);
        }
        else
        {
          sprintf(imgdata.shootinginfo.BodySerial, "%02x%02x%02x%02x", buffer[4], buffer[5], buffer[6], buffer[7]);
          sprintf(imgdata.shootinginfo.InternalBodySerial, "%02x%02x%02x%02x", buffer[8], buffer[9], buffer[10],
                  buffer[11]);
        }
      }
      else if ((tag == 0x1001) && (type == 3))
      {
        imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
        imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
        imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
        imgdata.lens.makernotes.LensID = -1;
        imgdata.lens.makernotes.FocalType = 1;
      }

      else if ((tag == 0x100b) && (type == 10))
      {
        imgdata.other.FlashEC = getreal(type);
      }

      else if ((tag == 0x1017) && (get2() == 2))
      {
        strcpy(imgdata.lens.makernotes.Attachment, "Wide-Angle Adapter");
      }
      else if (tag == 0x1500)
      {
        imgdata.lens.makernotes.CurFocal = getreal(type);
      }
    }

    else if (!strncmp(make, "RICOH", 5) && strncmp(model, "PENTAX", 6))
    {
      if ((tag == 0x0005) && !strncmp(model, "GXR", 3))
      {
        char buffer[9];
        buffer[8] = 0;
        fread(buffer, 8, 1, ifp);
        sprintf(imgdata.shootinginfo.InternalBodySerial, "%8s", buffer);
      }

      else if ((tag == 0x100b) && (type == 10))
      {
        imgdata.other.FlashEC = getreal(type);
      }

      else if ((tag == 0x1017) && (get2() == 2))
      {
        strcpy(imgdata.lens.makernotes.Attachment, "Wide-Angle Adapter");
      }

      else if (tag == 0x1500)
      {
        imgdata.lens.makernotes.CurFocal = getreal(type);
      }

      else if ((tag == 0x2001) && !strncmp(model, "GXR", 3))
      {
        short ntags, cur_tag;
        fseek(ifp, 20, SEEK_CUR);
        ntags = get2();
        cur_tag = get2();
        while (cur_tag != 0x002c)
        {
          fseek(ifp, 10, SEEK_CUR);
          cur_tag = get2();
        }
        fseek(ifp, 6, SEEK_CUR);
        fseek(ifp, get4() + 20, SEEK_SET);
        stread(imgdata.shootinginfo.BodySerial, 12, ifp);
        get2();
        imgdata.lens.makernotes.LensID = getc(ifp) - '0';
        switch (imgdata.lens.makernotes.LensID)
        {
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_RicohModule;
          break;
        case 8:
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Leica_M;
          imgdata.lens.makernotes.CameraFormat = LIBRAW_FORMAT_APSC;
          imgdata.lens.makernotes.LensID = -1;
          break;
        default:
          imgdata.lens.makernotes.LensID = -1;
        }
        fseek(ifp, 17, SEEK_CUR);
        stread(imgdata.lens.LensSerial, 12, ifp);
      }
    }

    else if ((!strncmp(make, "PENTAX", 6) || !strncmp(model, "PENTAX", 6) ||
              (!strncmp(make, "SAMSUNG", 7) && dng_version)) &&
             strncmp(model, "GR", 2))
    {
      if (tag == 0x0005)
      {
        unique_id = get4();
        setPentaxBodyFeatures(unique_id);
      }
      else if (tag == 0x000d)
      {
        imgdata.makernotes.pentax.FocusMode = get2();
      }
      else if (tag == 0x000e)
      {
        imgdata.makernotes.pentax.AFPointSelected = get2();
      }
      else if (tag == 0x000f)
      {
        imgdata.makernotes.pentax.AFPointsInFocus = getint(type);
      }
      else if (tag == 0x0010)
      {
        imgdata.makernotes.pentax.FocusPosition = get2();
      }
      else if (tag == 0x0013)
      {
        imgdata.lens.makernotes.CurAp = (float)get2() / 10.0f;
      }
      else if (tag == 0x0014)
      {
        PentaxISO(get2());
      }
      else if (tag == 0x001d)
      {
        imgdata.lens.makernotes.CurFocal = (float)get4() / 100.0f;
      }
      else if (tag == 0x0034)
      {
        uchar uc;
        FORC4
        {
          fread(&uc, 1, 1, ifp);
          imgdata.makernotes.pentax.DriveMode[c] = uc;
        }
      }
      else if (tag == 0x0038)
      {
        imgdata.sizes.raw_crop.cleft = get2();
        imgdata.sizes.raw_crop.ctop = get2();
      }
      else if (tag == 0x0039)
      {
        imgdata.sizes.raw_crop.cwidth = get2();
        imgdata.sizes.raw_crop.cheight = get2();
      }
      else if (tag == 0x003f)
      {
        imgdata.lens.makernotes.LensID = fgetc(ifp) << 8 | fgetc(ifp);
      }
      else if (tag == 0x0047)
      {
        imgdata.other.CameraTemperature = (float)fgetc(ifp);
      }
      else if (tag == 0x004d)
      {
        if (type == 9)
          imgdata.other.FlashEC = getreal(type) / 256.0f;
        else
          imgdata.other.FlashEC = (float)((signed short)fgetc(ifp)) / 6.0f;
      }
      else if (tag == 0x0072)
      {
        imgdata.makernotes.pentax.AFAdjustment = get2();
      }
      else if (tag == 0x007e)
      {
        imgdata.color.linear_max[0] = imgdata.color.linear_max[1] = imgdata.color.linear_max[2] =
            imgdata.color.linear_max[3] = (long)(-1) * get4();
      }
      else if (tag == 0x0207)
      {
        if (len < 65535) // Safety belt
          PentaxLensInfo(imgdata.lens.makernotes.CamID, len);
      }
      else if ((tag >= 0x020d) && (tag <= 0x0214))
      {
        FORC4 imgdata.color.WB_Coeffs[Pentax_wb_list1[tag - 0x020d]][c ^ (c >> 1)] = get2();
      }
      else if (tag == 0x0221)
      {
        int nWB = get2();
        if (nWB <= sizeof(imgdata.color.WBCT_Coeffs) / sizeof(imgdata.color.WBCT_Coeffs[0]))
          for (int i = 0; i < nWB; i++)
          {
            imgdata.color.WBCT_Coeffs[i][0] = (unsigned)0xcfc6 - get2();
            fseek(ifp, 2, SEEK_CUR);
            imgdata.color.WBCT_Coeffs[i][1] = get2();
            imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] = 0x2000;
            imgdata.color.WBCT_Coeffs[i][3] = get2();
          }
      }
      else if (tag == 0x0215)
      {
        fseek(ifp, 16, SEEK_CUR);
        sprintf(imgdata.shootinginfo.InternalBodySerial, "%d", get4());
      }
      else if (tag == 0x0229)
      {
        stmread(imgdata.shootinginfo.BodySerial, len, ifp);
      }
      else if (tag == 0x022d)
      {
        int wb_ind;
        getc(ifp);
        for (int wb_cnt = 0; wb_cnt < nPentax_wb_list2; wb_cnt++)
        {
          wb_ind = getc(ifp);
          if (wb_ind < nPentax_wb_list2)
            FORC4 imgdata.color.WB_Coeffs[Pentax_wb_list2[wb_ind]][c ^ (c >> 1)] = get2();
        }
      }
      else if (tag == 0x0239) // Q-series lens info (LensInfoQ)
      {
        char LensInfo[20];
        fseek(ifp, 2, SEEK_CUR);
        stread(imgdata.lens.makernotes.Lens, 30, ifp);
        strcat(imgdata.lens.makernotes.Lens, " ");
        stread(LensInfo, 20, ifp);
        strcat(imgdata.lens.makernotes.Lens, LensInfo);
      }
    }

    else if (!strncmp(make, "SAMSUNG", 7))
    {
      if (tag == 0x0002)
      {
        if (get4() == 0x2000)
        {
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Samsung_NX;
        }
        else if (!strncmp(model, "NX mini", 7))
        {
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_Samsung_NX_M;
        }
        else
        {
          imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
        }
      }
      else if (tag == 0x0003)
      {
        unique_id = imgdata.lens.makernotes.CamID = get4();
      }
      else if (tag == 0x0043)
      {
        int temp = get4();
        if (temp)
        {
          imgdata.other.CameraTemperature = (float)temp;
          if (get4() == 10)
            imgdata.other.CameraTemperature /= 10.0f;
        }
      }
      else if (tag == 0xa002)
      {
        stmread(imgdata.shootinginfo.BodySerial, len, ifp);
      }
      else if (tag == 0xa003)
      {
        imgdata.lens.makernotes.LensID = get2();
        if (imgdata.lens.makernotes.LensID)
          imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_Samsung_NX;
      }
      else if (tag == 0xa005)
      {
        stmread(imgdata.lens.InternalLensSerial, len, ifp);
      }
      else if (tag == 0xa019)
      {
        imgdata.lens.makernotes.CurAp = getreal(type);
      }
      else if (tag == 0xa01a)
      {
        imgdata.lens.makernotes.FocalLengthIn35mmFormat = get4() / 10.0f;
        if (imgdata.lens.makernotes.FocalLengthIn35mmFormat < 10.0f)
          imgdata.lens.makernotes.FocalLengthIn35mmFormat *= 10.0f;
      }
    }

    else if (!strncasecmp(make, "SONY", 4) || !strncasecmp(make, "Konica", 6) || !strncasecmp(make, "Minolta", 7) ||
             (!strncasecmp(make, "Hasselblad", 10) &&
              (!strncasecmp(model, "Stellar", 7) || !strncasecmp(model, "Lunar", 5) ||
               !strncasecmp(model, "Lusso", 5) || !strncasecmp(model, "HV", 2))))
    {
      parseSonyMakernotes(tag, type, len, nonDNG, table_buf_0x0116, table_buf_0x0116_len, table_buf_0x2010,
                          table_buf_0x2010_len, table_buf_0x9050, table_buf_0x9050_len, table_buf_0x9400,
                          table_buf_0x9400_len, table_buf_0x9402, table_buf_0x9402_len, table_buf_0x9403,
                          table_buf_0x9403_len, table_buf_0x9406, table_buf_0x9406_len, table_buf_0x940c,
                          table_buf_0x940c_len, table_buf_0x940e, table_buf_0x940e_len);
    }

    fseek(ifp, _pos, SEEK_SET);
#endif

    if (tag == 2 && strstr(make, "NIKON") && !iso_speed)
      iso_speed = (get2(), get2());
    if (tag == 37 && strstr(make, "NIKON") && (!iso_speed || iso_speed == 65535))
    {
      unsigned char cc;
      fread(&cc, 1, 1, ifp);
      iso_speed = int(100.0 * libraw_powf64l(2.0f, float(cc) / 12.0 - 5.0));
    }
    if (tag == 4 && len > 26 && len < 35)
    {
      if ((i = (get4(), get2())) != 0x7fff && (!iso_speed || iso_speed == 65535))
        iso_speed = 50 * libraw_powf64l(2.0, i / 32.0 - 4);
#ifdef LIBRAW_LIBRARY_BUILD
      get4();
#else
      if ((i = (get2(), get2())) != 0x7fff && !aperture)
        aperture = libraw_powf64l(2.0, i / 64.0);
#endif
      if ((i = get2()) != 0xffff && !shutter)
        shutter = libraw_powf64l(2.0, (short)i / -32.0);
      wbi = (get2(), get2());
      shot_order = (get2(), get2());
    }
    if ((tag == 4 || tag == 0x114) && !strncmp(make, "KONICA", 6))
    {
      fseek(ifp, tag == 4 ? 140 : 160, SEEK_CUR);
      switch (get2())
      {
      case 72:
        flip = 0;
        break;
      case 76:
        flip = 6;
        break;
      case 82:
        flip = 5;
        break;
      }
    }
    if (tag == 7 && type == 2 && len > 20)
      fgets(model2, 64, ifp);
    if (tag == 8 && type == 4)
      shot_order = get4();
    if (tag == 9 && !strncmp(make, "Canon", 5))
      fread(artist, 64, 1, ifp);
    if (tag == 0xc && len == 4)
      FORC3 cam_mul[(c << 1 | c >> 1) & 3] = getreal(type);
    if (tag == 0xd && type == 7 && get2() == 0xaaaa)
    {
#if 0 /* Canon rotation data is handled by EXIF.Orientation */
      for (c = i = 2; (ushort)c != 0xbbbb && i < len; i++)
        c = c << 8 | fgetc(ifp);
      while ((i += 4) < len - 5)
        if (get4() == 257 && (i = len) && (c = (get4(), fgetc(ifp))) < 3)
          flip = "065"[c] - '0';
#endif
    }

#ifndef LIBRAW_LIBRARY_BUILD
    if (tag == 0x10 && type == 4)
      unique_id = get4();
#endif

#ifdef LIBRAW_LIBRARY_BUILD
    INT64 _pos2 = ftell(ifp);
    if (!strncasecmp(make, "Olympus", 7))
    {
      short nWB, tWB;
      if ((tag == 0x20300108) || (tag == 0x20310109))
        imgdata.makernotes.olympus.ColorSpace = get2();

      if ((tag == 0x20400101) && (len == 2) && (!strncasecmp(model, "E-410", 5) || !strncasecmp(model, "E-510", 5)))
      {
        int i;
        for (i = 0; i < 64; i++)
          imgdata.color.WBCT_Coeffs[i][2] = imgdata.color.WBCT_Coeffs[i][4] = imgdata.color.WB_Coeffs[i][1] =
              imgdata.color.WB_Coeffs[i][3] = 0x100;
        for (i = 64; i < 256; i++)
          imgdata.color.WB_Coeffs[i][1] = imgdata.color.WB_Coeffs[i][3] = 0x100;
      }
      if ((tag >= 0x20400101) && (tag <= 0x20400111))
      {
        nWB = tag - 0x20400101;
        tWB = Oly_wb_list2[nWB << 1];
        ushort CT = Oly_wb_list2[(nWB << 1) | 1];
        int wb[4];
        wb[0] = get2();
        wb[2] = get2();
        if (tWB != 0x100)
        {
          imgdata.color.WB_Coeffs[tWB][0] = wb[0];
          imgdata.color.WB_Coeffs[tWB][2] = wb[2];
        }
        if (CT)
        {
          imgdata.color.WBCT_Coeffs[nWB - 1][0] = CT;
          imgdata.color.WBCT_Coeffs[nWB - 1][1] = wb[0];
          imgdata.color.WBCT_Coeffs[nWB - 1][3] = wb[2];
        }
        if (len == 4)
        {
          wb[1] = get2();
          wb[3] = get2();
          if (tWB != 0x100)
          {
            imgdata.color.WB_Coeffs[tWB][1] = wb[1];
            imgdata.color.WB_Coeffs[tWB][3] = wb[3];
          }
          if (CT)
          {
            imgdata.color.WBCT_Coeffs[nWB - 1][2] = wb[1];
            imgdata.color.WBCT_Coeffs[nWB - 1][4] = wb[3];
          }
        }
      }
      if ((tag >= 0x20400112) && (tag <= 0x2040011e))
      {
        nWB = tag - 0x20400112;
        int wbG = get2();
        tWB = Oly_wb_list2[nWB << 1];
        if (nWB)
          imgdata.color.WBCT_Coeffs[nWB - 1][2] = imgdata.color.WBCT_Coeffs[nWB - 1][4] = wbG;
        if (tWB != 0x100)
          imgdata.color.WB_Coeffs[tWB][1] = imgdata.color.WB_Coeffs[tWB][3] = wbG;
      }

      if (tag == 0x20400121)
      {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][2] = get2();
        if (len == 4)
        {
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] = get2();
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] = get2();
        }
      }
      if (tag == 0x2040011f)
      {
        int wbG = get2();
        if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][0])
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Flash][3] = wbG;
        FORC4 if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][0])
            imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Custom1 + c][3] =
            wbG;
      }
      if ((tag == 0x30000110) && strcmp(software, "v757-71"))
      {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] = get2();
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][2] = get2();
        if (len == 2)
        {
          for (int i = 0; i < 256; i++)
            imgdata.color.WB_Coeffs[i][1] = imgdata.color.WB_Coeffs[i][3] = 0x100;
        }
      }
      if ((((tag >= 0x30000120) && (tag <= 0x30000124)) || ((tag >= 0x30000130) && (tag <= 0x30000133))) &&
          strcmp(software, "v757-71"))
      {
        int wb_ind;
        if (tag <= 0x30000124)
          wb_ind = tag - 0x30000120;
        else
          wb_ind = tag - 0x30000130 + 5;
        imgdata.color.WB_Coeffs[Oly_wb_list1[wb_ind]][0] = get2();
        imgdata.color.WB_Coeffs[Oly_wb_list1[wb_ind]][2] = get2();
      }

      if ((tag == 0x20400805) && (len == 2))
      {
        imgdata.makernotes.olympus.OlympusSensorCalibration[0] = getreal(type);
        imgdata.makernotes.olympus.OlympusSensorCalibration[1] = getreal(type);
        FORC4 imgdata.color.linear_max[c] = imgdata.makernotes.olympus.OlympusSensorCalibration[0];
      }
      if (tag == 0x20200306)
      {
        uchar uc;
        fread(&uc, 1, 1, ifp);
        imgdata.makernotes.olympus.AFFineTune = uc;
      }
      if (tag == 0x20200307)
      {
        FORC3 imgdata.makernotes.olympus.AFFineTuneAdj[c] = get2();
      }
      if (tag == 0x20200401)
      {
        imgdata.other.FlashEC = getreal(type);
      }
    }
    fseek(ifp, _pos2, SEEK_SET);

#endif
    if (tag == 0x11 && is_raw && !strncmp(make, "NIKON", 5))
    {
      fseek(ifp, get4() + base, SEEK_SET);
      parse_tiff_ifd(base);
    }
    if (tag == 0x14 && type == 7)
    {
      if (len == 2560)
      {
        fseek(ifp, 1248, SEEK_CUR);
        goto get2_256;
      }
      fread(buf, 1, 10, ifp);
      if (!strncmp(buf, "NRW ", 4))
      {
        fseek(ifp, strcmp(buf + 4, "0100") ? 46 : 1546, SEEK_CUR);
        cam_mul[0] = get4() << 2;
        cam_mul[1] = get4() + get4();
        cam_mul[2] = get4() << 2;
      }
    }
    if (tag == 0x15 && type == 2 && is_raw)
      fread(model, 64, 1, ifp);
    if (strstr(make, "PENTAX"))
    {
      if (tag == 0x1b)
        tag = 0x1018;
      if (tag == 0x1c)
        tag = 0x1017;
    }
    if (tag == 0x1d)
    {
      while ((c = fgetc(ifp)) && c != EOF)
#ifdef LIBRAW_LIBRARY_BUILD
      {
        if ((!custom_serial) && (!isdigit(c)))
        {
          if ((strbuflen(model) == 3) && (!strcmp(model, "D50")))
          {
            custom_serial = 34;
          }
          else
          {
            custom_serial = 96;
          }
        }
#endif
        serial = serial * 10 + (isdigit(c) ? c - '0' : c % 10);
#ifdef LIBRAW_LIBRARY_BUILD
      }
      if (!imgdata.shootinginfo.BodySerial[0])
        sprintf(imgdata.shootinginfo.BodySerial, "%d", serial);
#endif
    }
    if (tag == 0x29 && type == 1)
    { // Canon PowerShot G9
      c = wbi < 18 ? "012347800000005896"[wbi] - '0' : 0;
      fseek(ifp, 8 + c * 32, SEEK_CUR);
      FORC4 cam_mul[c ^ (c >> 1) ^ 1] = get4();
    }
#ifndef LIBRAW_LIBRARY_BUILD
    if (tag == 0x3d && type == 3 && len == 4)
      FORC4 cblack[c ^ c >> 1] = get2() >> (14 - tiff_bps);
#endif
    if (tag == 0x81 && type == 4)
    {
      data_offset = get4();
      fseek(ifp, data_offset + 41, SEEK_SET);
      raw_height = get2() * 2;
      raw_width = get2();
      filters = 0x61616161;
    }
    if ((tag == 0x81 && type == 7) || (tag == 0x100 && type == 7) || (tag == 0x280 && type == 1))
    {
      thumb_offset = ftell(ifp);
      thumb_length = len;
    }
    if (tag == 0x88 && type == 4 && (thumb_offset = get4()))
      thumb_offset += base;
    if (tag == 0x89 && type == 4)
      thumb_length = get4();
    if (tag == 0x8c || tag == 0x96)
      meta_offset = ftell(ifp);
    if (tag == 0x97)
    {
      for (i = 0; i < 4; i++)
        ver97 = ver97 * 10 + fgetc(ifp) - '0';
      switch (ver97)
      {
      case 100:
        fseek(ifp, 68, SEEK_CUR);
        FORC4 cam_mul[(c >> 1) | ((c & 1) << 1)] = get2();
        break;
      case 102:
        fseek(ifp, 6, SEEK_CUR);
        FORC4 cam_mul[c ^ (c >> 1)] = get2();
        break;
      case 103:
        fseek(ifp, 16, SEEK_CUR);
        FORC4 cam_mul[c] = get2();
      }
      if (ver97 >= 200)
      {
        if (ver97 != 205)
          fseek(ifp, 280, SEEK_CUR);
        fread(buf97, 324, 1, ifp);
      }
    }
    if ((tag == 0xa1) && (type == 7) && strncasecmp(make, "Samsung", 7))
    {
      order = 0x4949;
      fseek(ifp, 140, SEEK_CUR);
      FORC3 cam_mul[c] = get4();
    }
    if (tag == 0xa4 && type == 3)
    {
      fseek(ifp, wbi * 48, SEEK_CUR);
      FORC3 cam_mul[c] = get2();
    }

    if (tag == 0xa7)
    { // shutter count
      NikonKey = fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp) ^ fgetc(ifp);
      if ((unsigned)(ver97 - 200) < 17)
      {
        ci = xlat[0][serial & 0xff];
        cj = xlat[1][NikonKey];
        ck = 0x60;
        for (i = 0; i < 324; i++)
          buf97[i] ^= (cj += ci * ck++);
        i = "66666>666;6A;:;55"[ver97 - 200] - '0';
        FORC4 cam_mul[c ^ (c >> 1) ^ (i & 1)] = sget2(buf97 + (i & -2) + c * 2);
      }
#ifdef LIBRAW_LIBRARY_BUILD
      if ((NikonLensDataVersion > 200) && lenNikonLensData)
      {
        if (custom_serial)
        {
          ci = xlat[0][custom_serial];
        }
        else
        {
          ci = xlat[0][serial & 0xff];
        }
        cj = xlat[1][NikonKey];
        ck = 0x60;
        for (i = 0; i < lenNikonLensData; i++)
          table_buf[i] ^= (cj += ci * ck++);
        processNikonLensData(table_buf, lenNikonLensData);
        lenNikonLensData = 0;
        free(table_buf);
      }
      if (ver97 == 601) // Coolpix A
      {
        imgdata.lens.makernotes.LensMount = LIBRAW_MOUNT_FixedLens;
        imgdata.lens.makernotes.CameraMount = LIBRAW_MOUNT_FixedLens;
      }
#endif
    }

    if (tag == 0xb001 && type == 3) // Sony ModelID
    {
      unique_id = get2();
    }
    if (tag == 0x200 && len == 3)
      shot_order = (get4(), get4());
    if (tag == 0x200 && len == 4) // Pentax black level
      FORC4 cblack[c ^ c >> 1] = get2();
    if (tag == 0x201 && len == 4) // Pentax As Shot WB
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
    if (tag == 0x220 && type == 7)
      meta_offset = ftell(ifp);
    if (tag == 0x401 && type == 4 && len == 4)
      FORC4 cblack[c ^ c >> 1] = get4();
#ifdef LIBRAW_LIBRARY_BUILD
    // not corrected for file bitcount, to be patched in open_datastream
    if (tag == 0x03d && strstr(make, "NIKON") && len == 4)
    {
      FORC4 cblack[c ^ c >> 1] = get2();
      i = cblack[3];
      FORC3 if (i > cblack[c]) i = cblack[c];
      FORC4 cblack[c] -= i;
      black += i;
    }
#endif
    if (tag == 0xe01)
    { /* Nikon Capture Note */
#ifdef LIBRAW_LIBRARY_BUILD
      int loopc = 0;
#endif
      order = 0x4949;
      fseek(ifp, 22, SEEK_CUR);
      for (offset = 22; offset + 22 < len; offset += 22 + i)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        if (loopc++ > 1024)
          throw LIBRAW_EXCEPTION_IO_CORRUPT;
#endif
        tag = get4();
        fseek(ifp, 14, SEEK_CUR);
        i = get4() - 4;
        if (tag == 0x76a43207)
          flip = get2();
        else
          fseek(ifp, i, SEEK_CUR);
      }
    }
    if (tag == 0xe80 && len == 256 && type == 7)
    {
      fseek(ifp, 48, SEEK_CUR);
      cam_mul[0] = get2() * 508 * 1.078 / 0x10000;
      cam_mul[2] = get2() * 382 * 1.173 / 0x10000;
    }
    if (tag == 0xf00 && type == 7)
    {
      if (len == 614)
        fseek(ifp, 176, SEEK_CUR);
      else if (len == 734 || len == 1502)
        fseek(ifp, 148, SEEK_CUR);
      else
        goto next;
      goto get2_256;
    }
    if (((tag == 0x1011 && len == 9) || tag == 0x20400200) && strcmp(software, "v757-71"))
      for (i = 0; i < 3; i++)
      {
#ifdef LIBRAW_LIBRARY_BUILD
        if (!imgdata.makernotes.olympus.ColorSpace)
        {
          FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
        }
        else
        {
          FORC3 imgdata.color.ccm[i][c] = ((short)get2()) / 256.0;
        }
#else
        FORC3 cmatrix[i][c] = ((short)get2()) / 256.0;
#endif
      }
    if ((tag == 0x1012 || tag == 0x20400600) && len == 4)
      FORC4 cblack[c ^ c >> 1] = get2();
    if (tag == 0x1017 || tag == 0x20400100)
      cam_mul[0] = get2() / 256.0;
    if (tag == 0x1018 || tag == 0x20400100)
      cam_mul[2] = get2() / 256.0;
    if (tag == 0x2011 && len == 2)
    {
    get2_256:
      order = 0x4d4d;
      cam_mul[0] = get2() / 256.0;
      cam_mul[2] = get2() / 256.0;
    }
    if ((tag | 0x70) == 0x2070 && (type == 4 || type == 13))
      fseek(ifp, get4() + base, SEEK_SET);
#ifdef LIBRAW_LIBRARY_BUILD
    // IB start
    if (tag == 0x2010)
    {
      INT64 _pos3 = ftell(ifp);
      parse_makernote(base, 0x2010);
      fseek(ifp, _pos3, SEEK_SET);
    }

    if (((tag == 0x2020) || (tag == 0x3000) || (tag == 0x2030) || (tag == 0x2031) || (tag == 0x2050)) &&
        ((type == 7) || (type == 13)) && !strncasecmp(make, "Olympus", 7))
    {
      INT64 _pos3 = ftell(ifp);
      parse_makernote(base, tag);
      fseek(ifp, _pos3, SEEK_SET);
    }
// IB end
#endif
    if ((tag == 0x2020) && ((type == 7) || (type == 13)) && !strncmp(buf, "OLYMP", 5))
      parse_thumb_note(base, 257, 258);
    if (tag == 0x2040)
      parse_makernote(base, 0x2040);
    if (tag == 0xb028)
    {
      fseek(ifp, get4() + base, SEEK_SET);
      parse_thumb_note(base, 136, 137);
    }
    if (tag == 0x4001 && len > 500 && len < 100000)
    {
      i = len == 582 ? 50 : len == 653 ? 68 : len == 5120 ? 142 : 126;
      fseek(ifp, i, SEEK_CUR);
      FORC4 cam_mul[c ^ (c >> 1)] = get2();
      for (i += 18; i <= len; i += 10)
      {
        get2();
        FORC4 sraw_mul[c ^ (c >> 1)] = get2();
        if (sraw_mul[1] == 1170)
          break;
      }
    }
    if (!strncasecmp(make, "Samsung", 7))
    {
      if (tag == 0xa020) // get the full Samsung encryption key
        for (i = 0; i < 11; i++)
          SamsungKey[i] = get4();
      if (tag == 0xa021) // get and decode Samsung cam_mul array
        FORC4 cam_mul[c ^ (c >> 1)] = get4() - SamsungKey[c];
#ifdef LIBRAW_LIBRARY_BUILD
      if (tag == 0xa022)
      {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][c ^ (c >> 1)] = get4() - SamsungKey[c + 4];
        if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][0] < (imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] >> 1))
        {
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][1] >> 4;
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Auto][3] >> 4;
        }
      }

      if (tag == 0xa023)
      {
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][0] = get4() - SamsungKey[8];
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] = get4() - SamsungKey[9];
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] = get4() - SamsungKey[10];
        imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][2] = get4() - SamsungKey[0];
        if (imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][0] < (imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] >> 1))
        {
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][1] >> 4;
          imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_Ill_A][3] >> 4;
        }
      }
      if (tag == 0xa024)
      {
        FORC4 imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][c ^ (c >> 1)] = get4() - SamsungKey[c + 1];
        if (imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][0] < (imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] >> 1))
        {
          imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] = imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][1] >> 4;
          imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][3] = imgdata.color.WB_Coeffs[LIBRAW_WBI_D65][3] >> 4;
        }
      }
      /*
            if (tag == 0xa025) {
              i = get4();
              imgdata.color.linear_max[0] = imgdata.color.linear_max[1] = imgdata.color.linear_max[2] =
                  imgdata.color.linear_max[3] = i - SamsungKey[0]; printf ("Samsung 0xa025 %d\n", i); }
      */
      if (tag == 0xa030 && len == 9)
        for (i = 0; i < 3; i++)
          FORC3 imgdata.color.ccm[i][c] = (float)((short)((get4() + SamsungKey[i * 3 + c]))) / 256.0;
#endif
      if (tag == 0xa031 && len == 9) // get and decode Samsung color matrix
        for (i = 0; i < 3; i++)
          FORC3 cmatrix[i][c] = (float)((short)((get4() + SamsungKey[i * 3 + c]))) / 256.0;

      if (tag == 0xa028)
        FORC4 cblack[c ^ (c >> 1)] = get4() - SamsungKey[c];
    }
    else
    {
      // Somebody else use 0xa021 and 0xa028?
      if (tag == 0xa021)
        FORC4 cam_mul[c ^ (c >> 1)] = get4();
      if (tag == 0xa028)
        FORC4 cam_mul[c ^ (c >> 1)] -= get4();
    }
#ifdef LIBRAW_LIBRARY_BUILD
    if (tag == 0x4021 && (imgdata.makernotes.canon.multishot[0] = get4()) &&
        (imgdata.makernotes.canon.multishot[1] = get4()))
    {
      if (len >= 4)
      {
        imgdata.makernotes.canon.multishot[2] = get4();
        imgdata.makernotes.canon.multishot[3] = get4();
      }
      FORC4 cam_mul[c] = 1024;
    }
#else
    if (tag == 0x4021 && get4() && get4())
      FORC4 cam_mul[c] = 1024;
#endif
  next:
    fseek(ifp, save, SEEK_SET);
  }
quit:
  order = sorder;
}