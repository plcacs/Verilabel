static int nf_conntrack_standalone_init_sysctl(struct net *net)
{
	struct nf_conntrack_net *cnet = net_generic(net, nf_conntrack_net_id);
	struct nf_udp_net *un = nf_udp_pernet(net);
	struct ctl_table *table;

	BUILD_BUG_ON(ARRAY_SIZE(nf_ct_sysctl_table) != NF_SYSCTL_CT_LAST_SYSCTL);

	table = kmemdup(nf_ct_sysctl_table, sizeof(nf_ct_sysctl_table),
			GFP_KERNEL);
	if (!table)
		return -ENOMEM;

	table[NF_SYSCTL_CT_COUNT].data = &net->ct.count;
	table[NF_SYSCTL_CT_CHECKSUM].data = &net->ct.sysctl_checksum;
	table[NF_SYSCTL_CT_LOG_INVALID].data = &net->ct.sysctl_log_invalid;
	table[NF_SYSCTL_CT_ACCT].data = &net->ct.sysctl_acct;
	table[NF_SYSCTL_CT_HELPER].data = &net->ct.sysctl_auto_assign_helper;
#ifdef CONFIG_NF_CONNTRACK_EVENTS
	table[NF_SYSCTL_CT_EVENTS].data = &net->ct.sysctl_events;
#endif
#ifdef CONFIG_NF_CONNTRACK_TIMESTAMP
	table[NF_SYSCTL_CT_TIMESTAMP].data = &net->ct.sysctl_tstamp;
#endif
	table[NF_SYSCTL_CT_PROTO_TIMEOUT_GENERIC].data = &nf_generic_pernet(net)->timeout;
	table[NF_SYSCTL_CT_PROTO_TIMEOUT_ICMP].data = &nf_icmp_pernet(net)->timeout;
	table[NF_SYSCTL_CT_PROTO_TIMEOUT_ICMPV6].data = &nf_icmpv6_pernet(net)->timeout;
	table[NF_SYSCTL_CT_PROTO_TIMEOUT_UDP].data = &un->timeouts[UDP_CT_UNREPLIED];
	table[NF_SYSCTL_CT_PROTO_TIMEOUT_UDP_STREAM].data = &un->timeouts[UDP_CT_REPLIED];

	nf_conntrack_standalone_init_tcp_sysctl(net, table);
	nf_conntrack_standalone_init_sctp_sysctl(net, table);
	nf_conntrack_standalone_init_dccp_sysctl(net, table);
	nf_conntrack_standalone_init_gre_sysctl(net, table);

	/* Don't allow unprivileged users to alter certain sysctls */
	if (net->user_ns != &init_user_ns) {
		table[NF_SYSCTL_CT_MAX].mode = 0444;
		table[NF_SYSCTL_CT_EXPECT_MAX].mode = 0444;
		table[NF_SYSCTL_CT_HELPER].mode = 0444;
#ifdef CONFIG_NF_CONNTRACK_EVENTS
		table[NF_SYSCTL_CT_EVENTS].mode = 0444;
#endif
		table[NF_SYSCTL_CT_BUCKETS].mode = 0444;
	} else if (!net_eq(&init_net, net)) {
		table[NF_SYSCTL_CT_BUCKETS].mode = 0444;
	}

	cnet->sysctl_header = register_net_sysctl(net, "net/netfilter", table);
	if (!cnet->sysctl_header)
		goto out_unregister_netfilter;

	return 0;

out_unregister_netfilter:
	kfree(table);
	return -ENOMEM;
}