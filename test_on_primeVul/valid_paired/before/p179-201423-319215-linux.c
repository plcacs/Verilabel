static int rawsock_create(struct net *net, struct socket *sock,
			  const struct nfc_protocol *nfc_proto, int kern)
{
	struct sock *sk;

	pr_debug("sock=%p\n", sock);

	if ((sock->type != SOCK_SEQPACKET) && (sock->type != SOCK_RAW))
		return -ESOCKTNOSUPPORT;

	if (sock->type == SOCK_RAW)
		sock->ops = &rawsock_raw_ops;
	else
		sock->ops = &rawsock_ops;

	sk = sk_alloc(net, PF_NFC, GFP_ATOMIC, nfc_proto->proto, kern);
	if (!sk)
		return -ENOMEM;

	sock_init_data(sock, sk);
	sk->sk_protocol = nfc_proto->id;
	sk->sk_destruct = rawsock_destruct;
	sock->state = SS_UNCONNECTED;
	if (sock->type == SOCK_RAW)
		nfc_sock_link(&raw_sk_list, sk);
	else {
		INIT_WORK(&nfc_rawsock(sk)->tx_work, rawsock_tx_work);
		nfc_rawsock(sk)->tx_work_scheduled = false;
	}

	return 0;
}