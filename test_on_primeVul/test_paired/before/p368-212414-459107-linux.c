static int tc_new_tfilter(struct sk_buff *skb, struct nlmsghdr *n,
			  struct netlink_ext_ack *extack)
{
	struct net *net = sock_net(skb->sk);
	struct nlattr *tca[TCA_MAX + 1];
	char name[IFNAMSIZ];
	struct tcmsg *t;
	u32 protocol;
	u32 prio;
	bool prio_allocate;
	u32 parent;
	u32 chain_index;
	struct Qdisc *q = NULL;
	struct tcf_chain_info chain_info;
	struct tcf_chain *chain = NULL;
	struct tcf_block *block;
	struct tcf_proto *tp;
	unsigned long cl;
	void *fh;
	int err;
	int tp_created;
	bool rtnl_held = false;
	u32 flags;

	if (!netlink_ns_capable(skb, net->user_ns, CAP_NET_ADMIN))
		return -EPERM;

replay:
	tp_created = 0;

	err = nlmsg_parse_deprecated(n, sizeof(*t), tca, TCA_MAX,
				     rtm_tca_policy, extack);
	if (err < 0)
		return err;

	t = nlmsg_data(n);
	protocol = TC_H_MIN(t->tcm_info);
	prio = TC_H_MAJ(t->tcm_info);
	prio_allocate = false;
	parent = t->tcm_parent;
	tp = NULL;
	cl = 0;
	block = NULL;
	flags = 0;

	if (prio == 0) {
		/* If no priority is provided by the user,
		 * we allocate one.
		 */
		if (n->nlmsg_flags & NLM_F_CREATE) {
			prio = TC_H_MAKE(0x80000000U, 0U);
			prio_allocate = true;
		} else {
			NL_SET_ERR_MSG(extack, "Invalid filter command with priority of zero");
			return -ENOENT;
		}
	}

	/* Find head of filter chain. */

	err = __tcf_qdisc_find(net, &q, &parent, t->tcm_ifindex, false, extack);
	if (err)
		return err;

	if (tcf_proto_check_kind(tca[TCA_KIND], name)) {
		NL_SET_ERR_MSG(extack, "Specified TC filter name too long");
		err = -EINVAL;
		goto errout;
	}

	/* Take rtnl mutex if rtnl_held was set to true on previous iteration,
	 * block is shared (no qdisc found), qdisc is not unlocked, classifier
	 * type is not specified, classifier is not unlocked.
	 */
	if (rtnl_held ||
	    (q && !(q->ops->cl_ops->flags & QDISC_CLASS_OPS_DOIT_UNLOCKED)) ||
	    !tcf_proto_is_unlocked(name)) {
		rtnl_held = true;
		rtnl_lock();
	}

	err = __tcf_qdisc_cl_find(q, parent, &cl, t->tcm_ifindex, extack);
	if (err)
		goto errout;

	block = __tcf_block_find(net, q, cl, t->tcm_ifindex, t->tcm_block_index,
				 extack);
	if (IS_ERR(block)) {
		err = PTR_ERR(block);
		goto errout;
	}
	block->classid = parent;

	chain_index = tca[TCA_CHAIN] ? nla_get_u32(tca[TCA_CHAIN]) : 0;
	if (chain_index > TC_ACT_EXT_VAL_MASK) {
		NL_SET_ERR_MSG(extack, "Specified chain index exceeds upper limit");
		err = -EINVAL;
		goto errout;
	}
	chain = tcf_chain_get(block, chain_index, true);
	if (!chain) {
		NL_SET_ERR_MSG(extack, "Cannot create specified filter chain");
		err = -ENOMEM;
		goto errout;
	}

	mutex_lock(&chain->filter_chain_lock);
	tp = tcf_chain_tp_find(chain, &chain_info, protocol,
			       prio, prio_allocate);
	if (IS_ERR(tp)) {
		NL_SET_ERR_MSG(extack, "Filter with specified priority/protocol not found");
		err = PTR_ERR(tp);
		goto errout_locked;
	}

