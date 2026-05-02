ipmi_get_session_info(struct ipmi_intf         * intf,
					  Ipmi_Session_Request_Type  session_request_type,
					  uint32_t                   id_or_handle)
{
	int i, retval = 0;

	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[5]; //  max length of the variable length request
	struct get_session_info_rsp   session_info;

	memset(&req, 0, sizeof(req));
	memset(&session_info, 0, sizeof(session_info));
	req.msg.netfn = IPMI_NETFN_APP;        // 0x06
	req.msg.cmd   = IPMI_GET_SESSION_INFO; // 0x3D
	req.msg.data = rqdata;

	switch (session_request_type)
	{
		
	case IPMI_SESSION_REQUEST_CURRENT:
	case IPMI_SESSION_REQUEST_BY_ID:	
	case IPMI_SESSION_REQUEST_BY_HANDLE:
		switch (session_request_type)
		{
		case IPMI_SESSION_REQUEST_CURRENT:
			rqdata[0]        = 0x00;
			req.msg.data_len = 1;
			break;
		case IPMI_SESSION_REQUEST_BY_ID:	
			rqdata[0]        = 0xFF;
			rqdata[1]        = id_or_handle         & 0x000000FF;
			rqdata[2]        = (id_or_handle >> 8)  & 0x000000FF;
			rqdata[3]        = (id_or_handle >> 16) & 0x000000FF;
			rqdata[4]        = (id_or_handle >> 24) & 0x000000FF;
			req.msg.data_len = 5;
			break;
		case IPMI_SESSION_REQUEST_BY_HANDLE:
			rqdata[0]        = 0xFE;
			rqdata[1]        = (uint8_t)id_or_handle;
			req.msg.data_len = 2;
			break;
		case IPMI_SESSION_REQUEST_ALL:
			break;
		}

		rsp = intf->sendrecv(intf, &req);
		if (!rsp)
		{
			lprintf(LOG_ERR, "Get Session Info command failed");
			retval = -1;
		}
		else if (rsp->ccode)
		{
			lprintf(LOG_ERR, "Get Session Info command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
			retval = -1;
		}

		if (retval < 0)
		{
			if ((session_request_type == IPMI_SESSION_REQUEST_CURRENT) &&
			    (strncmp(intf->name, "lan", 3) != 0))
				lprintf(LOG_ERR, "It is likely that the channel in use "
					"does not support sessions");
		}
		else
		{
			memcpy(&session_info,  rsp->data, rsp->data_len);
			print_session_info(&session_info, rsp->data_len);
		}
		break;
		
	case IPMI_SESSION_REQUEST_ALL:
		req.msg.data_len = 1;
		i = 1;
		do
		{
			rqdata[0] = i++;
			rsp = intf->sendrecv(intf, &req);
			
			if (!rsp)
			{
				lprintf(LOG_ERR, "Get Session Info command failed");
				retval = -1;
				break;
			}
			else if (rsp->ccode && rsp->ccode != 0xCC && rsp->ccode != 0xCB)
			{
				lprintf(LOG_ERR, "Get Session Info command failed: %s",
					val2str(rsp->ccode, completion_code_vals));
				retval = -1;
				break;
			}
			else if (rsp->data_len < 3)
			{
				retval = -1;
				break;
			}

			memcpy(&session_info,  rsp->data, rsp->data_len);
			print_session_info(&session_info, rsp->data_len);
			
		} while (i <= session_info.session_slot_count);
		break;
	}

	return retval;
}