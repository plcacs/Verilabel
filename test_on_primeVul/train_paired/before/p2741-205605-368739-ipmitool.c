ipmi_get_channel_cipher_suites(struct ipmi_intf *intf,
                               const char *payload_type,
                               uint8_t channel,
                               struct cipher_suite_info *suites,
                               size_t *count)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	uint8_t rqdata[3];
	uint8_t list_index = 0;
	/* 0x40 sets * 16 bytes per set */
	uint8_t cipher_suite_data[MAX_CIPHER_SUITE_RECORD_OFFSET *
	                          MAX_CIPHER_SUITE_DATA_LEN];
	size_t offset = 0;
	size_t nr_suites = 0;

	if (!suites || !count || !*count)
		return -1;

	nr_suites = *count;
	*count = 0;
	memset(cipher_suite_data, 0, sizeof(cipher_suite_data));

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = IPMI_GET_CHANNEL_CIPHER_SUITES;
	req.msg.data = rqdata;
	req.msg.data_len = sizeof(rqdata);

	rqdata[0] = channel;
	rqdata[1] = ((strncmp(payload_type, "ipmi", 4) == 0)? 0: 1);

	do {
		/* Always ask for cipher suite format */
		rqdata[2] = LIST_ALGORITHMS_BY_CIPHER_SUITE | list_index;
		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			lprintf(LOG_ERR, "Unable to Get Channel Cipher Suites");
			return -1;
		}
		if (rsp->ccode || rsp->data_len < 1) {
			lprintf(LOG_ERR, "Get Channel Cipher Suites failed: %s",
					val2str(rsp->ccode, completion_code_vals));
			return -1;
		}
		/*
		 * We got back cipher suite data -- store it.
		 * printf("copying data to offset %d\n", offset);
		 * printbuf(rsp->data + 1, rsp->data_len - 1, "this is the data");
		 */
		memcpy(cipher_suite_data + offset, rsp->data + 1, rsp->data_len - 1);
		offset += rsp->data_len - 1;

		/*
		 * Increment our list for the next call
		 */
		++list_index;
	} while ((rsp->data_len == (sizeof(uint8_t) + MAX_CIPHER_SUITE_DATA_LEN))
	         && (list_index < MAX_CIPHER_SUITE_RECORD_OFFSET));

	*count = parse_channel_cipher_suite_data(cipher_suite_data, offset, suites,
	                                         nr_suites);
	return 0;
}