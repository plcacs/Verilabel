bool AudioOutputSpeech::needSamples(unsigned int snum) {
	for (unsigned int i=iLastConsume;i<iBufferFilled;++i)
		pfBuffer[i-iLastConsume]=pfBuffer[i];
	iBufferFilled -= iLastConsume;

	iLastConsume = snum;

	if (iBufferFilled >= snum)
		return bLastAlive;

	float *pOut;
	bool nextalive = bLastAlive;

	while (iBufferFilled < snum) {
		int decodedSamples = iFrameSize;
		resizeBuffer(iBufferFilled + iOutputSize);

		pOut = (srs) ? fResamplerBuffer : (pfBuffer + iBufferFilled);

		if (! bLastAlive) {
			memset(pOut, 0, iFrameSize * sizeof(float));
		} else {
			if (p == &LoopUser::lpLoopy) {
				LoopUser::lpLoopy.fetchFrames();
			}

			int avail = 0;
			int ts = jitter_buffer_get_pointer_timestamp(jbJitter);
			jitter_buffer_ctl(jbJitter, JITTER_BUFFER_GET_AVAILABLE_COUNT, &avail);

			if (p && (ts == 0)) {
				int want = iroundf(p->fAverageAvailable);
				if (avail < want) {
					++iMissCount;
					if (iMissCount < 20) {
						memset(pOut, 0, iFrameSize * sizeof(float));
						goto nextframe;
					}
				}
			}

			if (qlFrames.isEmpty()) {
				QMutexLocker lock(&qmJitter);

				char data[4096];
				JitterBufferPacket jbp;
				jbp.data = data;
				jbp.len = 4096;

				spx_int32_t startofs = 0;

				if (jitter_buffer_get(jbJitter, &jbp, iFrameSize, &startofs) == JITTER_BUFFER_OK) {
					PacketDataStream pds(jbp.data, jbp.len);

					iMissCount = 0;
					ucFlags = static_cast<unsigned char>(pds.next());

					bHasTerminator = false;
					if (umtType == MessageHandler::UDPVoiceOpus) {
						int size;
						pds >> size;

						bHasTerminator = size & 0x2000;
						qlFrames << pds.dataBlock(size & 0x1fff);
					} else {
						unsigned int header = 0;
						do {
							header = static_cast<unsigned int>(pds.next());
							if (header)
								qlFrames << pds.dataBlock(header & 0x7f);
							else
								bHasTerminator = true;
						} while ((header & 0x80) && pds.isValid());
					}

					if (pds.left()) {
						pds >> fPos[0];
						pds >> fPos[1];
						pds >> fPos[2];
					} else {
						fPos[0] = fPos[1] = fPos[2] = 0.0f;
					}

					if (p) {
						float a = static_cast<float>(avail);
						if (avail >= p->fAverageAvailable)
							p->fAverageAvailable = a;
						else
							p->fAverageAvailable *= 0.99f;
					}
				} else {
					jitter_buffer_update_delay(jbJitter, &jbp, NULL);

					iMissCount++;
					if (iMissCount > 10)
						nextalive = false;
				}
			}

			if (! qlFrames.isEmpty()) {
				QByteArray qba = qlFrames.takeFirst();

				if (umtType == MessageHandler::UDPVoiceCELTAlpha || umtType == MessageHandler::UDPVoiceCELTBeta) {
					int wantversion = (umtType == MessageHandler::UDPVoiceCELTAlpha) ? g.iCodecAlpha : g.iCodecBeta;
					if ((p == &LoopUser::lpLoopy) && (! g.qmCodecs.isEmpty())) {
						QMap<int, CELTCodec *>::const_iterator i = g.qmCodecs.constEnd();
						--i;
						wantversion = i.key();
					}
					if (cCodec && (cCodec->bitstreamVersion() != wantversion)) {
						cCodec->celt_decoder_destroy(cdDecoder);
						cdDecoder = NULL;
					}
					if (! cCodec) {
						cCodec = g.qmCodecs.value(wantversion);
						if (cCodec) {
							cdDecoder = cCodec->decoderCreate();
						}
					}
					if (cdDecoder)
						cCodec->decode_float(cdDecoder, qba.isEmpty() ? NULL : reinterpret_cast<const unsigned char *>(qba.constData()), qba.size(), pOut);
					else
						memset(pOut, 0, sizeof(float) * iFrameSize);
				} else if (umtType == MessageHandler::UDPVoiceOpus) {
#ifdef USE_OPUS
					decodedSamples = opus_decode_float(opusState,
					                                   qba.isEmpty() ?
					                                       NULL :
					                                       reinterpret_cast<const unsigned char *>(qba.constData()),
					                                   qba.size(),
					                                   pOut,
					                                   iAudioBufferSize,
					                                   0);
#endif
				} else {
					if (qba.isEmpty()) {
						speex_decode(dsSpeex, NULL, pOut);
					} else {
						speex_bits_read_from(&sbBits, qba.data(), qba.size());
						speex_decode(dsSpeex, &sbBits, pOut);
					}
					for (unsigned int i=0;i<iFrameSize;++i)
						pOut[i] *= (1.0f / 32767.f);
				}

				bool update = true;
				if (p) {
					float &fPowerMax = p->fPowerMax;
					float &fPowerMin = p->fPowerMin;

					float pow = 0.0f;
					for (int i = 0; i < decodedSamples; ++i)
						pow += pOut[i] * pOut[i];
					pow = sqrtf(pow / static_cast<float>(decodedSamples));

					if (pow >= fPowerMax) {
						fPowerMax = pow;
					} else {
						if (pow <= fPowerMin) {
							fPowerMin = pow;
						} else {
							fPowerMax = 0.99f * fPowerMax;
							fPowerMin += 0.0001f * pow;
						}
					}

					update = (pow < (fPowerMin + 0.01f * (fPowerMax - fPowerMin)));
				}
				if (qlFrames.isEmpty() && update)
					jitter_buffer_update_delay(jbJitter, NULL, NULL);

				if (qlFrames.isEmpty() && bHasTerminator)
					nextalive = false;
			} else {
				if (umtType == MessageHandler::UDPVoiceCELTAlpha || umtType == MessageHandler::UDPVoiceCELTBeta) {
					if (cdDecoder)
						cCodec->decode_float(cdDecoder, NULL, 0, pOut);
					else
						memset(pOut, 0, sizeof(float) * iFrameSize);
				} else if (umtType == MessageHandler::UDPVoiceOpus) {
#ifdef USE_OPUS
					decodedSamples = opus_decode_float(opusState, NULL, 0, pOut, iFrameSize, 0);
#endif
				} else {
					speex_decode(dsSpeex, NULL, pOut);
					for (unsigned int i=0;i<iFrameSize;++i)
						pOut[i] *= (1.0f / 32767.f);
				}
			}

			if (! nextalive) {
				for (unsigned int i=0;i<iFrameSize;++i)
					pOut[i] *= fFadeOut[i];
			} else if (ts == 0) {
				for (unsigned int i=0;i<iFrameSize;++i)
					pOut[i] *= fFadeIn[i];
			}

			for (int i = decodedSamples / iFrameSize; i > 0; --i) {
				jitter_buffer_tick(jbJitter);
			}
		}
nextframe:
		spx_uint32_t inlen = decodedSamples;
		spx_uint32_t outlen = static_cast<unsigned int>(ceilf(static_cast<float>(decodedSamples * iMixerFreq) / static_cast<float>(iSampleRate)));
		if (srs && bLastAlive)
			speex_resampler_process_float(srs, 0, fResamplerBuffer, &inlen, pfBuffer + iBufferFilled, &outlen);
		iBufferFilled += outlen;
	}

	if (p) {
		Settings::TalkState ts;
		if (! nextalive)
			ucFlags = 0xFF;
		switch (ucFlags) {
			case 0:
				ts = Settings::Talking;
				break;
			case 1:
				ts = Settings::Shouting;
				break;
			case 0xFF:
				ts = Settings::Passive;
				break;
			default:
				ts = Settings::Whispering;
				break;
		}
		p->setTalking(ts);
	}

	bool tmp = bLastAlive;
	bLastAlive = nextalive;
	return tmp;
}