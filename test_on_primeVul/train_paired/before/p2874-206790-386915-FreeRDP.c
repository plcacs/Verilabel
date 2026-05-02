static UINT rdpei_recv_pdu(RDPEI_CHANNEL_CALLBACK* callback, wStream* s)
{
	UINT16 eventId;
	UINT32 pduLength;
	UINT error;
	Stream_Read_UINT16(s, eventId);   /* eventId (2 bytes) */
	Stream_Read_UINT32(s, pduLength); /* pduLength (4 bytes) */
#ifdef WITH_DEBUG_RDPEI
	WLog_DBG(TAG, "rdpei_recv_pdu: eventId: %" PRIu16 " (%s) length: %" PRIu32 "", eventId,
	         rdpei_eventid_string(eventId), pduLength);
#endif

	switch (eventId)
	{
		case EVENTID_SC_READY:
			if ((error = rdpei_recv_sc_ready_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_sc_ready_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			if ((error = rdpei_send_cs_ready_pdu(callback)))
			{
				WLog_ERR(TAG, "rdpei_send_cs_ready_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case EVENTID_SUSPEND_TOUCH:
			if ((error = rdpei_recv_suspend_touch_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_suspend_touch_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		case EVENTID_RESUME_TOUCH:
			if ((error = rdpei_recv_resume_touch_pdu(callback, s)))
			{
				WLog_ERR(TAG, "rdpei_recv_resume_touch_pdu failed with error %" PRIu32 "!", error);
				return error;
			}

			break;

		default:
			break;
	}

	return CHANNEL_RC_OK;
}