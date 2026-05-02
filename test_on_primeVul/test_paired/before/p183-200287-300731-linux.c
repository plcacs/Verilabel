static int __tipc_sendmsg(struct socket *sock, struct msghdr *m, size_t dlen)
{
	struct sock *sk = sock->sk;
	struct net *net = sock_net(sk);
	struct tipc_sock *tsk = tipc_sk(sk);
	struct tipc_uaddr *ua = (struct tipc_uaddr *)m->msg_name;
	long timeout = sock_sndtimeo(sk, m->msg_flags & MSG_DONTWAIT);
	struct list_head *clinks = &tsk->cong_links;
	bool syn = !tipc_sk_type_connectionless(sk);
	struct tipc_group *grp = tsk->group;
	struct tipc_msg *hdr = &tsk->phdr;
	struct tipc_socket_addr skaddr;
	struct sk_buff_head pkts;
	int atype, mtu, rc;

	if (unlikely(dlen > TIPC_MAX_USER_MSG_SIZE))
		return -EMSGSIZE;

	if (ua) {
		if (!tipc_uaddr_valid(ua, m->msg_namelen))
			return -EINVAL;
		atype = ua->addrtype;
	}

	/* If socket belongs to a communication group follow other paths */
	if (grp) {
		if (!ua)
			return tipc_send_group_bcast(sock, m, dlen, timeout);
		if (atype == TIPC_SERVICE_ADDR)
			return tipc_send_group_anycast(sock, m, dlen, timeout);
		if (atype == TIPC_SOCKET_ADDR)
			return tipc_send_group_unicast(sock, m, dlen, timeout);
		if (atype == TIPC_SERVICE_RANGE)
			return tipc_send_group_mcast(sock, m, dlen, timeout);
		return -EINVAL;
	}

	if (!ua) {
		ua = (struct tipc_uaddr *)&tsk->peer;
		if (!syn && ua->family != AF_TIPC)
			return -EDESTADDRREQ;
		atype = ua->addrtype;
	}

	if (unlikely(syn)) {
		if (sk->sk_state == TIPC_LISTEN)
			return -EPIPE;
		if (sk->sk_state != TIPC_OPEN)
			return -EISCONN;
		if (tsk->published)
			return -EOPNOTSUPP;
		if (atype == TIPC_SERVICE_ADDR)
			tsk->conn_addrtype = atype;
		msg_set_syn(hdr, 1);
	}

	/* Determine destination */
	if (atype == TIPC_SERVICE_RANGE) {
		return tipc_sendmcast(sock, ua, m, dlen, timeout);
	} else if (atype == TIPC_SERVICE_ADDR) {
		skaddr.node = ua->lookup_node;
		ua->scope = tipc_node2scope(skaddr.node);
		if (!tipc_nametbl_lookup_anycast(net, ua, &skaddr))
			return -EHOSTUNREACH;
	} else if (atype == TIPC_SOCKET_ADDR) {
		skaddr = ua->sk;
	} else {
		return -EINVAL;
	}

	/* Block or return if destination link is congested */
	rc = tipc_wait_for_cond(sock, &timeout,
				!tipc_dest_find(clinks, skaddr.node, 0));
	if (unlikely(rc))
		return rc;

	/* Finally build message header */
	msg_set_destnode(hdr, skaddr.node);
	msg_set_destport(hdr, skaddr.ref);
	if (atype == TIPC_SERVICE_ADDR) {
		msg_set_type(hdr, TIPC_NAMED_MSG);
		msg_set_hdr_sz(hdr, NAMED_H_SIZE);
		msg_set_nametype(hdr, ua->sa.type);
		msg_set_nameinst(hdr, ua->sa.instance);
		msg_set_lookup_scope(hdr, ua->scope);
	} else { /* TIPC_SOCKET_ADDR */
		msg_set_type(hdr, TIPC_DIRECT_MSG);
		msg_set_lookup_scope(hdr, 0);
		msg_set_hdr_sz(hdr, BASIC_H_SIZE);
	}

	/* Add message body */
	__skb_queue_head_init(&pkts);
	mtu = tipc_node_get_mtu(net, skaddr.node, tsk->portid, true);
	rc = tipc_msg_build(hdr, m, 0, dlen, mtu, &pkts);
	if (unlikely(rc != dlen))
		return rc;
	if (unlikely(syn && !tipc_msg_skb_clone(&pkts, &sk->sk_write_queue))) {
		__skb_queue_purge(&pkts);
		return -ENOMEM;
	}

	/* Send message */
	trace_tipc_sk_sendmsg(sk, skb_peek(&pkts), TIPC_DUMP_SK_SNDQ, " ");
	rc = tipc_node_xmit(net, &pkts, skaddr.node, tsk->portid);
	if (unlikely(rc == -ELINKCONG)) {
		tipc_dest_push(clinks, skaddr.node, 0);
		tsk->cong_link_cnt++;
		rc = 0;
	}

	if (unlikely(syn && !rc)) {
		tipc_set_sk_state(sk, TIPC_CONNECTING);
		if (dlen && timeout) {
			timeout = msecs_to_jiffies(timeout);
			tipc_wait_for_connect(sock, &timeout);
		}
	}

	return rc ? rc : dlen;
}