	if (tp == NULL) {
		struct tcf_proto *tp_new = NULL;

		if (chain->flushing) {
			err = -EAGAIN;
			goto errout_locked;
		}

		/* Proto-tcf does not exist, create new one */

		if (tca[TCA_KIND] == NULL || !protocol) {
			NL_SET_ERR_MSG(extack, "Filter kind and protocol must be specified");
			err = -EINVAL;
			goto errout_locked;
		}

		if (!(n->nlmsg_flags & NLM_F_CREATE)) {
			NL_SET_ERR_MSG(extack, "Need both RTM_NEWTFILTER and NLM_F_CREATE to create a new filter");
			err = -ENOENT;
			goto errout_locked;
		}

		if (prio_allocate)
			prio = tcf_auto_prio(tcf_chain_tp_prev(chain,
							       &chain_info));

		mutex_unlock(&chain->filter_chain_lock);
		tp_new = tcf_proto_create(name, protocol, prio, chain,
					  rtnl_held, extack);
		if (IS_ERR(tp_new)) {
			err = PTR_ERR(tp_new);
			goto errout_tp;
		}

		tp_created = 1;
		tp = tcf_chain_tp_insert_unique(chain, tp_new, protocol, prio,
						rtnl_held);
		if (IS_ERR(tp)) {
			err = PTR_ERR(tp);
			goto errout_tp;
		}
	} else {
		mutex_unlock(&chain->filter_chain_lock);
	}

	if (tca[TCA_KIND] && nla_strcmp(tca[TCA_KIND], tp->ops->kind)) {
		NL_SET_ERR_MSG(extack, "Specified filter kind does not match existing one");
		err = -EINVAL;
		goto errout;
	}

	fh = tp->ops->get(tp, t->tcm_handle);

	if (!fh) {
		if (!(n->nlmsg_flags & NLM_F_CREATE)) {
			NL_SET_ERR_MSG(extack, "Need both RTM_NEWTFILTER and NLM_F_CREATE to create a new filter");
			err = -ENOENT;
			goto errout;
		}
	} else if (n->nlmsg_flags & NLM_F_EXCL) {
		tfilter_put(tp, fh);
		NL_SET_ERR_MSG(extack, "Filter already exists");
		err = -EEXIST;
		goto errout;
	}

	if (chain->tmplt_ops && chain->tmplt_ops != tp->ops) {
		NL_SET_ERR_MSG(extack, "Chain template is set to a different filter kind");
		err = -EINVAL;
		goto errout;
	}

	if (!(n->nlmsg_flags & NLM_F_CREATE))
		flags |= TCA_ACT_FLAGS_REPLACE;
	if (!rtnl_held)
		flags |= TCA_ACT_FLAGS_NO_RTNL;
	err = tp->ops->change(net, skb, tp, cl, t->tcm_handle, tca, &fh,
			      flags, extack);
	if (err == 0) {
		tfilter_notify(net, skb, n, tp, block, q, parent, fh,
			       RTM_NEWTFILTER, false, rtnl_held);
		tfilter_put(tp, fh);
		/* q pointer is NULL for shared blocks */
		if (q)
			q->flags &= ~TCQ_F_CAN_BYPASS;
	}

errout:
	if (err && tp_created)
		tcf_chain_tp_delete_empty(chain, tp, rtnl_held, NULL);
errout_tp:
	if (chain) {
		if (tp && !IS_ERR(tp))
			tcf_proto_put(tp, rtnl_held, NULL);
		if (!tp_created)
			tcf_chain_put(chain);
	}
	tcf_block_release(q, block, rtnl_held);

	if (rtnl_held)
		rtnl_unlock();

	if (err == -EAGAIN) {
		/* Take rtnl lock in case EAGAIN is caused by concurrent flush
		 * of target chain.
		 */
		rtnl_held = true;
		/* Replay the request. */
		goto replay;
	}
	return err;

errout_locked:
	mutex_unlock(&chain->filter_chain_lock);
	goto errout;
}