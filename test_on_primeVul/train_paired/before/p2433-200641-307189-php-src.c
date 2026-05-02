static void exif_iif_add_value(image_info_type *image_info, int section_index, char *name, int tag, int format, int length, void* value, int motorola_intel)
{
	size_t idex;
	void *vptr;
	image_info_value *info_value;
	image_info_data  *info_data;
	image_info_data  *list;

	if (length < 0) {
		return;
	}

	list = safe_erealloc(image_info->info_list[section_index].list, (image_info->info_list[section_index].count+1), sizeof(image_info_data), 0);
	image_info->info_list[section_index].list = list;

	info_data  = &image_info->info_list[section_index].list[image_info->info_list[section_index].count];
	memset(info_data, 0, sizeof(image_info_data));
	info_data->tag    = tag;
	info_data->format = format;
	info_data->length = length;
	info_data->name   = estrdup(name);
	info_value        = &info_data->value;

	switch (format) {
		case TAG_FMT_STRING:
			if (value) {
				length = php_strnlen(value, length);
				info_value->s = estrndup(value, length);
				info_data->length = length;
			} else {
				info_data->length = 0;
				info_value->s = estrdup("");
			}
			break;

		default:
			/* Standard says more types possible but skip them...
			 * but allow users to handle data if they know how to
			 * So not return but use type UNDEFINED
			 * return;
			 */
			info_data->tag = TAG_FMT_UNDEFINED;/* otherwise not freed from memory */
		case TAG_FMT_SBYTE:
		case TAG_FMT_BYTE:
		/* in contrast to strings bytes do not need to allocate buffer for NULL if length==0 */
			if (!length)
				break;
		case TAG_FMT_UNDEFINED:
			if (value) {
				if (tag == TAG_MAKER_NOTE) {
					length = (int) php_strnlen(value, length);
				}

				/* do not recompute length here */
				info_value->s = estrndup(value, length);
				info_data->length = length;
			} else {
				info_data->length = 0;
				info_value->s = estrdup("");
			}
			break;

		case TAG_FMT_USHORT:
		case TAG_FMT_ULONG:
		case TAG_FMT_URATIONAL:
		case TAG_FMT_SSHORT:
		case TAG_FMT_SLONG:
		case TAG_FMT_SRATIONAL:
		case TAG_FMT_SINGLE:
		case TAG_FMT_DOUBLE:
			if (length==0) {
				break;
			} else
			if (length>1) {
				info_value->list = safe_emalloc(length, sizeof(image_info_value), 0);
			} else {
				info_value = &info_data->value;
			}
			for (idex=0,vptr=value; idex<(size_t)length; idex++,vptr=(char *) vptr + php_tiff_bytes_per_format[format]) {
				if (length>1) {
					info_value = &info_data->value.list[idex];
				}
				switch (format) {
					case TAG_FMT_USHORT:
						info_value->u = php_ifd_get16u(vptr, motorola_intel);
						break;

					case TAG_FMT_ULONG:
						info_value->u = php_ifd_get32u(vptr, motorola_intel);
						break;

					case TAG_FMT_URATIONAL:
						info_value->ur.num = php_ifd_get32u(vptr, motorola_intel);
						info_value->ur.den = php_ifd_get32u(4+(char *)vptr, motorola_intel);
						break;

					case TAG_FMT_SSHORT:
						info_value->i = php_ifd_get16s(vptr, motorola_intel);
						break;

					case TAG_FMT_SLONG:
						info_value->i = php_ifd_get32s(vptr, motorola_intel);
						break;

					case TAG_FMT_SRATIONAL:
						info_value->sr.num = php_ifd_get32u(vptr, motorola_intel);
						info_value->sr.den = php_ifd_get32u(4+(char *)vptr, motorola_intel);
						break;

					case TAG_FMT_SINGLE:
#ifdef EXIF_DEBUG
						php_error_docref(NULL, E_WARNING, "Found value of type single");
#endif
						info_value->f = *(float *)value;

					case TAG_FMT_DOUBLE:
#ifdef EXIF_DEBUG
						php_error_docref(NULL, E_WARNING, "Found value of type double");
#endif
						info_value->d = *(double *)value;
						break;
				}
			}
	}
	image_info->sections_found |= 1<<section_index;
	image_info->info_list[section_index].count++;
}