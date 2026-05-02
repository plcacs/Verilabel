ipmi_lan_alert_set(struct ipmi_intf * intf, uint8_t chan, uint8_t alert,
		   int argc, char ** argv)
{
	struct lan_param * p;
	uint8_t data[32], temp[32];
	int rc = 0;

	if (argc < 2) {
		print_lan_alert_set_usage();
		return (-1);
	}

	if (strncmp(argv[0], "help", 4) == 0 ||
	    strncmp(argv[1], "help", 4) == 0) {
		print_lan_alert_set_usage();
		return 0;
	}

	memset(data, 0, sizeof(data));
	memset(temp, 0, sizeof(temp));

	/* alert destination ip address */
	if (strncasecmp(argv[0], "ipaddr", 6) == 0 &&
	    (get_cmdline_ipaddr(argv[1], temp) == 0)) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_ADDR, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);
		/* set new ipaddr */
		memcpy(data+3, temp, 4);
		printf("Setting LAN Alert %d IP Address to %d.%d.%d.%d\n", alert,
		       data[3], data[4], data[5], data[6]);
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_ADDR, data, p->data_len);
	}
	/* alert destination mac address */
	else if (strncasecmp(argv[0], "macaddr", 7) == 0 &&
		 (str2mac(argv[1], temp) == 0)) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_ADDR, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);
		/* set new macaddr */
		memcpy(data+7, temp, 6);
		printf("Setting LAN Alert %d MAC Address to "
		       "%s\n", alert, mac2str(&data[7]));
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_ADDR, data, p->data_len);
	}
	/* alert destination gateway selector */
	else if (strncasecmp(argv[0], "gateway", 7) == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_ADDR, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);

		if (strncasecmp(argv[1], "def", 3) == 0 ||
		    strncasecmp(argv[1], "default", 7) == 0) {
			printf("Setting LAN Alert %d to use Default Gateway\n", alert);
			data[2] = 0;
		}
		else if (strncasecmp(argv[1], "bak", 3) == 0 ||
			 strncasecmp(argv[1], "backup", 6) == 0) {
			printf("Setting LAN Alert %d to use Backup Gateway\n", alert);
			data[2] = 1;
		}
		else {
			print_lan_alert_set_usage();
			return -1;
		}

		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_ADDR, data, p->data_len);
	}
	/* alert acknowledgement */
	else if (strncasecmp(argv[0], "ack", 3) == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);

		if (strncasecmp(argv[1], "on", 2) == 0 ||
		    strncasecmp(argv[1], "yes", 3) == 0) {
			printf("Setting LAN Alert %d to Acknowledged\n", alert);
			data[1] |= 0x80;
		}
		else if (strncasecmp(argv[1], "off", 3) == 0 ||
			 strncasecmp(argv[1], "no", 2) == 0) {
			printf("Setting LAN Alert %d to Unacknowledged\n", alert);
			data[1] &= ~0x80;
		}
		else {
			print_lan_alert_set_usage();
			return -1;
		}
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	/* alert destination type */
	else if (strncasecmp(argv[0], "type", 4) == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);

		if (strncasecmp(argv[1], "pet", 3) == 0) {
			printf("Setting LAN Alert %d destination to PET Trap\n", alert);
			data[1] &= ~0x07;
		}
		else if (strncasecmp(argv[1], "oem1", 4) == 0) {
			printf("Setting LAN Alert %d destination to OEM 1\n", alert);
			data[1] &= ~0x07;
			data[1] |= 0x06;
		}
		else if (strncasecmp(argv[1], "oem2", 4) == 0) {
			printf("Setting LAN Alert %d destination to OEM 2\n", alert);
			data[1] |= 0x07;
		}
		else {
			print_lan_alert_set_usage();
			return -1;
		}
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	/* alert acknowledge timeout or retry interval */
	else if (strncasecmp(argv[0], "time", 4) == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);

		if (str2uchar(argv[1], &data[2]) != 0) {
			lprintf(LOG_ERR, "Invalid time: %s", argv[1]);
			return (-1);
		}
		printf("Setting LAN Alert %d timeout/retry to %d seconds\n", alert, data[2]);
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	/* number of retries */
	else if (strncasecmp(argv[0], "retry", 5) == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, p->data_len);

		if (str2uchar(argv[1], &data[3]) != 0) {
			lprintf(LOG_ERR, "Invalid retry: %s", argv[1]);
			return (-1);
		}
		data[3] = data[3] & 0x7;
		printf("Setting LAN Alert %d number of retries to %d\n", alert, data[3]);
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	else {
		print_lan_alert_set_usage();
		return -1;
	}

	return rc;
}