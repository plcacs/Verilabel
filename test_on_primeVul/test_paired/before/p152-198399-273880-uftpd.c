static void handle_PORT(ctrl_t *ctrl, char *str)
{
	int a, b, c, d, e, f;
	char addr[INET_ADDRSTRLEN];
	struct sockaddr_in sin;

	if (ctrl->data_sd > 0) {
		uev_io_stop(&ctrl->data_watcher);
		close(ctrl->data_sd);
		ctrl->data_sd = -1;
	}

	/* Convert PORT command's argument to IP address + port */
	sscanf(str, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &e, &f);
	sprintf(addr, "%d.%d.%d.%d", a, b, c, d);

	/* Check IPv4 address using inet_aton(), throw away converted result */
	if (!inet_aton(addr, &(sin.sin_addr))) {
		ERR(0, "Invalid address '%s' given to PORT command", addr);
		send_msg(ctrl->sd, "500 Illegal PORT command.\r\n");
		return;
	}

	strlcpy(ctrl->data_address, addr, sizeof(ctrl->data_address));
	ctrl->data_port = e * 256 + f;

	DBG("Client PORT command accepted for %s:%d", ctrl->data_address, ctrl->data_port);
	send_msg(ctrl->sd, "200 PORT command successful.\r\n");
}