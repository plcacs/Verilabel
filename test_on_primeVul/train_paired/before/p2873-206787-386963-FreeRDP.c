static UINT serial_process_irp_write(SERIAL_DEVICE* serial, IRP* irp)
{
	UINT32 Length;
	UINT64 Offset;
	DWORD nbWritten = 0;

	if (Stream_GetRemainingLength(irp->input) < 32)
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(irp->input, Length); /* Length (4 bytes) */
	Stream_Read_UINT64(irp->input, Offset); /* Offset (8 bytes) */
	Stream_Seek(irp->input, 20);            /* Padding (20 bytes) */
	/* MS-RDPESP 3.2.5.1.5: The Offset field is ignored
	 * assert(Offset == 0);
	 *
	 * Using a serial printer, noticed though this field could be
	 * set.
	 */
	WLog_Print(serial->log, WLOG_DEBUG, "writing %" PRIu32 " bytes to %s", Length,
	           serial->device.name);

	/* FIXME: CommWriteFile to be replaced by WriteFile */
	if (CommWriteFile(serial->hComm, Stream_Pointer(irp->input), Length, &nbWritten, NULL))
	{
		irp->IoStatus = STATUS_SUCCESS;
	}
	else
	{
		WLog_Print(serial->log, WLOG_DEBUG,
		           "write failure to %s, nbWritten=%" PRIu32 ", last-error: 0x%08" PRIX32 "",
		           serial->device.name, nbWritten, GetLastError());
		irp->IoStatus = _GetLastErrorToIoStatus(serial);
	}

	WLog_Print(serial->log, WLOG_DEBUG, "%" PRIu32 " bytes written to %s", nbWritten,
	           serial->device.name);
	Stream_Write_UINT32(irp->output, nbWritten); /* Length (4 bytes) */
	Stream_Write_UINT8(irp->output, 0);          /* Padding (1 byte) */
	return CHANNEL_RC_OK;
}