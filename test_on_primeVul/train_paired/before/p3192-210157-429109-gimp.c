static FITS_HDU_LIST *fits_decode_header (FITS_RECORD_LIST *hdr,
                        long hdr_offset, long dat_offset)

{FITS_HDU_LIST *hdulist;
 FITS_DATA *fdat;
 char errmsg[80], key[9];
 int k, bpp, random_groups;
 long mul_axis, data_size, bitpix_supported;

#define FITS_DECODE_CARD(mhdr,mkey,mfdat,mtyp) \
 {strcpy (key, mkey); \
  mfdat = fits_decode_card (fits_search_card (mhdr, mkey), mtyp); \
  if (mfdat == NULL) goto err_missing; }

#define FITS_TRY_CARD(mhdr,mhdu,mkey,mvar,mtyp,unionvar) \
 {FITS_DATA *mfdat = fits_decode_card (fits_search_card (mhdr,mkey), mtyp); \
  mhdu->used.mvar = (mfdat != NULL); \
  if (mhdu->used.mvar) mhdu->mvar = mfdat->unionvar; }

 hdulist = fits_new_hdulist ();
 if (hdulist == NULL)
   FITS_RETURN ("fits_decode_header: Not enough memory", NULL);

 /* Initialize the header data */
 hdulist->header_offset = hdr_offset;
 hdulist->data_offset = dat_offset;

 hdulist->used.simple = (strncmp (hdr->data, "SIMPLE  ", 8) == 0);
 hdulist->used.xtension = (strncmp (hdr->data, "XTENSION", 8) == 0);
 if (hdulist->used.xtension)
 {
   fdat = fits_decode_card (fits_search_card (hdr, "XTENSION"), typ_fstring);
   strcpy (hdulist->xtension, fdat->fstring);
 }

 FITS_DECODE_CARD (hdr, "NAXIS", fdat, typ_flong);
 hdulist->naxis = fdat->flong;

 FITS_DECODE_CARD (hdr, "BITPIX", fdat, typ_flong);
 bpp = hdulist->bitpix = (int)fdat->flong;
 if (   (bpp != 8) && (bpp != 16) && (bpp != 32)
     && (bpp != -32) && (bpp != -64))
 {
   strcpy (errmsg, "fits_decode_header: Invalid BITPIX-value");
   goto err_return;
 }
 if (bpp < 0) bpp = -bpp;
 bpp /= 8;
 hdulist->bpp = bpp;

 FITS_TRY_CARD (hdr, hdulist, "GCOUNT", gcount, typ_flong, flong);
 FITS_TRY_CARD (hdr, hdulist, "PCOUNT", pcount, typ_flong, flong);

 FITS_TRY_CARD (hdr, hdulist, "GROUPS", groups, typ_fbool, fbool);
 random_groups = hdulist->used.groups && hdulist->groups;

 FITS_TRY_CARD (hdr, hdulist, "EXTEND", extend, typ_fbool, fbool);

 if (hdulist->used.xtension)  /* Extension requires GCOUNT and PCOUNT */
 {
   if ((!hdulist->used.gcount) || (!hdulist->used.pcount))
   {
     strcpy (errmsg, "fits_decode_header: Missing GCOUNT/PCOUNT for XTENSION");
     goto err_return;
   }
 }

 mul_axis = 1;

 /* Find all NAXISx-cards */
 for (k = 1; k <= FITS_MAX_AXIS; k++)
 {char naxisn[9];

   sprintf (naxisn, "NAXIS%-3d", k);
   fdat = fits_decode_card (fits_search_card (hdr, naxisn), typ_flong);
   if (fdat == NULL)
   {
     k--;   /* Save the last NAXISk read */
     break;
   }
   hdulist->naxisn[k-1] = (int)fdat->flong;
   if (hdulist->naxisn[k-1] < 0)
   {
     strcpy (errmsg, "fits_decode_header: Negative value in NAXISn");
     goto err_return;
   }
   if ((k == 1) && (random_groups))
   {
     if (hdulist->naxisn[0] != 0)
     {
       strcpy (errmsg, "fits_decode_header: Random groups with NAXIS1 != 0");
       goto err_return;
     }
   }
   else
     mul_axis *= hdulist->naxisn[k-1];
 }

 if ((hdulist->naxis > 0) && (k < hdulist->naxis))
 {
   strcpy (errmsg, "fits_decode_card: Not enough NAXISn-cards");
   goto err_return;
 }

 /* If we have only one dimension, just set the second to size one. */
 /* So we dont have to check for naxis < 2 in some places. */
 if (hdulist->naxis < 2)
   hdulist->naxisn[1] = 1;
 if (hdulist->naxis < 1)
 {
   mul_axis = 0;
   hdulist->naxisn[0] = 1;
 }

 if (hdulist->used.xtension)
   data_size = bpp*hdulist->gcount*(hdulist->pcount + mul_axis);
 else
   data_size = bpp*mul_axis;
 hdulist->udata_size = data_size;  /* Used data size without padding */

 /* Datasize must be a multiple of the FITS logical record size */
 data_size = (data_size + FITS_RECORD_SIZE - 1) / FITS_RECORD_SIZE;
 data_size *= FITS_RECORD_SIZE;
 hdulist->data_size = data_size;


 FITS_TRY_CARD (hdr, hdulist, "BLANK", blank, typ_flong, flong);

 FITS_TRY_CARD (hdr, hdulist, "DATAMIN", datamin, typ_fdouble, fdouble);
 FITS_TRY_CARD (hdr, hdulist, "DATAMAX", datamax, typ_fdouble, fdouble);

 FITS_TRY_CARD (hdr, hdulist, "BZERO", bzero, typ_fdouble, fdouble);
 FITS_TRY_CARD (hdr, hdulist, "BSCALE", bscale, typ_fdouble, fdouble);

 /* Evaluate number of interpretable images for this HDU */
 hdulist->numpic = 0;

 /* We must support this format */
 bitpix_supported =    (hdulist->bitpix > 0)
                    || (   (hdulist->bitpix == -64)
                        && (fits_ieee64_intel || fits_ieee64_motorola))
                    || (   (hdulist->bitpix == -32)
                        && (   fits_ieee32_intel || fits_ieee32_motorola
                            || fits_ieee64_intel || fits_ieee64_motorola));

 if (bitpix_supported)
 {
   if (hdulist->used.simple)
   {
     if (hdulist->naxis > 0)
     {
       hdulist->numpic = 1;
       for (k = 3; k <= hdulist->naxis; k++)
         hdulist->numpic *= hdulist->naxisn[k-1];
     }
   }
   else if (   hdulist->used.xtension
            && (strncmp (hdulist->xtension, "IMAGE", 5) == 0))
   {
     if (hdulist->naxis > 0)
     {
       hdulist->numpic = 1;
       for (k = 3; k <= hdulist->naxis; k++)
         hdulist->numpic *= hdulist->naxisn[k-1];
     }
   }
 }
 else
 {char msg[160];
   sprintf (msg, "fits_decode_header: IEEE floating point format required for\
 BITPIX=%d\nis not supported on this machine", hdulist->bitpix);
   fits_set_error (msg);
 }

 hdulist->header_record_list = hdr;  /* Add header records to the list */
 return (hdulist);

err_missing:
 sprintf (errmsg, "fits_decode_header: missing/invalid %s card", key);

err_return:
 fits_delete_hdulist (hdulist);
 fits_set_error (errmsg);
 return (NULL);

#undef FITS_DECODE_CARD
}