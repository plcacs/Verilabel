mnote_pentax_entry_get_value (MnotePentaxEntry *entry,
			      char *val, unsigned int maxlen)
{
	ExifLong vl;
	ExifShort vs, vs2;
	int i = 0, j = 0;

	if (!entry) return (NULL);

	memset (val, 0, maxlen);
	maxlen--;

	switch (entry->tag) {
	  case MNOTE_PENTAX_TAG_MODE:
	  case MNOTE_PENTAX_TAG_QUALITY:
	  case MNOTE_PENTAX_TAG_FOCUS:
	  case MNOTE_PENTAX_TAG_FLASH:
	  case MNOTE_PENTAX_TAG_WHITE_BALANCE:
	  case MNOTE_PENTAX_TAG_SHARPNESS:
	  case MNOTE_PENTAX_TAG_CONTRAST:
	  case MNOTE_PENTAX_TAG_SATURATION:
	  case MNOTE_PENTAX_TAG_ISO_SPEED:
	  case MNOTE_PENTAX_TAG_COLOR:
	  case MNOTE_PENTAX2_TAG_MODE:
	  case MNOTE_PENTAX2_TAG_QUALITY:
	  case MNOTE_PENTAX2_TAG_FLASH_MODE:
	  case MNOTE_PENTAX2_TAG_FOCUS_MODE:
	  case MNOTE_PENTAX2_TAG_AFPOINT_SELECTED:
	  case MNOTE_PENTAX2_TAG_AUTO_AFPOINT:
	  case MNOTE_PENTAX2_TAG_WHITE_BALANCE:
	  case MNOTE_PENTAX2_TAG_PICTURE_MODE:
	  case MNOTE_PENTAX2_TAG_IMAGE_SIZE:
	  case MNOTE_CASIO2_TAG_BESTSHOT_MODE:
		CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
		CC2 (entry->components, 1, 2, val, maxlen);
		if (entry->components == 1) {
			vs = exif_get_short (entry->data, entry->order);

			/* search the tag */
			for (i = 0; (items[i].tag && items[i].tag != entry->tag); i++);
			if (!items[i].tag) {
				snprintf (val, maxlen,
					  _("Internal error (unknown value %i)"), vs);
			  	break;
			}

			/* find the value */
			for (j = 0; items[i].elem[j].string &&
			    (items[i].elem[j].index < vs); j++);
			if (items[i].elem[j].index != vs) {
				snprintf (val, maxlen,
					  _("Internal error (unknown value %i)"), vs);
				break;
			}
			strncpy (val, _(items[i].elem[j].string), maxlen);
		} else {
			/* Two-component values */
			CF (entry->format, EXIF_FORMAT_SHORT, val, maxlen);
			CC2 (entry->components, 1, 2, val, maxlen);
			vs = exif_get_short (entry->data, entry->order);
			vs2 = exif_get_short (entry->data+2, entry->order) << 16;

			/* search the tag */
			for (i = 0; (items2[i].tag && items2[i].tag != entry->tag); i++);
			if (!items2[i].tag) {
				snprintf (val, maxlen,
					  _("Internal error (unknown value %i %i)"), vs, vs2);
			  	break;
			}

			/* find the value */
			for (j = 0; items2[i].elem[j].string && ((items2[i].elem[j].index2 < vs2)
				|| ((items2[i].elem[j].index2 == vs2) && (items2[i].elem[j].index1 < vs))); j++);
			if ((items2[i].elem[j].index1 != vs) || (items2[i].elem[j].index2 != vs2)) {
				snprintf (val, maxlen,
					  _("Internal error (unknown value %i %i)"), vs, vs2);
				break;
			}
			strncpy (val, _(items2[i].elem[j].string), maxlen);
		}
		break;

	case MNOTE_PENTAX_TAG_ZOOM:
		CF (entry->format, EXIF_FORMAT_LONG, val, maxlen);
		CC (entry->components, 1, val, maxlen);
		vl = exif_get_long (entry->data, entry->order);
		snprintf (val, maxlen, "%li", (long int) vl);
		break;
	case MNOTE_PENTAX_TAG_PRINTIM:
		CF (entry->format, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (entry->components, 124, val, maxlen);
		snprintf (val, maxlen, _("%i bytes unknown data"),
			entry->size);
		break;
	case MNOTE_PENTAX_TAG_TZ_CITY:
	case MNOTE_PENTAX_TAG_TZ_DST:
		CF (entry->format, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (entry->components, 4, val, maxlen);
		strncpy (val, (char*)entry->data, MIN(maxlen, entry->size));
		break;
	case MNOTE_PENTAX2_TAG_DATE:
		CF (entry->format, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC (entry->components, 4, val, maxlen);
		/* Note: format is UNDEFINED, not SHORT -> order is fixed: MOTOROLA */
		vs = exif_get_short (entry->data, EXIF_BYTE_ORDER_MOTOROLA);
		snprintf (val, maxlen, "%i:%02i:%02i", vs, entry->data[2], entry->data[3]);
		break;
	case MNOTE_PENTAX2_TAG_TIME:
		CF (entry->format, EXIF_FORMAT_UNDEFINED, val, maxlen);
		CC2 (entry->components, 3, 4, val, maxlen);
		snprintf (val, maxlen, "%02i:%02i:%02i", entry->data[0], entry->data[1], entry->data[2]);
		break;
	default:
		switch (entry->format) {
		case EXIF_FORMAT_ASCII:
		  strncpy (val, (char *)entry->data, MIN(maxlen, entry->size));
		  break;
		case EXIF_FORMAT_SHORT:
		  {
			const unsigned char *data = entry->data;
		  	size_t k, len = strlen(val);
		  	for(k=0; k<entry->components; k++) {
				vs = exif_get_short (data, entry->order);
				snprintf (val+len, maxlen-len, "%i ", vs);
				len = strlen(val);
				data += 2;
			}
		  }
		  break;
		case EXIF_FORMAT_LONG:
		  {
			const unsigned char *data = entry->data;
		  	size_t k, len = strlen(val);
		  	for(k=0; k<entry->components; k++) {
				vl = exif_get_long (data, entry->order);
				snprintf (val+len, maxlen-len, "%li", (long int) vl);
				len = strlen(val);
				data += 4;
			}
		  }
		  break;
		case EXIF_FORMAT_UNDEFINED:
		default:
		  snprintf (val, maxlen, _("%i bytes unknown data"),
			  entry->size);
		  break;
		}
		break;
	}

	return (val);
}