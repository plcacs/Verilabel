static void hidp_process_report(struct hidp_session *session,
				int type, const u8 *data, int len, int intr)
{
	if (len > HID_MAX_BUFFER_SIZE)
		len = HID_MAX_BUFFER_SIZE;

	memcpy(session->input_buf, data, len);
	hid_input_report(session->hid, type, session->input_buf, len, intr);
}