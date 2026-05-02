FLAC__bool write_bitbuffer_(FLAC__StreamEncoder *encoder, uint32_t samples, FLAC__bool is_last_block)
{
	const FLAC__byte *buffer;
	size_t bytes;

	FLAC__ASSERT(FLAC__bitwriter_is_byte_aligned(encoder->private_->frame));

	if(!FLAC__bitwriter_get_buffer(encoder->private_->frame, &buffer, &bytes)) {
		encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
		return false;
	}

	if(encoder->protected_->verify) {
		encoder->private_->verify.output.data = buffer;
		encoder->private_->verify.output.bytes = bytes;
		if(encoder->private_->verify.state_hint == ENCODER_IN_MAGIC) {
			encoder->private_->verify.needs_magic_hack = true;
		}
		else {
			if(!FLAC__stream_decoder_process_single(encoder->private_->verify.decoder)) {
				FLAC__bitwriter_release_buffer(encoder->private_->frame);
				FLAC__bitwriter_clear(encoder->private_->frame);
				if(encoder->protected_->state != FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA)
					encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR;
				return false;
			}
		}
	}

	if(write_frame_(encoder, buffer, bytes, samples, is_last_block) != FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
		FLAC__bitwriter_release_buffer(encoder->private_->frame);
		FLAC__bitwriter_clear(encoder->private_->frame);
		encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
		return false;
	}

	FLAC__bitwriter_release_buffer(encoder->private_->frame);
	FLAC__bitwriter_clear(encoder->private_->frame);

	if(samples > 0) {
		encoder->private_->streaminfo.data.stream_info.min_framesize = flac_min(bytes, encoder->private_->streaminfo.data.stream_info.min_framesize);
		encoder->private_->streaminfo.data.stream_info.max_framesize = flac_max(bytes, encoder->private_->streaminfo.data.stream_info.max_framesize);
	}

	return true;
}