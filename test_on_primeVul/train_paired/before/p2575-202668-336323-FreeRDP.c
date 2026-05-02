static BOOL update_read_icon_info(wStream* s, ICON_INFO* iconInfo)
{
	BYTE* newBitMask;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;

	Stream_Read_UINT16(s, iconInfo->cacheEntry); /* cacheEntry (2 bytes) */
	Stream_Read_UINT8(s, iconInfo->cacheId);     /* cacheId (1 byte) */
	Stream_Read_UINT8(s, iconInfo->bpp);         /* bpp (1 byte) */

	if ((iconInfo->bpp < 1) || (iconInfo->bpp > 32))
	{
		WLog_ERR(TAG, "invalid bpp value %" PRIu32 "", iconInfo->bpp);
		return FALSE;
	}

	Stream_Read_UINT16(s, iconInfo->width);  /* width (2 bytes) */
	Stream_Read_UINT16(s, iconInfo->height); /* height (2 bytes) */

	/* cbColorTable is only present when bpp is 1, 4 or 8 */
	switch (iconInfo->bpp)
	{
		case 1:
		case 4:
		case 8:
			if (Stream_GetRemainingLength(s) < 2)
				return FALSE;

			Stream_Read_UINT16(s, iconInfo->cbColorTable); /* cbColorTable (2 bytes) */
			break;

		default:
			iconInfo->cbColorTable = 0;
			break;
	}

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, iconInfo->cbBitsMask);  /* cbBitsMask (2 bytes) */
	Stream_Read_UINT16(s, iconInfo->cbBitsColor); /* cbBitsColor (2 bytes) */

	if (Stream_GetRemainingLength(s) < iconInfo->cbBitsMask + iconInfo->cbBitsColor)
		return FALSE;

	/* bitsMask */
	newBitMask = (BYTE*)realloc(iconInfo->bitsMask, iconInfo->cbBitsMask);

	if (!newBitMask)
	{
		free(iconInfo->bitsMask);
		iconInfo->bitsMask = NULL;
		return FALSE;
	}

	iconInfo->bitsMask = newBitMask;
	Stream_Read(s, iconInfo->bitsMask, iconInfo->cbBitsMask);

	/* colorTable */
	if (iconInfo->colorTable == NULL)
	{
		if (iconInfo->cbColorTable)
		{
			iconInfo->colorTable = (BYTE*)malloc(iconInfo->cbColorTable);

			if (!iconInfo->colorTable)
				return FALSE;
		}
	}
	else if (iconInfo->cbColorTable)
	{
		BYTE* new_tab;
		new_tab = (BYTE*)realloc(iconInfo->colorTable, iconInfo->cbColorTable);

		if (!new_tab)
		{
			free(iconInfo->colorTable);
			iconInfo->colorTable = NULL;
			return FALSE;
		}

		iconInfo->colorTable = new_tab;
	}
	else
	{
		free(iconInfo->colorTable);
		iconInfo->colorTable = NULL;
	}

	if (iconInfo->colorTable)
		Stream_Read(s, iconInfo->colorTable, iconInfo->cbColorTable);

	/* bitsColor */
	newBitMask = (BYTE*)realloc(iconInfo->bitsColor, iconInfo->cbBitsColor);

	if (!newBitMask)
	{
		free(iconInfo->bitsColor);
		iconInfo->bitsColor = NULL;
		return FALSE;
	}

	iconInfo->bitsColor = newBitMask;
	Stream_Read(s, iconInfo->bitsColor, iconInfo->cbBitsColor);
	return TRUE;
}