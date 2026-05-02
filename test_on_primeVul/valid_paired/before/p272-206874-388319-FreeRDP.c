UINT cliprdr_read_format_list(wStream* s, CLIPRDR_FORMAT_LIST* formatList, BOOL useLongFormatNames)
{
	UINT32 index;
	size_t position;
	BOOL asciiNames;
	int formatNameLength;
	char* szFormatName;
	WCHAR* wszFormatName;
	UINT32 dataLen = formatList->dataLen;
	CLIPRDR_FORMAT* formats = NULL;
	UINT error = CHANNEL_RC_OK;

	asciiNames = (formatList->msgFlags & CB_ASCII_NAMES) ? TRUE : FALSE;

	index = 0;
	formatList->numFormats = 0;
	position = Stream_GetPosition(s);

	if (!formatList->dataLen)
	{
		/* empty format list */
		formatList->formats = NULL;
		formatList->numFormats = 0;
	}
	else if (!useLongFormatNames)
	{
		formatList->numFormats = (dataLen / 36);

		if ((formatList->numFormats * 36) != dataLen)
		{
			WLog_ERR(TAG, "Invalid short format list length: %" PRIu32 "", dataLen);
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

		while (dataLen)
		{
			Stream_Read_UINT32(s, formats[index].formatId); /* formatId (4 bytes) */
			dataLen -= 4;

			formats[index].formatName = NULL;

			/* According to MS-RDPECLIP 2.2.3.1.1.1 formatName is "a 32-byte block containing
			 * the *null-terminated* name assigned to the Clipboard Format: (32 ASCII 8 characters
			 * or 16 Unicode characters)"
			 * However, both Windows RDSH and mstsc violate this specs as seen in the following
			 * example of a transferred short format name string: [R.i.c.h. .T.e.x.t. .F.o.r.m.a.t.]
			 * These are 16 unicode charaters - *without* terminating null !
			 */

			if (asciiNames)
			{
				szFormatName = (char*)Stream_Pointer(s);

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
				wszFormatName = (WCHAR*)Stream_Pointer(s);

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

			Stream_Seek(s, 32);
			dataLen -= 32;
			index++;
		}
	}
	else
	{
		while (dataLen)
		{
			Stream_Seek(s, 4); /* formatId (4 bytes) */
			dataLen -= 4;

			wszFormatName = (WCHAR*)Stream_Pointer(s);

			if (!wszFormatName[0])
				formatNameLength = 0;
			else
				formatNameLength = _wcslen(wszFormatName);

			Stream_Seek(s, (formatNameLength + 1) * 2);
			dataLen -= ((formatNameLength + 1) * 2);

			formatList->numFormats++;
		}

		dataLen = formatList->dataLen;
		Stream_SetPosition(s, position);

		if (formatList->numFormats)
			formats = (CLIPRDR_FORMAT*)calloc(formatList->numFormats, sizeof(CLIPRDR_FORMAT));

		if (!formats)
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

		formatList->formats = formats;

		while (dataLen)
		{
			Stream_Read_UINT32(s, formats[index].formatId); /* formatId (4 bytes) */
			dataLen -= 4;

			formats[index].formatName = NULL;

			wszFormatName = (WCHAR*)Stream_Pointer(s);

			if (!wszFormatName[0])
				formatNameLength = 0;
			else
				formatNameLength = _wcslen(wszFormatName);

			if (formatNameLength)
			{
				if (ConvertFromUnicode(CP_UTF8, 0, wszFormatName, -1, &(formats[index].formatName),
				                       0, NULL, NULL) < 1)
				{
					WLog_ERR(TAG, "failed to convert long clipboard format name");
					error = ERROR_INTERNAL_ERROR;
					goto error_out;
				}
			}

			Stream_Seek(s, (formatNameLength + 1) * 2);
			dataLen -= ((formatNameLength + 1) * 2);

			index++;
		}
	}

	return error;

error_out:
	cliprdr_free_format_list(formatList);
	return error;
}