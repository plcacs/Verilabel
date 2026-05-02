static INLINE INT32 planar_skip_plane_rle(const BYTE* pSrcData, UINT32 SrcSize, UINT32 nWidth,
                                          UINT32 nHeight)
{
	UINT32 used = 0;
	UINT32 x, y;
	BYTE controlByte;

	for (y = 0; y < nHeight; y++)
	{
		for (x = 0; x < nWidth;)
		{
			int cRawBytes;
			int nRunLength;

			if (used >= SrcSize)
				return -1;

			controlByte = pSrcData[used++];
			nRunLength = PLANAR_CONTROL_BYTE_RUN_LENGTH(controlByte);
			cRawBytes = PLANAR_CONTROL_BYTE_RAW_BYTES(controlByte);

			if (nRunLength == 1)
			{
				nRunLength = cRawBytes + 16;
				cRawBytes = 0;
			}
			else if (nRunLength == 2)
			{
				nRunLength = cRawBytes + 32;
				cRawBytes = 0;
			}

			used += cRawBytes;
			x += cRawBytes;
			x += nRunLength;

			if (x > nWidth)
				return -1;

			if (used > SrcSize)
				return -1;
		}
	}

	if (used > INT32_MAX)
		return -1;
	return (INT32)used;
}