void auth_client_request_abort(struct auth_client_request **_request)
{
	struct auth_client_request *request = *_request;

	*_request = NULL;

	auth_client_send_cancel(request->conn->client, request->id);
	call_callback(request, AUTH_REQUEST_STATUS_ABORT, NULL, NULL);
	/* remove the request */
	auth_server_connection_remove_request(request->conn, request->id);
	pool_unref(&request->pool);
}