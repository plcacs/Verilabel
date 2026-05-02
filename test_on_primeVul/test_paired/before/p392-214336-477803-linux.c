static int pfkey_register(struct sock *sk, struct sk_buff *skb, const struct sadb_msg *hdr, void * const *ext_hdrs)
{
	struct pfkey_sock *pfk = pfkey_sk(sk);
	struct sk_buff *supp_skb;

	if (hdr->sadb_msg_satype > SADB_SATYPE_MAX)
		return -EINVAL;

	if (hdr->sadb_msg_satype != SADB_SATYPE_UNSPEC) {
		if (pfk->registered&(1<<hdr->sadb_msg_satype))
			return -EEXIST;
		pfk->registered |= (1<<hdr->sadb_msg_satype);
	}

	xfrm_probe_algs();

	supp_skb = compose_sadb_supported(hdr, GFP_KERNEL | __GFP_ZERO);
	if (!supp_skb) {
		if (hdr->sadb_msg_satype != SADB_SATYPE_UNSPEC)
			pfk->registered &= ~(1<<hdr->sadb_msg_satype);

		return -ENOBUFS;
	}

	pfkey_broadcast(supp_skb, GFP_KERNEL, BROADCAST_REGISTERED, sk,
			sock_net(sk));
	return 0;
}