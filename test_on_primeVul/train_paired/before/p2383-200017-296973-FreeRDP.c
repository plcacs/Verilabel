SECURITY_STATUS ntlm_read_ChallengeMessage(NTLM_CONTEXT* context, PSecBuffer buffer)
{
	wStream* s;
	int length;
	PBYTE StartOffset;
	PBYTE PayloadOffset;
	NTLM_AV_PAIR* AvTimestamp;
	NTLM_CHALLENGE_MESSAGE* message;
	ntlm_generate_client_challenge(context);
	message = &context->CHALLENGE_MESSAGE;
	ZeroMemory(message, sizeof(NTLM_CHALLENGE_MESSAGE));
	s = Stream_New((BYTE*)buffer->pvBuffer, buffer->cbBuffer);

	if (!s)
		return SEC_E_INTERNAL_ERROR;

	StartOffset = Stream_Pointer(s);

	if (ntlm_read_message_header(s, (NTLM_MESSAGE_HEADER*)message) < 0)
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	if (message->MessageType != MESSAGE_TYPE_CHALLENGE)
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	if (ntlm_read_message_fields(s, &(message->TargetName)) < 0) /* TargetNameFields (8 bytes) */
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	if (Stream_GetRemainingLength(s) < 4)
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	Stream_Read_UINT32(s, message->NegotiateFlags); /* NegotiateFlags (4 bytes) */
	context->NegotiateFlags = message->NegotiateFlags;

	if (Stream_GetRemainingLength(s) < 8)
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	Stream_Read(s, message->ServerChallenge, 8); /* ServerChallenge (8 bytes) */
	CopyMemory(context->ServerChallenge, message->ServerChallenge, 8);

	if (Stream_GetRemainingLength(s) < 8)
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	Stream_Read(s, message->Reserved, 8); /* Reserved (8 bytes), should be ignored */

	if (ntlm_read_message_fields(s, &(message->TargetInfo)) < 0) /* TargetInfoFields (8 bytes) */
	{
		Stream_Free(s, FALSE);
		return SEC_E_INVALID_TOKEN;
	}

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
	{
		if (ntlm_read_version_info(s, &(message->Version)) < 0) /* Version (8 bytes) */
		{
			Stream_Free(s, FALSE);
			return SEC_E_INVALID_TOKEN;
		}
	}

	/* Payload (variable) */
	PayloadOffset = Stream_Pointer(s);

	if (message->TargetName.Len > 0)
	{
		if (ntlm_read_message_fields_buffer(s, &(message->TargetName)) < 0)
		{
			Stream_Free(s, FALSE);
			return SEC_E_INTERNAL_ERROR;
		}
	}

	if (message->TargetInfo.Len > 0)
	{
		size_t cbAvTimestamp;

		if (ntlm_read_message_fields_buffer(s, &(message->TargetInfo)) < 0)
		{
			Stream_Free(s, FALSE);
			return SEC_E_INTERNAL_ERROR;
		}

		context->ChallengeTargetInfo.pvBuffer = message->TargetInfo.Buffer;
		context->ChallengeTargetInfo.cbBuffer = message->TargetInfo.Len;
		AvTimestamp = ntlm_av_pair_get((NTLM_AV_PAIR*)message->TargetInfo.Buffer,
		                               message->TargetInfo.Len, MsvAvTimestamp, &cbAvTimestamp);

		if (AvTimestamp)
		{
			PBYTE ptr = ntlm_av_pair_get_value_pointer(AvTimestamp);

			if (!ptr)
				return SEC_E_INTERNAL_ERROR;

			if (context->NTLMv2)
				context->UseMIC = TRUE;

			CopyMemory(context->ChallengeTimestamp, ptr, 8);
		}
	}

	length = (PayloadOffset - StartOffset) + message->TargetName.Len + message->TargetInfo.Len;

	if (!sspi_SecBufferAlloc(&context->ChallengeMessage, length))
	{
		Stream_Free(s, FALSE);
		return SEC_E_INTERNAL_ERROR;
	}

	CopyMemory(context->ChallengeMessage.pvBuffer, StartOffset, length);
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "CHALLENGE_MESSAGE (length = %d)", length);
	winpr_HexDump(TAG, WLOG_DEBUG, context->ChallengeMessage.pvBuffer,
	              context->ChallengeMessage.cbBuffer);
	ntlm_print_negotiate_flags(context->NegotiateFlags);

	if (context->NegotiateFlags & NTLMSSP_NEGOTIATE_VERSION)
		ntlm_print_version_info(&(message->Version));

	ntlm_print_message_fields(&(message->TargetName), "TargetName");
	ntlm_print_message_fields(&(message->TargetInfo), "TargetInfo");

	if (context->ChallengeTargetInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "ChallengeTargetInfo (%" PRIu32 "):", context->ChallengeTargetInfo.cbBuffer);
		ntlm_print_av_pair_list(context->ChallengeTargetInfo.pvBuffer,
		                        context->ChallengeTargetInfo.cbBuffer);
	}

