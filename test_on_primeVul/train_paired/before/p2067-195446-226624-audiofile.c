status WAVEFile::parseFormat(const Tag &id, uint32_t size)
{
	Track *track = getTrack();

	uint16_t formatTag;
	readU16(&formatTag);
	uint16_t channelCount;
	readU16(&channelCount);
	uint32_t sampleRate;
	readU32(&sampleRate);
	uint32_t averageBytesPerSecond;
	readU32(&averageBytesPerSecond);
	uint16_t blockAlign;
	readU16(&blockAlign);

	if (!channelCount)
	{
		_af_error(AF_BAD_CHANNELS, "invalid file with 0 channels");
		return AF_FAIL;
	}

	track->f.channelCount = channelCount;
	track->f.sampleRate = sampleRate;
	track->f.byteOrder = AF_BYTEORDER_LITTLEENDIAN;

	/* Default to uncompressed audio data. */
	track->f.compressionType = AF_COMPRESSION_NONE;
	track->f.framesPerPacket = 1;

	switch (formatTag)
	{
		case WAVE_FORMAT_PCM:
		{
			uint16_t bitsPerSample;
			readU16(&bitsPerSample);

			track->f.sampleWidth = bitsPerSample;

			if (bitsPerSample == 0 || bitsPerSample > 32)
			{
				_af_error(AF_BAD_WIDTH,
					"bad sample width of %d bits",
					bitsPerSample);
				return AF_FAIL;
			}

			if (bitsPerSample <= 8)
				track->f.sampleFormat = AF_SAMPFMT_UNSIGNED;
			else
				track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
		}
		break;

		case WAVE_FORMAT_MULAW:
		case IBM_FORMAT_MULAW:
			track->f.sampleWidth = 16;
			track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
			track->f.byteOrder = _AF_BYTEORDER_NATIVE;
			track->f.compressionType = AF_COMPRESSION_G711_ULAW;
			track->f.bytesPerPacket = track->f.channelCount;
			break;

		case WAVE_FORMAT_ALAW:
		case IBM_FORMAT_ALAW:
			track->f.sampleWidth = 16;
			track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
			track->f.byteOrder = _AF_BYTEORDER_NATIVE;
			track->f.compressionType = AF_COMPRESSION_G711_ALAW;
			track->f.bytesPerPacket = track->f.channelCount;
			break;

		case WAVE_FORMAT_IEEE_FLOAT:
		{
			uint16_t bitsPerSample;
			readU16(&bitsPerSample);

			if (bitsPerSample == 64)
			{
				track->f.sampleWidth = 64;
				track->f.sampleFormat = AF_SAMPFMT_DOUBLE;
			}
			else
			{
				track->f.sampleWidth = 32;
				track->f.sampleFormat = AF_SAMPFMT_FLOAT;
			}
		}
		break;

		case WAVE_FORMAT_ADPCM:
		{
			uint16_t bitsPerSample, extraByteCount,
					samplesPerBlock, numCoefficients;

			if (track->f.channelCount != 1 &&
				track->f.channelCount != 2)
			{
				_af_error(AF_BAD_CHANNELS,
					"WAVE file with MS ADPCM compression "
					"must have 1 or 2 channels");
			}

			readU16(&bitsPerSample);
			readU16(&extraByteCount);
			readU16(&samplesPerBlock);
			readU16(&numCoefficients);

			/* numCoefficients should be at least 7. */
			assert(numCoefficients >= 7 && numCoefficients <= 255);

			m_msadpcmNumCoefficients = numCoefficients;

			for (int i=0; i<m_msadpcmNumCoefficients; i++)
			{
				readS16(&m_msadpcmCoefficients[i][0]);
				readS16(&m_msadpcmCoefficients[i][1]);
			}

			track->f.sampleWidth = 16;
			track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
			track->f.compressionType = AF_COMPRESSION_MS_ADPCM;
			track->f.byteOrder = _AF_BYTEORDER_NATIVE;

			track->f.framesPerPacket = samplesPerBlock;
			track->f.bytesPerPacket = blockAlign;

			// Create the parameter list.
			AUpvlist pv = AUpvnew(2);
			AUpvsetparam(pv, 0, _AF_MS_ADPCM_NUM_COEFFICIENTS);
			AUpvsetvaltype(pv, 0, AU_PVTYPE_LONG);
			long l = m_msadpcmNumCoefficients;
			AUpvsetval(pv, 0, &l);

			AUpvsetparam(pv, 1, _AF_MS_ADPCM_COEFFICIENTS);
			AUpvsetvaltype(pv, 1, AU_PVTYPE_PTR);
			void *v = m_msadpcmCoefficients;
			AUpvsetval(pv, 1, &v);

			track->f.compressionParams = pv;
		}
		break;

		case WAVE_FORMAT_DVI_ADPCM:
		{
			uint16_t bitsPerSample, extraByteCount, samplesPerBlock;

			readU16(&bitsPerSample);
			readU16(&extraByteCount);
			readU16(&samplesPerBlock);

			if (bitsPerSample != 4)
			{
				_af_error(AF_BAD_NOT_IMPLEMENTED,
					"IMA ADPCM compression supports only 4 bits per sample");
			}

			int bytesPerBlock = (samplesPerBlock + 14) / 8 * 4 * channelCount;
			if (bytesPerBlock > blockAlign || (samplesPerBlock % 8) != 1)
			{
				_af_error(AF_BAD_CODEC_CONFIG,
					"Invalid samples per block for IMA ADPCM compression");
			}

			track->f.sampleWidth = 16;
			track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
			track->f.compressionType = AF_COMPRESSION_IMA;
			track->f.byteOrder = _AF_BYTEORDER_NATIVE;

			initIMACompressionParams();

			track->f.framesPerPacket = samplesPerBlock;
			track->f.bytesPerPacket = blockAlign;
		}
		break;

		case WAVE_FORMAT_EXTENSIBLE:
		{
			uint16_t bitsPerSample;
			readU16(&bitsPerSample);
			uint16_t extraByteCount;
			readU16(&extraByteCount);
			uint16_t reserved;
			readU16(&reserved);
			uint32_t channelMask;
			readU32(&channelMask);
			UUID subformat;
			readUUID(&subformat);
			if (subformat == _af_wave_guid_pcm)
			{
				track->f.sampleWidth = bitsPerSample;

				if (bitsPerSample == 0 || bitsPerSample > 32)
				{
					_af_error(AF_BAD_WIDTH,
						"bad sample width of %d bits",
						bitsPerSample);
					return AF_FAIL;
				}

				// Use valid bits per sample if bytes per sample is the same.
				if (reserved <= bitsPerSample &&
					(reserved + 7) / 8 == (bitsPerSample + 7) / 8)
					track->f.sampleWidth = reserved;

				if (bitsPerSample <= 8)
					track->f.sampleFormat = AF_SAMPFMT_UNSIGNED;
				else
					track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
			}
			else if (subformat == _af_wave_guid_ieee_float)
			{
				if (bitsPerSample == 64)
				{
					track->f.sampleWidth = 64;
					track->f.sampleFormat = AF_SAMPFMT_DOUBLE;
				}
				else
				{
					track->f.sampleWidth = 32;
					track->f.sampleFormat = AF_SAMPFMT_FLOAT;
				}
			}
			else if (subformat == _af_wave_guid_alaw ||
				subformat == _af_wave_guid_ulaw)
			{
				track->f.compressionType = subformat == _af_wave_guid_alaw ?
					AF_COMPRESSION_G711_ALAW : AF_COMPRESSION_G711_ULAW;
				track->f.sampleWidth = 16;
				track->f.sampleFormat = AF_SAMPFMT_TWOSCOMP;
				track->f.byteOrder = _AF_BYTEORDER_NATIVE;
				track->f.bytesPerPacket = channelCount;
			}
			else
			{
				_af_error(AF_BAD_NOT_IMPLEMENTED, "WAVE extensible data format %s is not currently supported", subformat.name().c_str());
				return AF_FAIL;
			}
		}
		break;

		case WAVE_FORMAT_YAMAHA_ADPCM:
		case WAVE_FORMAT_OKI_ADPCM:
		case WAVE_FORMAT_CREATIVE_ADPCM:
		case IBM_FORMAT_ADPCM:
			_af_error(AF_BAD_NOT_IMPLEMENTED, "WAVE ADPCM data format 0x%x is not currently supported", formatTag);
			return AF_FAIL;
			break;

		case WAVE_FORMAT_MPEG:
			_af_error(AF_BAD_NOT_IMPLEMENTED, "WAVE MPEG data format is not supported");
			return AF_FAIL;
			break;

		case WAVE_FORMAT_MPEGLAYER3:
			_af_error(AF_BAD_NOT_IMPLEMENTED, "WAVE MPEG layer 3 data format is not supported");
			return AF_FAIL;
			break;

		default:
			_af_error(AF_BAD_NOT_IMPLEMENTED, "WAVE file data format 0x%x not currently supported != 0xfffe ? %d, != EXTENSIBLE? %d", formatTag, formatTag != 0xfffe, formatTag != WAVE_FORMAT_EXTENSIBLE);
			return AF_FAIL;
			break;
	}

	if (track->f.isUncompressed())
		track->f.computeBytesPerPacketPCM();

	_af_set_sample_format(&track->f, track->f.sampleFormat, track->f.sampleWidth);

	return AF_SUCCEED;
}