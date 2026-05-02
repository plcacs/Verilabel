UINT rdpgfx_read_rect16(wStream* s, RECTANGLE_16* rect16)
{
	if (Stream_GetRemainingLength(s) < 8)
	{
		WLog_ERR(TAG, "not enough data!");
		return ERROR_INVALID_DATA;
	}

	Stream_Read_UINT16(s, rect16->left);   /* left (2 bytes) */
	Stream_Read_UINT16(s, rect16->top);    /* top (2 bytes) */
	Stream_Read_UINT16(s, rect16->right);  /* right (2 bytes) */
	Stream_Read_UINT16(s, rect16->bottom); /* bottom (2 bytes) */
	if (rect16->left >= rect16->right)
		return ERROR_INVALID_DATA;
	if (rect16->top >= rect16->bottom)
		return ERROR_INVALID_DATA;
	return CHANNEL_RC_OK;
}