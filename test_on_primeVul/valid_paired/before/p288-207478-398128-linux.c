static void sctp_destroy_sock(struct sock *sk)
{
	struct sctp_sock *sp;

	pr_debug("%s: sk:%p\n", __func__, sk);

	/* Release our hold on the endpoint. */
	sp = sctp_sk(sk);
	/* This could happen during socket init, thus we bail out
	 * early, since the rest of the below is not setup either.
	 */
	if (sp->ep == NULL)
		return;

	if (sp->do_auto_asconf) {
		sp->do_auto_asconf = 0;
		list_del(&sp->auto_asconf_list);
	}
	sctp_endpoint_free(sp->ep);
	local_bh_disable();
	sk_sockets_allocated_dec(sk);
	sock_prot_inuse_add(sock_net(sk), sk->sk_prot, -1);
	local_bh_enable();
}