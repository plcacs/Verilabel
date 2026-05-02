ipmi_spd_print_fru(struct ipmi_intf * intf, uint8_t id)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct fru_info fru;
	uint8_t *spd_data, msg_data[4];
	int len, offset;

	msg_data[0] = id;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_INFO;
	req.msg.data = msg_data;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf(" Device not present (No Response)\n");
		return -1;
	}
	if (rsp->ccode) {
		printf(" Device not present (%s)\n",
		       val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	fru.size = (rsp->data[1] << 8) | rsp->data[0];
	fru.access = rsp->data[2] & 0x1;

	lprintf(LOG_DEBUG, "fru.size = %d bytes (accessed by %s)",
		fru.size, fru.access ? "words" : "bytes");


	if (fru.size < 1) {
		lprintf(LOG_ERR, " Invalid FRU size %d", fru.size);
		return -1;
	}

        spd_data = malloc(fru.size);

        if (!spd_data) {
		printf(" Unable to malloc memory for spd array of size=%d\n",
		       fru.size);
		return -1;
        }

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_STORAGE;
	req.msg.cmd = GET_FRU_DATA;
	req.msg.data = msg_data;
	req.msg.data_len = 4;

	offset = 0;
	memset(spd_data, 0, fru.size);
	do {
		msg_data[0] = id;
		msg_data[1] = offset & 0xFF;
		msg_data[2] = offset >> 8;
		msg_data[3] = FRU_DATA_RQST_SIZE;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp) {
			printf(" Device not present (No Response)\n");
                        free(spd_data);
                        spd_data = NULL;
			return -1;
		}
		if (rsp->ccode) {
			printf(" Device not present (%s)\n",
			       val2str(rsp->ccode, completion_code_vals));

                        free(spd_data);
                        spd_data = NULL;
			/* Timeouts are acceptable. No DIMM in the socket */
			if (rsp->ccode == 0xc3)
				return 1;

			return -1;
		}

		len = rsp->data[0];
		memcpy(&spd_data[offset], rsp->data + 1, len);
		offset += len;
	} while (offset < fru.size);

	/* now print spd info */
	ipmi_spd_print(spd_data, offset);
        free(spd_data);
        spd_data = NULL;

	return 0;
}