#endif
	/* AV_PAIRs */

	if (context->NTLMv2)
	{
		if (ntlm_construct_authenticate_target_info(context) < 0)
		{
			Stream_Free(s, FALSE);
			return SEC_E_INTERNAL_ERROR;
		}

		sspi_SecBufferFree(&context->ChallengeTargetInfo);
		context->ChallengeTargetInfo.pvBuffer = context->AuthenticateTargetInfo.pvBuffer;
		context->ChallengeTargetInfo.cbBuffer = context->AuthenticateTargetInfo.cbBuffer;
	}

	ntlm_generate_timestamp(context); /* Timestamp */

	if (ntlm_compute_lm_v2_response(context) < 0) /* LmChallengeResponse */
	{
		Stream_Free(s, FALSE);
		return SEC_E_INTERNAL_ERROR;
	}

	if (ntlm_compute_ntlm_v2_response(context) < 0) /* NtChallengeResponse */
	{
		Stream_Free(s, FALSE);
		return SEC_E_INTERNAL_ERROR;
	}

	ntlm_generate_key_exchange_key(context);     /* KeyExchangeKey */
	ntlm_generate_random_session_key(context);   /* RandomSessionKey */
	ntlm_generate_exported_session_key(context); /* ExportedSessionKey */
	ntlm_encrypt_random_session_key(context);    /* EncryptedRandomSessionKey */
	/* Generate signing keys */
	ntlm_generate_client_signing_key(context);
	ntlm_generate_server_signing_key(context);
	/* Generate sealing keys */
	ntlm_generate_client_sealing_key(context);
	ntlm_generate_server_sealing_key(context);
	/* Initialize RC4 seal state using client sealing key */
	ntlm_init_rc4_seal_states(context);
#ifdef WITH_DEBUG_NTLM
	WLog_DBG(TAG, "ClientChallenge");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientChallenge, 8);
	WLog_DBG(TAG, "ServerChallenge");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerChallenge, 8);
	WLog_DBG(TAG, "SessionBaseKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->SessionBaseKey, 16);
	WLog_DBG(TAG, "KeyExchangeKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->KeyExchangeKey, 16);
	WLog_DBG(TAG, "ExportedSessionKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ExportedSessionKey, 16);
	WLog_DBG(TAG, "RandomSessionKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->RandomSessionKey, 16);
	WLog_DBG(TAG, "ClientSigningKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientSigningKey, 16);
	WLog_DBG(TAG, "ClientSealingKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ClientSealingKey, 16);
	WLog_DBG(TAG, "ServerSigningKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerSigningKey, 16);
	WLog_DBG(TAG, "ServerSealingKey");
	winpr_HexDump(TAG, WLOG_DEBUG, context->ServerSealingKey, 16);
	WLog_DBG(TAG, "Timestamp");
	winpr_HexDump(TAG, WLOG_DEBUG, context->Timestamp, 8);
#endif
	context->state = NTLM_STATE_AUTHENTICATE;
	ntlm_free_message_fields_buffer(&(message->TargetName));
	Stream_Free(s, FALSE);
	return SEC_I_CONTINUE_NEEDED;
}