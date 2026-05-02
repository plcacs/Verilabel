ncp_read_kernel(struct ncp_server *server, const char *file_id,
	     __u32 offset, __u16 to_read, char *target, int *bytes_read)
{
	const char *source;
	int result;

	ncp_init_request(server);
	ncp_add_byte(server, 0);
	ncp_add_mem(server, file_id, 6);
	ncp_add_be32(server, offset);
	ncp_add_be16(server, to_read);

	if ((result = ncp_request(server, 72)) != 0) {
		goto out;
	}
	*bytes_read = ncp_reply_be16(server, 0);
	source = ncp_reply_data(server, 2 + (offset & 1));

	memcpy(target, source, *bytes_read);
out:
	ncp_unlock_server(server);
	return result;
}