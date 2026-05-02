UINT cliprdr_read_format_list(wStream* s, CLIPRDR_FORMAT_LIST* formatList, BOOL useLongFormatNames)
{
	UINT32 index;
	size_t position;
	BOOL asciiNames;
	int formatNameLength;
	char* szFormatName;
	WCHAR* wszFormatName;
	wStream sub1, sub2;
	CLIPRDR_FORMAT* formats = NULL;
	UINT error = CHANNEL_RC_OK;

	asciiNames = (formatList->msgFlags & CB_ASCII_NAMES) ? TRUE : FALSE;

	index = 0;
	/* empty format list */
	formatList->formats = NULL;
	formatList->numFormats = 0;

	Stream_StaticInit(&sub1, Stream_Pointer(s), formatList->dataLen);
	if (!Stream_SafeSeek(s, formatList->dataLen))
		return ERROR_INVALID_DATA;

	if (!formatList->dataLen)
	{
	}
	else if (!useLongFormatNames)
	{
		const size_t cap = Stream_Capacity(&sub1);
		formatList->numFormats = (cap / 36);

		if ((formatList->numFormats * 36) != cap)
		{
			WLog_ERR(TAG, "Invalid short format list length: %" PRIuz "", cap);
			return ERROR_INTERNAL_ERROR;
		}

		if (formatList->numFormats)
			formats = (CLIPRDR_FORMAT*)calloc(formatList->numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		formatList->formats = formats;

		while (Stream_GetRemainingLength(&sub1) >= 4)
		{
			Stream_Read_UINT32(&sub1, formats[index].formatId); /* formatId (4 bytes) */

			formats[index].formatName = NULL;

			/* According to MS-RDPECLIP 2.2.3.1.1.1 formatName is "a 32-byte block containing
			 * the *null-terminated* name assigned to the Clipboard Format: (32 ASCII 8 characters
			 * or 16 Unicode characters)"
			 * However, both Windows RDSH and mstsc violate this specs as seen in the following
			 * example of a transferred short format name string: [R.i.c.h. .T.e.x.t. .F.o.r.m.a.t.]
			 * These are 16 unicode charaters - *without* terminating null !
			 */

			szFormatName = (char*)Stream_Pointer(&sub1);
			wszFormatName = (WCHAR*)Stream_Pointer(&sub1);
			if (!Stream_SafeSeek(&sub1, 32))
				goto error_out;
			if (asciiNames)
			{
				if (szFormatName[0])
				{
					/* ensure null termination */
					formats[index].formatName = (char*)malloc(32 + 1);
					if (!formats[index].formatName)
					{
						WLog_ERR(TAG, "malloc failed!");
						error = CHANNEL_RC_NO_MEMORY;
						goto error_out;
					}
					CopyMemory(formats[index].formatName, szFormatName, 32);
					formats[index].formatName[32] = '\0';
				}
			}
			else
			{
				if (wszFormatName[0])
				{
					/* ConvertFromUnicode always returns a null-terminated
					 * string on success, even if the source string isn't.
					 */
					if (ConvertFromUnicode(CP_UTF8, 0, wszFormatName, 16,
					                       &(formats[index].formatName), 0, NULL, NULL) < 1)
					{
						WLog_ERR(TAG, "failed to convert short clipboard format name");
						error = ERROR_INTERNAL_ERROR;
						goto error_out;
					}
				}
			}

			index++;
		}
	}
	else
	{
		sub2 = sub1;
		while (Stream_GetRemainingLength(&sub1) > 0)
		{
			size_t rest;
			if (!Stream_SafeSeek(&sub1, 4)) /* formatId (4 bytes) */
				goto error_out;

			wszFormatName = (WCHAR*)Stream_Pointer(&sub1);
			rest = Stream_GetRemainingLength(&sub1);
			formatNameLength = _wcsnlen(wszFormatName, rest / sizeof(WCHAR));

			if (!Stream_SafeSeek(&sub1, (formatNameLength + 1) * sizeof(WCHAR)))
				goto error_out;
			formatList->numFormats++;
		}

		if (formatList->numFormats)
			formats = (CLIPRDR_FORMAT*)calloc(formatList->numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		formatList->formats = formats;

		while (Stream_GetRemainingLength(&sub2) >= 4)
		{
			size_t rest;
			Stream_Read_UINT32(&sub2, formats[index].formatId); /* formatId (4 bytes) */

			formats[index].formatName = NULL;

			wszFormatName = (WCHAR*)Stream_Pointer(&sub2);
			rest = Stream_GetRemainingLength(&sub2);
			formatNameLength = _wcsnlen(wszFormatName, rest / sizeof(WCHAR));
			if (!Stream_SafeSeek(&sub2, (formatNameLength + 1) * sizeof(WCHAR)))
				goto error_out;

			if (formatNameLength)
			{
				if (ConvertFromUnicode(CP_UTF8, 0, wszFormatName, formatNameLength,
				                       &(formats[index].formatName), 0, NULL, NULL) < 1)
				{
					WLog_ERR(TAG, "failed to convert long clipboard format name");
					error = ERROR_INTERNAL_ERROR;
					goto error_out;
				}
			}

			index++;
		}
	}

	return error;

error_out:
	cliprdr_free_format_list(formatList);
	return error;
}