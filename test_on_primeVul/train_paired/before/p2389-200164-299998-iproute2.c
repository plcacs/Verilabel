int get_netnsid_from_name(const char *name)
{
	struct {
		struct nlmsghdr n;
		struct rtgenmsg g;
		char            buf[1024];
	} req = {
		.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtgenmsg)),
		.n.nlmsg_flags = NLM_F_REQUEST,
		.n.nlmsg_type = RTM_GETNSID,
		.g.rtgen_family = AF_UNSPEC,
	};
	struct nlmsghdr *answer;
	struct rtattr *tb[NETNSA_MAX + 1];
	struct rtgenmsg *rthdr;
	int len, fd;

	netns_nsid_socket_init();

	fd = netns_get_fd(name);
	if (fd < 0)
		return fd;

	addattr32(&req.n, 1024, NETNSA_FD, fd);
	if (rtnl_talk(&rtnsh, &req.n, &answer) < 0) {
		close(fd);
		return -2;
	}
	close(fd);

	/* Validate message and parse attributes */
	if (answer->nlmsg_type == NLMSG_ERROR)
		goto err_out;

	rthdr = NLMSG_DATA(answer);
	len = answer->nlmsg_len - NLMSG_SPACE(sizeof(*rthdr));
	if (len < 0)
		goto err_out;

	parse_rtattr(tb, NETNSA_MAX, NETNS_RTA(rthdr), len);

	if (tb[NETNSA_NSID]) {
		free(answer);
		return rta_getattr_u32(tb[NETNSA_NSID]);
	}

err_out:
	free(answer);
	return -1;
}