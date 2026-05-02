static PresentationContext* PresentationContext_new(VideoClientContext* video, BYTE PresentationId,
                                                    UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
	VideoClientContextPriv* priv = video->priv;
	PresentationContext* ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->video = video;
	ret->PresentationId = PresentationId;

	ret->h264 = h264_context_new(FALSE);
	if (!ret->h264)
	{
		WLog_ERR(TAG, "unable to create a h264 context");
		goto error_h264;
	}
	h264_context_reset(ret->h264, width, height);

	ret->currentSample = Stream_New(NULL, 4096);
	if (!ret->currentSample)
	{
		WLog_ERR(TAG, "unable to create current packet stream");
		goto error_currentSample;
	}

	ret->surfaceData = BufferPool_Take(priv->surfacePool, width * height * 4);
	if (!ret->surfaceData)
	{
		WLog_ERR(TAG, "unable to allocate surfaceData");
		goto error_surfaceData;
	}

	ret->surface = video->createSurface(video, ret->surfaceData, x, y, width, height);
	if (!ret->surface)
	{
		WLog_ERR(TAG, "unable to create surface");
		goto error_surface;
	}

	ret->yuv = yuv_context_new(FALSE);
	if (!ret->yuv)
	{
		WLog_ERR(TAG, "unable to create YUV decoder");
		goto error_yuv;
	}

	yuv_context_reset(ret->yuv, width, height);
	ret->refCounter = 1;
	return ret;

error_yuv:
	video->deleteSurface(video, ret->surface);
error_surface:
	BufferPool_Return(priv->surfacePool, ret->surfaceData);
error_surfaceData:
	Stream_Free(ret->currentSample, TRUE);
error_currentSample:
	h264_context_free(ret->h264);
error_h264:
	free(ret);
	return NULL;
}