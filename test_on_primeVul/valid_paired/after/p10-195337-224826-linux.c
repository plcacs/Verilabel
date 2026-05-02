static struct sock *pep_sock_accept(struct sock *sk, int flags, int *errp,
				    bool kern)
{
	struct pep_sock *pn = pep_sk(sk), *newpn;
	struct sock *newsk = NULL;
	struct sk_buff *skb;
	struct pnpipehdr *hdr;
	struct sockaddr_pn dst, src;
	int err;
	u16 peer_type;
	u8 pipe_handle, enabled, n_sb;
	u8 aligned = 0;

	skb = skb_recv_datagram(sk, 0, flags & O_NONBLOCK, errp);
	if (!skb)
		return NULL;

	lock_sock(sk);
	if (sk->sk_state != TCP_LISTEN) {
		err = -EINVAL;
		goto drop;
	}
	sk_acceptq_removed(sk);

	err = -EPROTO;
	if (!pskb_may_pull(skb, sizeof(*hdr) + 4))
		goto drop;

	hdr = pnp_hdr(skb);
	pipe_handle = hdr->pipe_handle;
	switch (hdr->state_after_connect) {
	case PN_PIPE_DISABLE:
		enabled = 0;
		break;
	case PN_PIPE_ENABLE:
		enabled = 1;
		break;
	default:
		pep_reject_conn(sk, skb, PN_PIPE_ERR_INVALID_PARAM,
				GFP_KERNEL);
		goto drop;
	}
	peer_type = hdr->other_pep_type << 8;

	/* Parse sub-blocks (options) */
	n_sb = hdr->data[3];
	while (n_sb > 0) {
		u8 type, buf[1], len = sizeof(buf);
		const u8 *data = pep_get_sb(skb, &type, &len, buf);

		if (data == NULL)
			goto drop;
		switch (type) {
		case PN_PIPE_SB_CONNECT_REQ_PEP_SUB_TYPE:
			if (len < 1)
				goto drop;
			peer_type = (peer_type & 0xff00) | data[0];
			break;
		case PN_PIPE_SB_ALIGNED_DATA:
			aligned = data[0] != 0;
			break;
		}
		n_sb--;
	}

	/* Check for duplicate pipe handle */
	newsk = pep_find_pipe(&pn->hlist, &dst, pipe_handle);
	if (unlikely(newsk)) {
		__sock_put(newsk);
		newsk = NULL;
		pep_reject_conn(sk, skb, PN_PIPE_ERR_PEP_IN_USE, GFP_KERNEL);
		goto drop;
	}

	/* Create a new to-be-accepted sock */
	newsk = sk_alloc(sock_net(sk), PF_PHONET, GFP_KERNEL, sk->sk_prot,
			 kern);
	if (!newsk) {
		pep_reject_conn(sk, skb, PN_PIPE_ERR_OVERLOAD, GFP_KERNEL);
		err = -ENOBUFS;
		goto drop;
	}

	sock_init_data(NULL, newsk);
	newsk->sk_state = TCP_SYN_RECV;
	newsk->sk_backlog_rcv = pipe_do_rcv;
	newsk->sk_protocol = sk->sk_protocol;
	newsk->sk_destruct = pipe_destruct;

	newpn = pep_sk(newsk);
	pn_skb_get_dst_sockaddr(skb, &dst);
	pn_skb_get_src_sockaddr(skb, &src);
	newpn->pn_sk.sobject = pn_sockaddr_get_object(&dst);
	newpn->pn_sk.dobject = pn_sockaddr_get_object(&src);
	newpn->pn_sk.resource = pn_sockaddr_get_resource(&dst);
	sock_hold(sk);
	newpn->listener = sk;
	skb_queue_head_init(&newpn->ctrlreq_queue);
	newpn->pipe_handle = pipe_handle;
	atomic_set(&newpn->tx_credits, 0);
	newpn->ifindex = 0;
	newpn->peer_type = peer_type;
	newpn->rx_credits = 0;
	newpn->rx_fc = newpn->tx_fc = PN_LEGACY_FLOW_CONTROL;
	newpn->init_enable = enabled;
	newpn->aligned = aligned;

	err = pep_accept_conn(newsk, skb);
	if (err) {
		__sock_put(sk);
		sock_put(newsk);
		newsk = NULL;
		goto drop;
	}
	sk_add_node(newsk, &pn->hlist);
drop:
	release_sock(sk);
	kfree_skb(skb);
	*errp = err;
	return newsk;
}