xbstream_open(ds_ctxt_t *ctxt, const char *path, MY_STAT *mystat)
{
	ds_file_t		*file;
	ds_stream_file_t	*stream_file;
	ds_stream_ctxt_t	*stream_ctxt;
	ds_ctxt_t		*dest_ctxt;
	xb_wstream_t		*xbstream;
	xb_wstream_file_t	*xbstream_file;


	xb_ad(ctxt->pipe_ctxt != NULL);
	dest_ctxt = ctxt->pipe_ctxt;

	stream_ctxt = (ds_stream_ctxt_t *) ctxt->ptr;

	pthread_mutex_lock(&stream_ctxt->mutex);
	if (stream_ctxt->dest_file == NULL) {
		stream_ctxt->dest_file = ds_open(dest_ctxt, path, mystat);
		if (stream_ctxt->dest_file == NULL) {
			return NULL;
		}
	}
	pthread_mutex_unlock(&stream_ctxt->mutex);

	file = (ds_file_t *) my_malloc(sizeof(ds_file_t) +
				       sizeof(ds_stream_file_t),
				       MYF(MY_FAE));
	stream_file = (ds_stream_file_t *) (file + 1);

	xbstream = stream_ctxt->xbstream;

	xbstream_file = xb_stream_write_open(xbstream, path, mystat,
		                             stream_ctxt,
					     my_xbstream_write_callback);

	if (xbstream_file == NULL) {
		msg("xb_stream_write_open() failed.");
		goto err;
	}

	stream_file->xbstream_file = xbstream_file;
	stream_file->stream_ctxt = stream_ctxt;
	file->ptr = stream_file;
	file->path = stream_ctxt->dest_file->path;

	return file;

err:
	if (stream_ctxt->dest_file) {
		ds_close(stream_ctxt->dest_file);
		stream_ctxt->dest_file = NULL;
	}
	my_free(file);

	return NULL;
}