gxps_archive_input_stream_read (GInputStream  *stream,
				void          *buffer,
				gsize          count,
				GCancellable  *cancellable,
				GError       **error)
{
	GXPSArchiveInputStream *istream = GXPS_ARCHIVE_INPUT_STREAM (stream);
        gssize                  bytes_read;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return -1;

        bytes_read = archive_read_data (istream->zip->archive, buffer, count);
        if (bytes_read < 0) {
                g_set_error_literal (error,
                                     G_IO_ERROR,
                                     g_io_error_from_errno (archive_errno (istream->zip->archive)),
                                     archive_error_string (istream->zip->archive));
                return -1;
        }
        if (bytes_read == 0 && istream->is_interleaved && !gxps_archive_input_stream_is_last_piece (istream)) {
                /* Read next piece */
                gxps_archive_input_stream_next_piece (istream);
                bytes_read = gxps_archive_input_stream_read (stream, buffer, count, cancellable, error);
        }

	return bytes_read;
}