static int ntlm_read_ntlm_v2_client_challenge(wStream* s, NTLMv2_CLIENT_CHALLENGE* challenge)
{
	size_t size;
	Stream_Read_UINT8(s, challenge->RespType);
	Stream_Read_UINT8(s, challenge->HiRespType);
	Stream_Read_UINT16(s, challenge->Reserved1);
	Stream_Read_UINT32(s, challenge->Reserved2);
	Stream_Read(s, challenge->Timestamp, 8);
	Stream_Read(s, challenge->ClientChallenge, 8);
	Stream_Read_UINT32(s, challenge->Reserved3);
	size = Stream_Length(s) - Stream_GetPosition(s);

	if (size > UINT32_MAX)
		return -1;

	challenge->cbAvPairs = size;
	challenge->AvPairs = (NTLM_AV_PAIR*)malloc(challenge->cbAvPairs);

	if (!challenge->AvPairs)
		return -1;

	Stream_Read(s, challenge->AvPairs, size);
	return 1;
}