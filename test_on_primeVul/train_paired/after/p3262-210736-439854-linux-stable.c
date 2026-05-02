static void ip6_rt_update_pmtu(struct dst_entry *dst, struct sock *sk,
			       struct sk_buff *skb, u32 mtu)
{
	struct rt6_info *rt6 = (struct rt6_info *)dst;

	dst_confirm(dst);
	if (mtu < dst_mtu(dst) && rt6->rt6i_dst.plen == 128) {
		struct net *net = dev_net(dst->dev);

		rt6->rt6i_flags |= RTF_MODIFIED;
		if (mtu < IPV6_MIN_MTU)
			mtu = IPV6_MIN_MTU;

		dst_metric_set(dst, RTAX_MTU, mtu);
		rt6_update_expires(rt6, net->ipv6.sysctl.ip6_rt_mtu_expires);
	}
}