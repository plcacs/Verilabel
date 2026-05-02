static int route4_change(struct net *net, struct sk_buff *in_skb,
			 struct tcf_proto *tp, unsigned long base, u32 handle,
			 struct nlattr **tca, void **arg, bool ovr,
			 bool rtnl_held, struct netlink_ext_ack *extack)
{
	struct route4_head *head = rtnl_dereference(tp->root);
	struct route4_filter __rcu **fp;
	struct route4_filter *fold, *f1, *pfp, *f = NULL;
	struct route4_bucket *b;
	struct nlattr *opt = tca[TCA_OPTIONS];
	struct nlattr *tb[TCA_ROUTE4_MAX + 1];
	unsigned int h, th;
	int err;
	bool new = true;

	if (opt == NULL)
		return handle ? -EINVAL : 0;

	err = nla_parse_nested_deprecated(tb, TCA_ROUTE4_MAX, opt,
					  route4_policy, NULL);
	if (err < 0)
		return err;

	fold = *arg;
	if (fold && handle && fold->handle != handle)
			return -EINVAL;

	err = -ENOBUFS;
	f = kzalloc(sizeof(struct route4_filter), GFP_KERNEL);
	if (!f)
		goto errout;

	err = tcf_exts_init(&f->exts, net, TCA_ROUTE4_ACT, TCA_ROUTE4_POLICE);
	if (err < 0)
		goto errout;

	if (fold) {
		f->id = fold->id;
		f->iif = fold->iif;
		f->res = fold->res;
		f->handle = fold->handle;

		f->tp = fold->tp;
		f->bkt = fold->bkt;
		new = false;
	}

	err = route4_set_parms(net, tp, base, f, handle, head, tb,
			       tca[TCA_RATE], new, ovr, extack);
	if (err < 0)
		goto errout;

	h = from_hash(f->handle >> 16);
	fp = &f->bkt->ht[h];
	for (pfp = rtnl_dereference(*fp);
	     (f1 = rtnl_dereference(*fp)) != NULL;
	     fp = &f1->next)
		if (f->handle < f1->handle)
			break;

	tcf_block_netif_keep_dst(tp->chain->block);
	rcu_assign_pointer(f->next, f1);
	rcu_assign_pointer(*fp, f);

	if (fold && fold->handle && f->handle != fold->handle) {
		th = to_hash(fold->handle);
		h = from_hash(fold->handle >> 16);
		b = rtnl_dereference(head->table[th]);
		if (b) {
			fp = &b->ht[h];
			for (pfp = rtnl_dereference(*fp); pfp;
			     fp = &pfp->next, pfp = rtnl_dereference(*fp)) {
				if (pfp == f) {
					*fp = f->next;
					break;
				}
			}
		}
	}

	route4_reset_fastmap(head);
	*arg = f;
	if (fold) {
		tcf_unbind_filter(tp, &fold->res);
		tcf_exts_get_net(&fold->exts);
		tcf_queue_work(&fold->rwork, route4_delete_filter_work);
	}
	return 0;

errout:
	if (f)
		tcf_exts_destroy(&f->exts);
	kfree(f);
	return err;
}