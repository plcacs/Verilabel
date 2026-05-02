static int cbor2json (OSCTXT* pCborCtxt, OSCTXT* pJsonCtxt)
{
   int ret = 0;
   OSOCTET tag, ub;

   /* Read byte from stream */
   ret = rtxReadBytes (pCborCtxt, &ub, 1);
   if (0 != ret) return LOG_RTERR (pCborCtxt, ret);
   tag = ub >> 5;

   /* Switch on tag value */
   switch (tag) {
   case OSRTCBOR_UINT: {
      OSUINTTYPE value;
      ret = rtCborDecUInt (pCborCtxt, ub, &value);
      if (0 != ret) return LOG_RTERR (pCborCtxt, ret);

      /* Encode JSON */
#ifndef _NO_INT64_SUPPORT
      ret = rtJsonEncUInt64Value (pJsonCtxt, value);
#else
      ret = rtJsonEncUIntValue (pJsonCtxt, value);
#endif
      if (0 != ret) return LOG_RTERR (pJsonCtxt, ret);
      break;
   }
   case OSRTCBOR_NEGINT: {
      OSINTTYPE value;
      ret = rtCborDecInt (pCborCtxt, ub, &value);
      if (0 != ret) return LOG_RTERR (pCborCtxt, ret);

      /* Encode JSON */
#ifndef _NO_INT64_SUPPORT
      ret = rtJsonEncInt64Value (pJsonCtxt, value);
#else
      ret = rtJsonEncIntValue (pJsonCtxt, value);
#endif
      if (0 != ret) return LOG_RTERR (pJsonCtxt, ret);
      break;
   }
   case OSRTCBOR_BYTESTR: {
      OSDynOctStr64 byteStr;
      ret = rtCborDecDynByteStr (pCborCtxt, ub, &byteStr);
      if (0 != ret) return LOG_RTERR (pCborCtxt, ret);

      /* Encode JSON */
      ret = rtJsonEncHexStr (pJsonCtxt, byteStr.numocts, byteStr.data);
      rtxMemFreePtr (pCborCtxt, byteStr.data);
      if (0 != ret) return LOG_RTERR (pJsonCtxt, ret);

      break;
   }
   case OSRTCBOR_UTF8STR: {
      OSUTF8CHAR* utf8str;
      ret = rtCborDecDynUTF8Str (pCborCtxt, ub, (char**)&utf8str);
      if (0 != ret) return LOG_RTERR (pCborCtxt, ret);

      ret = rtJsonEncStringValue (pJsonCtxt, utf8str);
      rtxMemFreePtr (pCborCtxt, utf8str);
      if (0 != ret) return LOG_RTERR (pJsonCtxt, ret);

      break;
   }
   case OSRTCBOR_ARRAY: 
   case OSRTCBOR_MAP: {
      OSOCTET len = ub & 0x1F;
      char startChar = (tag == OSRTCBOR_ARRAY) ? '[' : '{';
      char endChar = (tag == OSRTCBOR_ARRAY) ? ']' : '}';

      OSRTSAFEPUTCHAR (pJsonCtxt, startChar);

      if (len == OSRTCBOR_INDEF) {
         OSBOOL first = TRUE;
         for (;;) {
            if (OSRTCBOR_MATCHEOC (pCborCtxt)) {
               pCborCtxt->buffer.byteIndex++;
               break;
            }

            if (!first) 
               OSRTSAFEPUTCHAR (pJsonCtxt, ',');
            else
               first = FALSE;

            /* If map, decode object name */
            if (tag == OSRTCBOR_MAP) {
               ret = cborElemNameToJson (pCborCtxt, pJsonCtxt);
            }

            /* Make recursive call */
            if (0 == ret)
               ret = cbor2json (pCborCtxt, pJsonCtxt);
            if (0 != ret) {
               OSCTXT* pctxt = 
                  (rtxErrGetErrorCnt(pJsonCtxt) > 0) ? pJsonCtxt : pCborCtxt;
               return LOG_RTERR (pctxt, ret);
            }
         }
      }
      else { /* definite length */
         OSSIZE nitems;

         /* Decode tag and number of items */
         ret = rtCborDecSize (pCborCtxt, len, &nitems);
         if (0 == ret) {
            OSSIZE i;

            /* Loop to decode array items */
            for (i = 0; i < nitems; i++) {
               if (0 != i) OSRTSAFEPUTCHAR (pJsonCtxt, ',');

               /* If map, decode object name */
               if (tag == OSRTCBOR_MAP) {
                  ret = cborElemNameToJson (pCborCtxt, pJsonCtxt);
               }

               /* Make recursive call */
               if (0 == ret)
                  ret = cbor2json (pCborCtxt, pJsonCtxt);
               if (0 != ret) {
                  OSCTXT* pctxt = 
                  (rtxErrGetErrorCnt(pJsonCtxt) > 0) ? pJsonCtxt : pCborCtxt;
                  return LOG_RTERR (pctxt, ret);
               }
            }
         }
      }
      OSRTSAFEPUTCHAR (pJsonCtxt, endChar);
      break;
   }

   case OSRTCBOR_FLOAT:
      if (tag == OSRTCBOR_FALSEENC || tag == OSRTCBOR_TRUEENC) {
         OSBOOL boolval = (ub == OSRTCBOR_TRUEENC) ? TRUE : FALSE;
         ret = rtJsonEncBoolValue (pJsonCtxt, boolval);
         if (0 != ret) return LOG_RTERR (pJsonCtxt, ret);
      }
      else if (tag == OSRTCBOR_FLT16ENC ||
               tag == OSRTCBOR_FLT32ENC ||
               tag == OSRTCBOR_FLT64ENC) {
         OSDOUBLE fltval;
         ret = rtCborDecFloat (pCborCtxt, ub, &fltval);
         if (0 != ret) return LOG_RTERR (pCborCtxt, ret);

         /* Encode JSON */
         ret = rtJsonEncDoubleValue (pJsonCtxt, fltval, 0);
         if (0 != ret) return LOG_RTERR (pJsonCtxt, ret);
      }
      else {
         ret = cborTagNotSupp (pCborCtxt, tag);
      }
      break;

   default:
      ret = cborTagNotSupp (pCborCtxt, tag);
   }

   return ret;
}