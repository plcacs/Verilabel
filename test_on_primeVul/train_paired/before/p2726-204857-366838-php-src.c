static size_t
php_mysqlnd_change_auth_response_write(MYSQLND_CONN_DATA * conn, void * _packet)
{
	MYSQLND_PACKET_CHANGE_AUTH_RESPONSE *packet= (MYSQLND_PACKET_CHANGE_AUTH_RESPONSE *) _packet;
	MYSQLND_ERROR_INFO * error_info = conn->error_info;
	MYSQLND_PFC * pfc = conn->protocol_frame_codec;
	MYSQLND_VIO * vio = conn->vio;
	MYSQLND_STATS * stats = conn->stats;
	MYSQLND_CONNECTION_STATE * connection_state = &conn->state;
	zend_uchar * const buffer = pfc->cmd_buffer.length >= packet->auth_data_len? pfc->cmd_buffer.buffer : mnd_emalloc(packet->auth_data_len);
	zend_uchar * p = buffer + MYSQLND_HEADER_SIZE; /* start after the header */

	DBG_ENTER("php_mysqlnd_change_auth_response_write");

	if (packet->auth_data_len) {
		memcpy(p, packet->auth_data, packet->auth_data_len);
		p+= packet->auth_data_len;
	}

	{
		/*
		  The auth handshake packet has no command in it. Thus we can't go over conn->command directly.
		  Well, we can have a command->no_command(conn, payload)
		*/
		const size_t sent = pfc->data->m.send(pfc, vio, buffer, p - buffer - MYSQLND_HEADER_SIZE, stats, error_info);
		if (buffer != pfc->cmd_buffer.buffer) {
			mnd_efree(buffer);
		}
		if (!sent) {
			SET_CONNECTION_STATE(connection_state, CONN_QUIT_SENT);
		}
		DBG_RETURN(sent);
	}