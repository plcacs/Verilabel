nfqnl_build_packet_message(struct net *net, struct nfqnl_instance *queue,
			   struct nf_queue_entry *entry,
			   __be32 **packet_id_ptr)
{
	size_t size;
	size_t data_len = 0, cap_len = 0;
	unsigned int hlen = 0;
	struct sk_buff *skb;
	struct nlattr *nla;
	struct nfqnl_msg_packet_hdr *pmsg;
	struct nlmsghdr *nlh;
	struct nfgenmsg *nfmsg;
	struct sk_buff *entskb = entry->skb;
	struct net_device *indev;
	struct net_device *outdev;
	struct nf_conn *ct = NULL;
	enum ip_conntrack_info uninitialized_var(ctinfo);
	bool csum_verify;

	size =    nlmsg_total_size(sizeof(struct nfgenmsg))
		+ nla_total_size(sizeof(struct nfqnl_msg_packet_hdr))
		+ nla_total_size(sizeof(u_int32_t))	/* ifindex */
		+ nla_total_size(sizeof(u_int32_t))	/* ifindex */
#ifdef CONFIG_BRIDGE_NETFILTER
		+ nla_total_size(sizeof(u_int32_t))	/* ifindex */
		+ nla_total_size(sizeof(u_int32_t))	/* ifindex */
#endif
		+ nla_total_size(sizeof(u_int32_t))	/* mark */
		+ nla_total_size(sizeof(struct nfqnl_msg_packet_hw))
		+ nla_total_size(sizeof(u_int32_t))	/* skbinfo */
		+ nla_total_size(sizeof(u_int32_t));	/* cap_len */

	if (entskb->tstamp.tv64)
		size += nla_total_size(sizeof(struct nfqnl_msg_packet_timestamp));

	if (entry->hook <= NF_INET_FORWARD ||
	   (entry->hook == NF_INET_POST_ROUTING && entskb->sk == NULL))
		csum_verify = !skb_csum_unnecessary(entskb);
	else
		csum_verify = false;

	outdev = entry->outdev;

	switch ((enum nfqnl_config_mode)ACCESS_ONCE(queue->copy_mode)) {
	case NFQNL_COPY_META:
	case NFQNL_COPY_NONE:
		break;

	case NFQNL_COPY_PACKET:
		if (!(queue->flags & NFQA_CFG_F_GSO) &&
		    entskb->ip_summed == CHECKSUM_PARTIAL &&
		    skb_checksum_help(entskb))
			return NULL;

		data_len = ACCESS_ONCE(queue->copy_range);
		if (data_len > entskb->len)
			data_len = entskb->len;

		hlen = skb_zerocopy_headlen(entskb);
		hlen = min_t(unsigned int, hlen, data_len);
		size += sizeof(struct nlattr) + hlen;
		cap_len = entskb->len;
		break;
	}

	if (queue->flags & NFQA_CFG_F_CONNTRACK)
		ct = nfqnl_ct_get(entskb, &size, &ctinfo);

	if (queue->flags & NFQA_CFG_F_UID_GID) {
		size +=  (nla_total_size(sizeof(u_int32_t))	/* uid */
			+ nla_total_size(sizeof(u_int32_t)));	/* gid */
	}

	skb = nfnetlink_alloc_skb(net, size, queue->peer_portid,
				  GFP_ATOMIC);
	if (!skb)
		return NULL;

	nlh = nlmsg_put(skb, 0, 0,
			NFNL_SUBSYS_QUEUE << 8 | NFQNL_MSG_PACKET,
			sizeof(struct nfgenmsg), 0);
	if (!nlh) {
		kfree_skb(skb);
		return NULL;
	}
	nfmsg = nlmsg_data(nlh);
	nfmsg->nfgen_family = entry->pf;
	nfmsg->version = NFNETLINK_V0;
	nfmsg->res_id = htons(queue->queue_num);

	nla = __nla_reserve(skb, NFQA_PACKET_HDR, sizeof(*pmsg));
	pmsg = nla_data(nla);
	pmsg->hw_protocol	= entskb->protocol;
	pmsg->hook		= entry->hook;
	*packet_id_ptr		= &pmsg->packet_id;

	indev = entry->indev;
	if (indev) {
#ifndef CONFIG_BRIDGE_NETFILTER
		if (nla_put_be32(skb, NFQA_IFINDEX_INDEV, htonl(indev->ifindex)))
			goto nla_put_failure;
#else
		if (entry->pf == PF_BRIDGE) {
			/* Case 1: indev is physical input device, we need to
			 * look for bridge group (when called from
			 * netfilter_bridge) */
			if (nla_put_be32(skb, NFQA_IFINDEX_PHYSINDEV,
					 htonl(indev->ifindex)) ||
			/* this is the bridge group "brX" */
			/* rcu_read_lock()ed by __nf_queue */
			    nla_put_be32(skb, NFQA_IFINDEX_INDEV,
					 htonl(br_port_get_rcu(indev)->br->dev->ifindex)))
				goto nla_put_failure;
		} else {
			/* Case 2: indev is bridge group, we need to look for
			 * physical device (when called from ipv4) */
			if (nla_put_be32(skb, NFQA_IFINDEX_INDEV,
					 htonl(indev->ifindex)))
				goto nla_put_failure;
			if (entskb->nf_bridge && entskb->nf_bridge->physindev &&
			    nla_put_be32(skb, NFQA_IFINDEX_PHYSINDEV,
					 htonl(entskb->nf_bridge->physindev->ifindex)))
				goto nla_put_failure;
		}
#endif
	}

	if (outdev) {
#ifndef CONFIG_BRIDGE_NETFILTER
		if (nla_put_be32(skb, NFQA_IFINDEX_OUTDEV, htonl(outdev->ifindex)))
			goto nla_put_failure;
#else
		if (entry->pf == PF_BRIDGE) {
			/* Case 1: outdev is physical output device, we need to
			 * look for bridge group (when called from
			 * netfilter_bridge) */
			if (nla_put_be32(skb, NFQA_IFINDEX_PHYSOUTDEV,
					 htonl(outdev->ifindex)) ||
			/* this is the bridge group "brX" */
			/* rcu_read_lock()ed by __nf_queue */
			    nla_put_be32(skb, NFQA_IFINDEX_OUTDEV,
					 htonl(br_port_get_rcu(outdev)->br->dev->ifindex)))
				goto nla_put_failure;
		} else {
			/* Case 2: outdev is bridge group, we need to look for
			 * physical output device (when called from ipv4) */
			if (nla_put_be32(skb, NFQA_IFINDEX_OUTDEV,
					 htonl(outdev->ifindex)))
				goto nla_put_failure;
			if (entskb->nf_bridge && entskb->nf_bridge->physoutdev &&
			    nla_put_be32(skb, NFQA_IFINDEX_PHYSOUTDEV,
					 htonl(entskb->nf_bridge->physoutdev->ifindex)))
				goto nla_put_failure;
		}
#endif
	}

	if (entskb->mark &&
	    nla_put_be32(skb, NFQA_MARK, htonl(entskb->mark)))
		goto nla_put_failure;

	if (indev && entskb->dev &&
	    entskb->mac_header != entskb->network_header) {
		struct nfqnl_msg_packet_hw phw;
		int len;

		memset(&phw, 0, sizeof(phw));
		len = dev_parse_header(entskb, phw.hw_addr);
		if (len) {
			phw.hw_addrlen = htons(len);
			if (nla_put(skb, NFQA_HWADDR, sizeof(phw), &phw))
				goto nla_put_failure;
		}
	}

	if (entskb->tstamp.tv64) {
		struct nfqnl_msg_packet_timestamp ts;
		struct timeval tv = ktime_to_timeval(entskb->tstamp);
		ts.sec = cpu_to_be64(tv.tv_sec);
		ts.usec = cpu_to_be64(tv.tv_usec);

		if (nla_put(skb, NFQA_TIMESTAMP, sizeof(ts), &ts))
			goto nla_put_failure;
	}

	if ((queue->flags & NFQA_CFG_F_UID_GID) && entskb->sk &&
	    nfqnl_put_sk_uidgid(skb, entskb->sk) < 0)
		goto nla_put_failure;

	if (ct && nfqnl_ct_put(skb, ct, ctinfo) < 0)
		goto nla_put_failure;

	if (cap_len > data_len &&
	    nla_put_be32(skb, NFQA_CAP_LEN, htonl(cap_len)))
		goto nla_put_failure;

	if (nfqnl_put_packet_info(skb, entskb, csum_verify))
		goto nla_put_failure;

	if (data_len) {
		struct nlattr *nla;

		if (skb_tailroom(skb) < sizeof(*nla) + hlen)
			goto nla_put_failure;

		nla = (struct nlattr *)skb_put(skb, sizeof(*nla));
		nla->nla_type = NFQA_PAYLOAD;
		nla->nla_len = nla_attr_size(data_len);

		skb_zerocopy(skb, entskb, data_len, hlen);
	}

	nlh->nlmsg_len = skb->len;
	return skb;

nla_put_failure:
	kfree_skb(skb);
	net_err_ratelimited("nf_queue: error creating packet message\n");
	return NULL;
}