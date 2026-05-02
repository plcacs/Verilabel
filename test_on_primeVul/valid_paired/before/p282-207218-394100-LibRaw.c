void LibRaw::parseSonySRF(unsigned len)
{

  if ((len > 0xfffff) || (len == 0))
    return;

  INT64 save = ftell(ifp);
  INT64 offset =
      0x0310c0 - save; /* for non-DNG this value normally is 0x8ddc */
  if (len < offset || offset < 0)
    return;
  INT64 decrypt_len = offset >> 2; /* master key offset value is the next
                                      un-encrypted metadata field after SRF0 */

  unsigned i, nWB;
  unsigned MasterKey, SRF2Key, RawDataKey;
  INT64 srf_offset, tag_offset, tag_data, tag_dataoffset;
  int tag_dataunitlen;
  uchar *srf_buf;
  short entries;
  unsigned tag_id, tag_type, tag_datalen;

  srf_buf = (uchar *)malloc(len);
  fread(srf_buf, len, 1, ifp);

  offset += srf_buf[offset] << 2;

#define CHECKBUFFER_SGET4(offset)                                              \
  do                                                                           \
  {                                                                            \
    if ((((offset) + 4) > len) || ((offset) < 0))                              \
      goto restore_after_parseSonySRF;                                         \
  } while (0)

#define CHECKBUFFER_SGET2(offset)                                              \
  do                                                                           \
  {                                                                            \
    if ( ((offset + 2) > len) || ((offset) < 0))                               \
      goto restore_after_parseSonySRF;                                         \
  } while (0)

  CHECKBUFFER_SGET4(offset);

  /* master key is stored in big endian */
  MasterKey = ((unsigned)srf_buf[offset] << 24) |
              ((unsigned)srf_buf[offset + 1] << 16) |
              ((unsigned)srf_buf[offset + 2] << 8) |
              (unsigned)srf_buf[offset + 3];

  /* skip SRF0 */
  srf_offset = 0;
  CHECKBUFFER_SGET2(srf_offset);
  entries = sget2(srf_buf + srf_offset);
  if (entries > 1000)
    goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  CHECKBUFFER_SGET4(offset);
  CHECKBUFFER_SGET4(offset + 12 * entries);
  srf_offset = sget4(srf_buf + offset + 12 * entries) -
               save; /* SRF0 ends with SRF1 abs. position */

  /* get SRF1, it has fixed 40 bytes length and contains keys to decode metadata
   * and raw data */
  if (srf_offset < 0 || decrypt_len < srf_offset / 4)
    goto restore_after_parseSonySRF;
  sony_decrypt((unsigned *)(srf_buf + srf_offset), decrypt_len - srf_offset / 4,
               1, MasterKey);
  CHECKBUFFER_SGET2(srf_offset);
  entries = sget2(srf_buf + srf_offset);
  if (entries > 1000)
    goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  tag_offset = offset;

  while (entries--) {
    if (tiff_sget (save, srf_buf, len,
                   &tag_offset, &tag_id, &tag_type, &tag_dataoffset,
                   &tag_datalen, &tag_dataunitlen) == 0) {
      if (tag_id == 0x0000) {
        SRF2Key = sget4(srf_buf + tag_dataoffset);
      } else if (tag_id == 0x0001) {
        RawDataKey = sget4(srf_buf + tag_dataoffset);
      }
    } else goto restore_after_parseSonySRF;
  }
  offset = tag_offset;

  /* get SRF2 */
  CHECKBUFFER_SGET4(offset);
  srf_offset =
      sget4(srf_buf + offset) - save; /* SRFn ends with SRFn+1 position */
  if (srf_offset < 0 || decrypt_len < srf_offset / 4)
    goto restore_after_parseSonySRF;
  sony_decrypt((unsigned *)(srf_buf + srf_offset), decrypt_len - srf_offset / 4,
               1, SRF2Key);
  CHECKBUFFER_SGET2(srf_offset);
  entries = sget2(srf_buf + srf_offset);
  if (entries > 1000)
    goto restore_after_parseSonySRF;
  offset = srf_offset + 2;
  tag_offset = offset;

  while (entries--) {
    if (tiff_sget (save, srf_buf, len,
                   &tag_offset, &tag_id, &tag_type, &tag_dataoffset,
                   &tag_datalen, &tag_dataunitlen) == 0) {
      if ((tag_id >= 0x00c0) && (tag_id <= 0x00ce)) {
        i = (tag_id - 0x00c0) % 3;
        nWB = (tag_id - 0x00c0) / 3;
        icWBC[Sony_SRF_wb_list[nWB]][i] = sget4(srf_buf + tag_dataoffset);
        if (i == 1) {
          icWBC[Sony_SRF_wb_list[nWB]][3] =
            icWBC[Sony_SRF_wb_list[nWB]][i];
        }
      } else if ((tag_id >= 0x00d0) && (tag_id <= 0x00d2)) {
        i = (tag_id - 0x00d0) % 3;
        cam_mul[i] = sget4(srf_buf + tag_dataoffset);
        if (i == 1) {
          cam_mul[3] = cam_mul[i];
        }
      } else switch (tag_id) {
        /*
        0x0002  SRF6Offset
        0x0003  SRFDataOffset (?)
        0x0004  RawDataOffset
        0x0005  RawDataLength
        */
      case 0x0043:
        ilm.MaxAp4MaxFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      case 0x0044:
         ilm.MaxAp4MinFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      case 0x0045:
        ilm.MinFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      case 0x0046:
        ilm.MaxFocal = sgetreal(tag_type, srf_buf + tag_dataoffset);
        break;
      }
    } else goto restore_after_parseSonySRF;
  }
  offset = tag_offset;

restore_after_parseSonySRF:
  free(srf_buf);
  fseek(ifp, save, SEEK_SET);
#undef CHECKBUFFER_SGET4
#undef CHECKBUFFER_SGET2
}