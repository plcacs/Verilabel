static BOOL rfx_process_message_tileset(RFX_CONTEXT* context, RFX_MESSAGE* message, wStream* s,
                                        UINT16* pExpectedBlockType)
{
	BOOL rc;
	int i, close_cnt;
	BYTE quant;
	RFX_TILE* tile;
	RFX_TILE** tmpTiles;
	UINT32* quants;
	UINT16 subtype, numTiles;
	UINT32 blockLen;
	UINT32 blockType;
	UINT32 tilesDataSize;
	PTP_WORK* work_objects = NULL;
	RFX_TILE_PROCESS_WORK_PARAM* params = NULL;
	void* pmem;

	if (*pExpectedBlockType != WBT_EXTENSION)
	{
		WLog_ERR(TAG, "%s: message unexpected wants a tileset", __FUNCTION__);
		return FALSE;
	}

	*pExpectedBlockType = WBT_FRAME_END;

	if (Stream_GetRemainingLength(s) < 14)
	{
		WLog_ERR(TAG, "RfxMessageTileSet packet too small");
		return FALSE;
	}

	Stream_Read_UINT16(s, subtype); /* subtype (2 bytes) must be set to CBT_TILESET (0xCAC2) */
	if (subtype != CBT_TILESET)
	{
		WLog_ERR(TAG, "invalid subtype, expected CBT_TILESET.");
		return FALSE;
	}

	Stream_Seek_UINT16(s);                   /* idx (2 bytes), must be set to 0x0000 */
	Stream_Seek_UINT16(s);                   /* properties (2 bytes) */
	Stream_Read_UINT8(s, context->numQuant); /* numQuant (1 byte) */
	Stream_Seek_UINT8(s);                    /* tileSize (1 byte), must be set to 0x40 */

	if (context->numQuant < 1)
	{
		WLog_ERR(TAG, "no quantization value.");
		return FALSE;
	}

	Stream_Read_UINT16(s, numTiles); /* numTiles (2 bytes) */
	if (numTiles < 1)
	{
		/* Windows Server 2012 (not R2) can send empty tile sets */
		return TRUE;
	}

	Stream_Read_UINT32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	if (!(pmem = realloc((void*)context->quants, context->numQuant * 10 * sizeof(UINT32))))
		return FALSE;

	quants = context->quants = (UINT32*)pmem;

	/* quantVals */
	if (Stream_GetRemainingLength(s) < (size_t)(context->numQuant * 5))
	{
		WLog_ERR(TAG, "RfxMessageTileSet packet too small for num_quants=%" PRIu8 "",
		         context->numQuant);
		return FALSE;
	}

	for (i = 0; i < context->numQuant; i++)
	{
		/* RFX_CODEC_QUANT */
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		Stream_Read_UINT8(s, quant);
		*quants++ = (quant & 0x0F);
		*quants++ = (quant >> 4);
		WLog_Print(context->priv->log, WLOG_DEBUG,
		           "quant %d (%" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32
		           " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 ").",
		           i, context->quants[i * 10], context->quants[i * 10 + 1],
		           context->quants[i * 10 + 2], context->quants[i * 10 + 3],
		           context->quants[i * 10 + 4], context->quants[i * 10 + 5],
		           context->quants[i * 10 + 6], context->quants[i * 10 + 7],
		           context->quants[i * 10 + 8], context->quants[i * 10 + 9]);
	}

	for (i = 0; i < message->numTiles; i++)
	{
		ObjectPool_Return(context->priv->TilePool, message->tiles[i]);
		message->tiles[i] = NULL;
	}

	tmpTiles = (RFX_TILE**)realloc(message->tiles, numTiles * sizeof(RFX_TILE*));
	if (!tmpTiles)
		return FALSE;

	message->tiles = tmpTiles;
	message->numTiles = numTiles;

	if (context->priv->UseThreads)
	{
		work_objects = (PTP_WORK*)calloc(message->numTiles, sizeof(PTP_WORK));
		params = (RFX_TILE_PROCESS_WORK_PARAM*)calloc(message->numTiles,
		                                              sizeof(RFX_TILE_PROCESS_WORK_PARAM));

		if (!work_objects)
		{
			free(params);
			return FALSE;
		}

		if (!params)
		{
			free(work_objects);
			return FALSE;
		}
	}

	/* tiles */
	close_cnt = 0;
	rc = TRUE;

	for (i = 0; i < message->numTiles; i++)
	{
		wStream sub;
		if (!(tile = (RFX_TILE*)ObjectPool_Take(context->priv->TilePool)))
		{
			WLog_ERR(TAG, "RfxMessageTileSet failed to get tile from object pool");
			rc = FALSE;
			break;
		}

		message->tiles[i] = tile;

		/* RFX_TILE */
		if (Stream_GetRemainingLength(s) < 6)
		{
			WLog_ERR(TAG, "RfxMessageTileSet packet too small to read tile %d/%" PRIu16 "", i,
			         message->numTiles);
			rc = FALSE;
			break;
		}

		Stream_StaticInit(&sub, Stream_Pointer(s), Stream_GetRemainingLength(s));
		Stream_Read_UINT16(&sub,
		                   blockType); /* blockType (2 bytes), must be set to CBT_TILE (0xCAC3) */
		Stream_Read_UINT32(&sub, blockLen); /* blockLen (4 bytes) */

		if (!Stream_SafeSeek(s, blockLen))
		{
			rc = FALSE;
			break;
		}
		if ((blockLen < 6 + 13) || (Stream_GetRemainingLength(&sub) < blockLen - 6))
		{
			WLog_ERR(TAG,
			         "RfxMessageTileSet not enough bytes to read tile %d/%" PRIu16
			         " with blocklen=%" PRIu32 "",
			         i, message->numTiles, blockLen);
			rc = FALSE;
			break;
		}

		if (blockType != CBT_TILE)
		{
			WLog_ERR(TAG, "unknown block type 0x%" PRIX32 ", expected CBT_TILE (0xCAC3).",
			         blockType);
			rc = FALSE;
			break;
		}

		Stream_Read_UINT8(&sub, tile->quantIdxY);  /* quantIdxY (1 byte) */
		Stream_Read_UINT8(&sub, tile->quantIdxCb); /* quantIdxCb (1 byte) */
		Stream_Read_UINT8(&sub, tile->quantIdxCr); /* quantIdxCr (1 byte) */
		Stream_Read_UINT16(&sub, tile->xIdx);      /* xIdx (2 bytes) */
		Stream_Read_UINT16(&sub, tile->yIdx);      /* yIdx (2 bytes) */
		Stream_Read_UINT16(&sub, tile->YLen);      /* YLen (2 bytes) */
		Stream_Read_UINT16(&sub, tile->CbLen);     /* CbLen (2 bytes) */
		Stream_Read_UINT16(&sub, tile->CrLen);     /* CrLen (2 bytes) */
		Stream_GetPointer(&sub, tile->YData);
		if (!Stream_SafeSeek(&sub, tile->YLen))
		{
			rc = FALSE;
			break;
		}
		Stream_GetPointer(&sub, tile->CbData);
		if (!Stream_SafeSeek(&sub, tile->CbLen))
		{
			rc = FALSE;
			break;
		}
		Stream_GetPointer(&sub, tile->CrData);
		if (!Stream_SafeSeek(&sub, tile->CrLen))
		{
			rc = FALSE;
			break;
		}
		tile->x = tile->xIdx * 64;
		tile->y = tile->yIdx * 64;

		if (context->priv->UseThreads)
		{
			if (!params)
			{
				rc = FALSE;
				break;
			}

			params[i].context = context;
			params[i].tile = message->tiles[i];

			if (!(work_objects[i] =
			          CreateThreadpoolWork(rfx_process_message_tile_work_callback,
			                               (void*)&params[i], &context->priv->ThreadPoolEnv)))
			{
				WLog_ERR(TAG, "CreateThreadpoolWork failed.");
				rc = FALSE;
				break;
			}

			SubmitThreadpoolWork(work_objects[i]);
			close_cnt = i + 1;
		}
		else
		{
			rfx_decode_rgb(context, tile, tile->data, 64 * 4);
		}
	}

	if (context->priv->UseThreads)
	{
		for (i = 0; i < close_cnt; i++)
		{
			WaitForThreadpoolWorkCallbacks(work_objects[i], FALSE);
			CloseThreadpoolWork(work_objects[i]);
		}
	}

	free(work_objects);
	free(params);

	for (i = 0; i < message->numTiles; i++)
	{
		if (!(tile = message->tiles[i]))
			continue;

		tile->YLen = tile->CbLen = tile->CrLen = 0;
		tile->YData = tile->CbData = tile->CrData = NULL;
	}

	return rc;
}