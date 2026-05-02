static BOOL clear_decompress_subcode_rlex(wStream* s, UINT32 bitmapDataByteCount, UINT32 width,
                                          UINT32 height, BYTE* pDstData, UINT32 DstFormat,
                                          UINT32 nDstStep, UINT32 nXDstRel, UINT32 nYDstRel,
                                          UINT32 nDstWidth, UINT32 nDstHeight)
{
	UINT32 x = 0, y = 0;
	UINT32 i;
	UINT32 pixelCount;
	UINT32 bitmapDataOffset;
	UINT32 pixelIndex;
	UINT32 numBits;
	BYTE startIndex;
	BYTE stopIndex;
	BYTE suiteIndex;
	BYTE suiteDepth;
	BYTE paletteCount;
	UINT32 palette[128] = { 0 };

	if (Stream_GetRemainingLength(s) < bitmapDataByteCount)
	{
		WLog_ERR(TAG, "stream short %" PRIuz " [%" PRIu32 " expected]",
		         Stream_GetRemainingLength(s), bitmapDataByteCount);
		return FALSE;
	}

	Stream_Read_UINT8(s, paletteCount);
	bitmapDataOffset = 1 + (paletteCount * 3);

	if ((paletteCount > 127) || (paletteCount < 1))
	{
		WLog_ERR(TAG, "paletteCount %" PRIu8 "", paletteCount);
		return FALSE;
	}

	for (i = 0; i < paletteCount; i++)
	{
		BYTE r, g, b;
		Stream_Read_UINT8(s, b);
		Stream_Read_UINT8(s, g);
		Stream_Read_UINT8(s, r);
		palette[i] = FreeRDPGetColor(DstFormat, r, g, b, 0xFF);
	}

	pixelIndex = 0;
	pixelCount = width * height;
	numBits = CLEAR_LOG2_FLOOR[paletteCount - 1] + 1;

	while (bitmapDataOffset < bitmapDataByteCount)
	{
		UINT32 tmp;
		UINT32 color;
		UINT32 runLengthFactor;

		if (Stream_GetRemainingLength(s) < 2)
		{
			WLog_ERR(TAG, "stream short %" PRIuz " [2 expected]", Stream_GetRemainingLength(s));
			return FALSE;
		}

		Stream_Read_UINT8(s, tmp);
		Stream_Read_UINT8(s, runLengthFactor);
		bitmapDataOffset += 2;
		suiteDepth = (tmp >> numBits) & CLEAR_8BIT_MASKS[(8 - numBits)];
		stopIndex = tmp & CLEAR_8BIT_MASKS[numBits];
		startIndex = stopIndex - suiteDepth;

		if (runLengthFactor >= 0xFF)
		{
			if (Stream_GetRemainingLength(s) < 2)
			{
				WLog_ERR(TAG, "stream short %" PRIuz " [2 expected]", Stream_GetRemainingLength(s));
				return FALSE;
			}

			Stream_Read_UINT16(s, runLengthFactor);
			bitmapDataOffset += 2;

			if (runLengthFactor >= 0xFFFF)
			{
				if (Stream_GetRemainingLength(s) < 4)
				{
					WLog_ERR(TAG, "stream short %" PRIuz " [4 expected]",
					         Stream_GetRemainingLength(s));
					return FALSE;
				}

				Stream_Read_UINT32(s, runLengthFactor);
				bitmapDataOffset += 4;
			}
		}

		if (startIndex >= paletteCount)
		{
			WLog_ERR(TAG, "startIndex %" PRIu8 " > paletteCount %" PRIu8 "]", startIndex,
			         paletteCount);
			return FALSE;
		}

		if (stopIndex >= paletteCount)
		{
			WLog_ERR(TAG, "stopIndex %" PRIu8 " > paletteCount %" PRIu8 "]", stopIndex,
			         paletteCount);
			return FALSE;
		}

		suiteIndex = startIndex;

		if (suiteIndex > 127)
		{
			WLog_ERR(TAG, "suiteIndex %" PRIu8 " > 127]", suiteIndex);
			return FALSE;
		}

		color = palette[suiteIndex];

		if ((pixelIndex + runLengthFactor) > pixelCount)
		{
			WLog_ERR(TAG,
			         "pixelIndex %" PRIu32 " + runLengthFactor %" PRIu32 " > pixelCount %" PRIu32
			         "",
			         pixelIndex, runLengthFactor, pixelCount);
			return FALSE;
		}

		for (i = 0; i < runLengthFactor; i++)
		{
			BYTE* pTmpData =
			    &pDstData[(nXDstRel + x) * GetBytesPerPixel(DstFormat) + (nYDstRel + y) * nDstStep];

			if ((nXDstRel + x < nDstWidth) && (nYDstRel + y < nDstHeight))
				WriteColor(pTmpData, DstFormat, color);

			if (++x >= width)
			{
				y++;
				x = 0;
			}
		}

		pixelIndex += runLengthFactor;

		if ((pixelIndex + (suiteDepth + 1)) > pixelCount)
		{
			WLog_ERR(TAG,
			         "pixelIndex %" PRIu32 " + suiteDepth %" PRIu8 " + 1 > pixelCount %" PRIu32 "",
			         pixelIndex, suiteDepth, pixelCount);
			return FALSE;
		}

		for (i = 0; i <= suiteDepth; i++)
		{
			BYTE* pTmpData =
			    &pDstData[(nXDstRel + x) * GetBytesPerPixel(DstFormat) + (nYDstRel + y) * nDstStep];
			UINT32 color = palette[suiteIndex];

			if (suiteIndex > 127)
			{
				WLog_ERR(TAG, "suiteIndex %" PRIu8 " > 127", suiteIndex);
				return FALSE;
			}

			suiteIndex++;

			if ((nXDstRel + x < nDstWidth) && (nYDstRel + y < nDstHeight))
				WriteColor(pTmpData, DstFormat, color);

			if (++x >= width)
			{
				y++;
				x = 0;
			}
		}

		pixelIndex += (suiteDepth + 1);
	}

	if (pixelIndex != pixelCount)
	{
		WLog_ERR(TAG, "pixelIndex %" PRIu32 " != pixelCount %" PRIu32 "", pixelIndex, pixelCount);
		return FALSE;
	}

	return TRUE;
}