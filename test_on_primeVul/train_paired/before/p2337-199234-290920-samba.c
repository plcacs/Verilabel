static void nbt_name_socket_recv(struct nbt_name_socket *nbtsock)
{
	TALLOC_CTX *tmp_ctx = talloc_new(nbtsock);
	NTSTATUS status;
	enum ndr_err_code ndr_err;
	struct socket_address *src;
	DATA_BLOB blob;
	size_t nread, dsize;
	struct nbt_name_packet *packet;
	struct nbt_name_request *req;

	status = socket_pending(nbtsock->sock, &dsize);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}

	blob = data_blob_talloc(tmp_ctx, NULL, dsize);
	if (blob.data == NULL) {
		talloc_free(tmp_ctx);
		return;
	}

	status = socket_recvfrom(nbtsock->sock, blob.data, blob.length, &nread,
				 tmp_ctx, &src);
	if (!NT_STATUS_IS_OK(status)) {
		talloc_free(tmp_ctx);
		return;
	}

	packet = talloc(tmp_ctx, struct nbt_name_packet);
	if (packet == NULL) {
		talloc_free(tmp_ctx);
		return;
	}

	/* parse the request */
	ndr_err = ndr_pull_struct_blob(&blob, packet, packet,
				       (ndr_pull_flags_fn_t)ndr_pull_nbt_name_packet);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		status = ndr_map_error2ntstatus(ndr_err);
		DEBUG(2,("Failed to parse incoming NBT name packet - %s\n",
			 nt_errstr(status)));
		talloc_free(tmp_ctx);
		return;
	}

	if (DEBUGLVL(10)) {
		DEBUG(10,("Received nbt packet of length %d from %s:%d\n",
			  (int)blob.length, src->addr, src->port));
		NDR_PRINT_DEBUG(nbt_name_packet, packet);
	}

	/* if its not a reply then pass it off to the incoming request
	   handler, if any */
	if (!(packet->operation & NBT_FLAG_REPLY)) {
		if (nbtsock->incoming.handler) {
			nbtsock->incoming.handler(nbtsock, packet, src);
		}
		talloc_free(tmp_ctx);
		return;
	}

	/* find the matching request */
	req = (struct nbt_name_request *)idr_find(nbtsock->idr,
						  packet->name_trn_id);
	if (req == NULL) {
		if (nbtsock->unexpected.handler) {
			nbtsock->unexpected.handler(nbtsock, packet, src);
		} else {
			DEBUG(10,("Failed to match request for incoming name packet id 0x%04x on %p\n",
				 packet->name_trn_id, nbtsock));
		}
		talloc_free(tmp_ctx);
		return;
	}

	talloc_steal(req, packet);
	talloc_steal(req, src);
	talloc_free(tmp_ctx);
	nbt_name_socket_handle_response_packet(req, packet, src);
}