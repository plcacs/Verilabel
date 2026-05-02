wf_cliprdr_server_file_contents_request(CliprdrClientContext* context,
                                        const CLIPRDR_FILE_CONTENTS_REQUEST* fileContentsRequest)
{
	DWORD uSize = 0;
	BYTE* pData = NULL;
	HRESULT hRet = S_OK;
	FORMATETC vFormatEtc;
	LPDATAOBJECT pDataObj = NULL;
	STGMEDIUM vStgMedium;
	BOOL bIsStreamFile = TRUE;
	static LPSTREAM pStreamStc = NULL;
	static UINT32 uStreamIdStc = 0;
	wfClipboard* clipboard;
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT sRc;
	UINT32 cbRequested;

	if (!context || !fileContentsRequest)
		return ERROR_INTERNAL_ERROR;

	clipboard = (wfClipboard*)context->custom;

	if (!clipboard)
		return ERROR_INTERNAL_ERROR;

	cbRequested = fileContentsRequest->cbRequested;
	if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		cbRequested = sizeof(UINT64);

	pData = (BYTE*)calloc(1, cbRequested);

	if (!pData)
		goto error;

	hRet = OleGetClipboard(&pDataObj);

	if (FAILED(hRet))
	{
		WLog_ERR(TAG, "filecontents: get ole clipboard failed.");
		goto error;
	}

	ZeroMemory(&vFormatEtc, sizeof(FORMATETC));
	ZeroMemory(&vStgMedium, sizeof(STGMEDIUM));
	vFormatEtc.cfFormat = RegisterClipboardFormat(CFSTR_FILECONTENTS);
	vFormatEtc.tymed = TYMED_ISTREAM;
	vFormatEtc.dwAspect = 1;
	vFormatEtc.lindex = fileContentsRequest->listIndex;
	vFormatEtc.ptd = NULL;

	if ((uStreamIdStc != fileContentsRequest->streamId) || !pStreamStc)
	{
		LPENUMFORMATETC pEnumFormatEtc;
		ULONG CeltFetched;
		FORMATETC vFormatEtc2;

		if (pStreamStc)
		{
			IStream_Release(pStreamStc);
			pStreamStc = NULL;
		}

		bIsStreamFile = FALSE;
		hRet = IDataObject_EnumFormatEtc(pDataObj, DATADIR_GET, &pEnumFormatEtc);

		if (hRet == S_OK)
		{
			do
			{
				hRet = IEnumFORMATETC_Next(pEnumFormatEtc, 1, &vFormatEtc2, &CeltFetched);

				if (hRet == S_OK)
				{
					if (vFormatEtc2.cfFormat == RegisterClipboardFormat(CFSTR_FILECONTENTS))
					{
						hRet = IDataObject_GetData(pDataObj, &vFormatEtc, &vStgMedium);

						if (hRet == S_OK)
						{
							pStreamStc = vStgMedium.pstm;
							uStreamIdStc = fileContentsRequest->streamId;
							bIsStreamFile = TRUE;
						}

						break;
					}
				}
			} while (hRet == S_OK);
		}
	}

	if (bIsStreamFile == TRUE)
	{
		if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		{
			STATSTG vStatStg;
			ZeroMemory(&vStatStg, sizeof(STATSTG));
			hRet = IStream_Stat(pStreamStc, &vStatStg, STATFLAG_NONAME);

			if (hRet == S_OK)
			{
				*((UINT32*)&pData[0]) = vStatStg.cbSize.LowPart;
				*((UINT32*)&pData[4]) = vStatStg.cbSize.HighPart;
				uSize = cbRequested;
			}
		}
		else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
		{
			LARGE_INTEGER dlibMove;
			ULARGE_INTEGER dlibNewPosition;
			dlibMove.HighPart = fileContentsRequest->nPositionHigh;
			dlibMove.LowPart = fileContentsRequest->nPositionLow;
			hRet = IStream_Seek(pStreamStc, dlibMove, STREAM_SEEK_SET, &dlibNewPosition);

			if (SUCCEEDED(hRet))
				hRet = IStream_Read(pStreamStc, pData, cbRequested, (PULONG)&uSize);
		}
	}
	else
	{
		if (fileContentsRequest->dwFlags == FILECONTENTS_SIZE)
		{
			*((UINT32*)&pData[0]) =
			    clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeLow;
			*((UINT32*)&pData[4]) =
			    clipboard->fileDescriptor[fileContentsRequest->listIndex]->nFileSizeHigh;
			uSize = cbRequested;
		}
		else if (fileContentsRequest->dwFlags == FILECONTENTS_RANGE)
		{
			BOOL bRet;
			bRet = wf_cliprdr_get_file_contents(
			    clipboard->file_names[fileContentsRequest->listIndex], pData,
			    fileContentsRequest->nPositionLow, fileContentsRequest->nPositionHigh, cbRequested,
			    &uSize);

			if (bRet == FALSE)
			{
				WLog_ERR(TAG, "get file contents failed.");
				uSize = 0;
				goto error;
			}
		}
	}

	rc = CHANNEL_RC_OK;
error:

	if (pDataObj)
		IDataObject_Release(pDataObj);

	if (uSize == 0)
	{
		free(pData);
		pData = NULL;
	}

	sRc =
	    cliprdr_send_response_filecontents(clipboard, fileContentsRequest->streamId, uSize, pData);
	free(pData);

	if (sRc != CHANNEL_RC_OK)
		return sRc;

	return rc;
}