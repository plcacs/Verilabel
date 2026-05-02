static UINT parallel_process_irp_create(PARALLEL_DEVICE* parallel, IRP* irp)
{
	char* path = NULL;
	int status;
	UINT32 PathLength;
	Stream_Seek(irp->input, 28);
	/* DesiredAccess(4) AllocationSize(8), FileAttributes(4) */
	/* SharedAccess(4) CreateDisposition(4), CreateOptions(4) */
	Stream_Read_UINT32(irp->input, PathLength);
	status = ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)Stream_Pointer(irp->input), PathLength / 2,
	                            &path, 0, NULL, NULL);

	if (status < 1)
		if (!(path = (char*)calloc(1, 1)))
		{
			WLog_ERR(TAG, "calloc failed!");
			return CHANNEL_RC_NO_MEMORY;
		}

	parallel->id = irp->devman->id_sequence++;
	parallel->file = open(parallel->path, O_RDWR);

	if (parallel->file < 0)
	{
		irp->IoStatus = STATUS_ACCESS_DENIED;
		parallel->id = 0;
	}
	else
	{
		/* all read and write operations should be non-blocking */
		if (fcntl(parallel->file, F_SETFL, O_NONBLOCK) == -1)
		{
		}
	}

	Stream_Write_UINT32(irp->output, parallel->id);
	Stream_Write_UINT8(irp->output, 0);
	free(path);
	return irp->Complete(irp);
}