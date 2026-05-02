static MSUSB_PIPE_DESCRIPTOR** msusb_mspipes_read(wStream* s, UINT32 NumberOfPipes)
{
	UINT32 pnum;
	MSUSB_PIPE_DESCRIPTOR** MsPipes;

	if (Stream_GetRemainingCapacity(s) < 12 * NumberOfPipes)
		return NULL;

	MsPipes = (MSUSB_PIPE_DESCRIPTOR**)calloc(NumberOfPipes, sizeof(MSUSB_PIPE_DESCRIPTOR*));

	if (!MsPipes)
		return NULL;

	for (pnum = 0; pnum < NumberOfPipes; pnum++)
	{
		MSUSB_PIPE_DESCRIPTOR* MsPipe = msusb_mspipe_new();

		if (!MsPipe)
			goto out_error;

		Stream_Read_UINT16(s, MsPipe->MaximumPacketSize);
		Stream_Seek(s, 2);
		Stream_Read_UINT32(s, MsPipe->MaximumTransferSize);
		Stream_Read_UINT32(s, MsPipe->PipeFlags);
		/* Already set to zero by memset
		        MsPipe->PipeHandle	   = 0;
		        MsPipe->bEndpointAddress = 0;
		        MsPipe->bInterval		= 0;
		        MsPipe->PipeType		 = 0;
		        MsPipe->InitCompleted	= 0;
		*/
		MsPipes[pnum] = MsPipe;
	}

	return MsPipes;
out_error:

	for (pnum = 0; pnum < NumberOfPipes; pnum++)
		free(MsPipes[pnum]);

	free(MsPipes);
	return NULL;
}