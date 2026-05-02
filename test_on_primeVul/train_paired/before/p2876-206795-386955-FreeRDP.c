static UINT serial_process_irp_create(SERIAL_DEVICE* serial, IRP* irp)
{
	DWORD DesiredAccess;
	DWORD SharedAccess;
	DWORD CreateDisposition;
	UINT32 PathLength;

	if (Stream_GetRemainingLength(irp->input) < 32)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(irp->input, DesiredAccess);     /* DesiredAccess (4 bytes) */
	Stream_Seek_UINT64(irp->input);                    /* AllocationSize (8 bytes) */
	Stream_Seek_UINT32(irp->input);                    /* FileAttributes (4 bytes) */
	Stream_Read_UINT32(irp->input, SharedAccess);      /* SharedAccess (4 bytes) */
	Stream_Read_UINT32(irp->input, CreateDisposition); /* CreateDisposition (4 bytes) */
	Stream_Seek_UINT32(irp->input);                    /* CreateOptions (4 bytes) */
	Stream_Read_UINT32(irp->input, PathLength);        /* PathLength (4 bytes) */

	if (Stream_GetRemainingLength(irp->input) < PathLength)
		return ERROR_INVALID_DATA;

	Stream_Seek(irp->input, PathLength); /* Path (variable) */
	assert(PathLength == 0);             /* MS-RDPESP 2.2.2.2 */
#ifndef _WIN32
	/* Windows 2012 server sends on a first call :
	 *     DesiredAccess     = 0x00100080: SYNCHRONIZE | FILE_READ_ATTRIBUTES
	 *     SharedAccess      = 0x00000007: FILE_SHARE_DELETE | FILE_SHARE_WRITE | FILE_SHARE_READ
	 *     CreateDisposition = 0x00000001: CREATE_NEW
	 *
	 * then Windows 2012 sends :
	 *     DesiredAccess     = 0x00120089: SYNCHRONIZE | READ_CONTROL | FILE_READ_ATTRIBUTES |
	 * FILE_READ_EA | FILE_READ_DATA SharedAccess      = 0x00000007: FILE_SHARE_DELETE |
	 * FILE_SHARE_WRITE | FILE_SHARE_READ CreateDisposition = 0x00000001: CREATE_NEW
	 *
	 * assert(DesiredAccess == (GENERIC_READ | GENERIC_WRITE));
	 * assert(SharedAccess == 0);
	 * assert(CreateDisposition == OPEN_EXISTING);
	 *
	 */
	WLog_Print(serial->log, WLOG_DEBUG,
	           "DesiredAccess: 0x%" PRIX32 ", SharedAccess: 0x%" PRIX32
	           ", CreateDisposition: 0x%" PRIX32 "",
	           DesiredAccess, SharedAccess, CreateDisposition);
	/* FIXME: As of today only the flags below are supported by CommCreateFileA: */
	DesiredAccess = GENERIC_READ | GENERIC_WRITE;
	SharedAccess = 0;
	CreateDisposition = OPEN_EXISTING;
#endif
	serial->hComm =
	    CreateFile(serial->device.name, DesiredAccess, SharedAccess, NULL, /* SecurityAttributes */
	               CreateDisposition, 0,                                   /* FlagsAndAttributes */
	               NULL);                                                  /* TemplateFile */

	if (!serial->hComm || (serial->hComm == INVALID_HANDLE_VALUE))
	{
		WLog_Print(serial->log, WLOG_WARN, "CreateFile failure: %s last-error: 0x%08" PRIX32 "",
		           serial->device.name, GetLastError());
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		goto error_handle;
	}

	_comm_setServerSerialDriver(serial->hComm, serial->ServerSerialDriverId);
	_comm_set_permissive(serial->hComm, serial->permissive);
	/* NOTE: binary mode/raw mode required for the redirection. On
	 * Linux, CommCreateFileA forces this setting.
	 */
	/* ZeroMemory(&dcb, sizeof(DCB)); */
	/* dcb.DCBlength = sizeof(DCB); */
	/* GetCommState(serial->hComm, &dcb); */
	/* dcb.fBinary = TRUE; */
	/* SetCommState(serial->hComm, &dcb); */
	assert(irp->FileId == 0);
	irp->FileId = irp->devman->id_sequence++; /* FIXME: why not ((WINPR_COMM*)hComm)->fd? */
	irp->IoStatus = STATUS_SUCCESS;
	WLog_Print(serial->log, WLOG_DEBUG, "%s (DeviceId: %" PRIu32 ", FileId: %" PRIu32 ") created.",
	           serial->device.name, irp->device->id, irp->FileId);
error_handle:
	Stream_Write_UINT32(irp->output, irp->FileId); /* FileId (4 bytes) */
	Stream_Write_UINT8(irp->output, 0);            /* Information (1 byte) */
	return CHANNEL_RC_OK;
}