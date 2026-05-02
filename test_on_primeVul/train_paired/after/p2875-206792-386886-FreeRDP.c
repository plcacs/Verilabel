static UINT printer_process_irp_write(PRINTER_DEVICE* printer_dev, IRP* irp)
{
	UINT32 Length;
	UINT64 Offset;
	rdpPrintJob* printjob = NULL;
	UINT error = CHANNEL_RC_OK;
	void* ptr;

	if (Stream_GetRemainingLength(irp->input) < 32)
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(irp->input, Length);
	Stream_Read_UINT64(irp->input, Offset);
	Stream_Seek(irp->input, 20); /* Padding */
	ptr = Stream_Pointer(irp->input);
	if (!Stream_SafeSeek(irp->input, Length))
		return ERROR_INVALID_DATA;
	if (printer_dev->printer)
		printjob = printer_dev->printer->FindPrintJob(printer_dev->printer, irp->FileId);

	if (!printjob)
	{
		irp->IoStatus = STATUS_UNSUCCESSFUL;
		Length = 0;
	}
	else
	{
		error = printjob->Write(printjob, ptr, Length);
	}

	if (error)
	{
		WLog_ERR(TAG, "printjob->Write failed with error %" PRIu32 "!", error);
		return error;
	}

	Stream_Write_UINT32(irp->output, Length);
	Stream_Write_UINT8(irp->output, 0); /* Padding */
	return irp->Complete(irp);
}