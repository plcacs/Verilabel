static int nf_tables_newset(struct sk_buff *skb, const struct nfnl_info *info,
			    const struct nlattr * const nla[])
{
	const struct nfgenmsg *nfmsg = nlmsg_data(info->nlh);
	u32 ktype, dtype, flags, policy, gc_int, objtype;
	struct netlink_ext_ack *extack = info->extack;
	u8 genmask = nft_genmask_next(info->net);
	int family = nfmsg->nfgen_family;
	const struct nft_set_ops *ops;
	struct nft_expr *expr = NULL;
	struct net *net = info->net;
	struct nft_set_desc desc;
	struct nft_table *table;
	unsigned char *udata;
	struct nft_set *set;
	struct nft_ctx ctx;
	size_t alloc_size;
	u64 timeout;
	char *name;
	int err, i;
	u16 udlen;
	u64 size;

	if (nla[NFTA_SET_TABLE] == NULL ||
	    nla[NFTA_SET_NAME] == NULL ||
	    nla[NFTA_SET_KEY_LEN] == NULL ||
	    nla[NFTA_SET_ID] == NULL)
		return -EINVAL;

	memset(&desc, 0, sizeof(desc));

	ktype = NFT_DATA_VALUE;
	if (nla[NFTA_SET_KEY_TYPE] != NULL) {
		ktype = ntohl(nla_get_be32(nla[NFTA_SET_KEY_TYPE]));
		if ((ktype & NFT_DATA_RESERVED_MASK) == NFT_DATA_RESERVED_MASK)
			return -EINVAL;
	}

	desc.klen = ntohl(nla_get_be32(nla[NFTA_SET_KEY_LEN]));
	if (desc.klen == 0 || desc.klen > NFT_DATA_VALUE_MAXLEN)
		return -EINVAL;

	flags = 0;
	if (nla[NFTA_SET_FLAGS] != NULL) {
		flags = ntohl(nla_get_be32(nla[NFTA_SET_FLAGS]));
		if (flags & ~(NFT_SET_ANONYMOUS | NFT_SET_CONSTANT |
			      NFT_SET_INTERVAL | NFT_SET_TIMEOUT |
			      NFT_SET_MAP | NFT_SET_EVAL |
			      NFT_SET_OBJECT | NFT_SET_CONCAT | NFT_SET_EXPR))
			return -EOPNOTSUPP;
		/* Only one of these operations is supported */
		if ((flags & (NFT_SET_MAP | NFT_SET_OBJECT)) ==
			     (NFT_SET_MAP | NFT_SET_OBJECT))
			return -EOPNOTSUPP;
		if ((flags & (NFT_SET_EVAL | NFT_SET_OBJECT)) ==
			     (NFT_SET_EVAL | NFT_SET_OBJECT))
			return -EOPNOTSUPP;
	}

	dtype = 0;
	if (nla[NFTA_SET_DATA_TYPE] != NULL) {
		if (!(flags & NFT_SET_MAP))
			return -EINVAL;

		dtype = ntohl(nla_get_be32(nla[NFTA_SET_DATA_TYPE]));
		if ((dtype & NFT_DATA_RESERVED_MASK) == NFT_DATA_RESERVED_MASK &&
		    dtype != NFT_DATA_VERDICT)
			return -EINVAL;

		if (dtype != NFT_DATA_VERDICT) {
			if (nla[NFTA_SET_DATA_LEN] == NULL)
				return -EINVAL;
			desc.dlen = ntohl(nla_get_be32(nla[NFTA_SET_DATA_LEN]));
			if (desc.dlen == 0 || desc.dlen > NFT_DATA_VALUE_MAXLEN)
				return -EINVAL;
		} else
			desc.dlen = sizeof(struct nft_verdict);
	} else if (flags & NFT_SET_MAP)
		return -EINVAL;

	if (nla[NFTA_SET_OBJ_TYPE] != NULL) {
		if (!(flags & NFT_SET_OBJECT))
			return -EINVAL;

		objtype = ntohl(nla_get_be32(nla[NFTA_SET_OBJ_TYPE]));
		if (objtype == NFT_OBJECT_UNSPEC ||
		    objtype > NFT_OBJECT_MAX)
			return -EOPNOTSUPP;
	} else if (flags & NFT_SET_OBJECT)
		return -EINVAL;
	else
		objtype = NFT_OBJECT_UNSPEC;

	timeout = 0;
	if (nla[NFTA_SET_TIMEOUT] != NULL) {
		if (!(flags & NFT_SET_TIMEOUT))
			return -EINVAL;

		err = nf_msecs_to_jiffies64(nla[NFTA_SET_TIMEOUT], &timeout);
		if (err)
			return err;
	}
	gc_int = 0;
	if (nla[NFTA_SET_GC_INTERVAL] != NULL) {
		if (!(flags & NFT_SET_TIMEOUT))
			return -EINVAL;
		gc_int = ntohl(nla_get_be32(nla[NFTA_SET_GC_INTERVAL]));
	}

	policy = NFT_SET_POL_PERFORMANCE;
	if (nla[NFTA_SET_POLICY] != NULL)
		policy = ntohl(nla_get_be32(nla[NFTA_SET_POLICY]));

	if (nla[NFTA_SET_DESC] != NULL) {
		err = nf_tables_set_desc_parse(&desc, nla[NFTA_SET_DESC]);
		if (err < 0)
			return err;
	}

	if (nla[NFTA_SET_EXPR] || nla[NFTA_SET_EXPRESSIONS])
		desc.expr = true;

	table = nft_table_lookup(net, nla[NFTA_SET_TABLE], family, genmask,
				 NETLINK_CB(skb).portid);
	if (IS_ERR(table)) {
		NL_SET_BAD_ATTR(extack, nla[NFTA_SET_TABLE]);
		return PTR_ERR(table);
	}

	nft_ctx_init(&ctx, net, skb, info->nlh, family, table, NULL, nla);

	set = nft_set_lookup(table, nla[NFTA_SET_NAME], genmask);
	if (IS_ERR(set)) {
		if (PTR_ERR(set) != -ENOENT) {
			NL_SET_BAD_ATTR(extack, nla[NFTA_SET_NAME]);
			return PTR_ERR(set);
		}
	} else {
		if (info->nlh->nlmsg_flags & NLM_F_EXCL) {
			NL_SET_BAD_ATTR(extack, nla[NFTA_SET_NAME]);
			return -EEXIST;
		}
		if (info->nlh->nlmsg_flags & NLM_F_REPLACE)
			return -EOPNOTSUPP;

		return 0;
	}

	if (!(info->nlh->nlmsg_flags & NLM_F_CREATE))
		return -ENOENT;

	ops = nft_select_set_ops(&ctx, nla, &desc, policy);
	if (IS_ERR(ops))
		return PTR_ERR(ops);

	udlen = 0;
	if (nla[NFTA_SET_USERDATA])
		udlen = nla_len(nla[NFTA_SET_USERDATA]);

	size = 0;
	if (ops->privsize != NULL)
		size = ops->privsize(nla, &desc);
	alloc_size = sizeof(*set) + size + udlen;
	if (alloc_size < size)
		return -ENOMEM;
	set = kvzalloc(alloc_size, GFP_KERNEL);
	if (!set)
		return -ENOMEM;

	name = nla_strdup(nla[NFTA_SET_NAME], GFP_KERNEL);
	if (!name) {
		err = -ENOMEM;
		goto err_set_name;
	}

	err = nf_tables_set_alloc_name(&ctx, set, name);
	kfree(name);
	if (err < 0)
		goto err_set_alloc_name;

	if (nla[NFTA_SET_EXPR]) {
		expr = nft_set_elem_expr_alloc(&ctx, set, nla[NFTA_SET_EXPR]);
		if (IS_ERR(expr)) {
			err = PTR_ERR(expr);
			goto err_set_alloc_name;
		}
		set->exprs[0] = expr;
		set->num_exprs++;
	} else if (nla[NFTA_SET_EXPRESSIONS]) {
		struct nft_expr *expr;
		struct nlattr *tmp;
		int left;

		if (!(flags & NFT_SET_EXPR)) {
			err = -EINVAL;
			goto err_set_alloc_name;
		}
		i = 0;
		nla_for_each_nested(tmp, nla[NFTA_SET_EXPRESSIONS], left) {
			if (i == NFT_SET_EXPR_MAX) {
				err = -E2BIG;
				goto err_set_init;
			}
			if (nla_type(tmp) != NFTA_LIST_ELEM) {
				err = -EINVAL;
				goto err_set_init;
			}
			expr = nft_set_elem_expr_alloc(&ctx, set, tmp);
			if (IS_ERR(expr)) {
				err = PTR_ERR(expr);
				goto err_set_init;
			}
			set->exprs[i++] = expr;
			set->num_exprs++;
		}
	}

	udata = NULL;
	if (udlen) {
		udata = set->data + size;
		nla_memcpy(udata, nla[NFTA_SET_USERDATA], udlen);
	}

	INIT_LIST_HEAD(&set->bindings);
	INIT_LIST_HEAD(&set->catchall_list);
	set->table = table;
	write_pnet(&set->net, net);
	set->ops   = ops;
	set->ktype = ktype;
	set->klen  = desc.klen;
	set->dtype = dtype;
	set->objtype = objtype;
	set->dlen  = desc.dlen;
	set->flags = flags;
	set->size  = desc.size;
	set->policy = policy;
	set->udlen  = udlen;
	set->udata  = udata;
	set->timeout = timeout;
	set->gc_int = gc_int;
	set->handle = nf_tables_alloc_handle(table);

	set->field_count = desc.field_count;
	for (i = 0; i < desc.field_count; i++)
		set->field_len[i] = desc.field_len[i];

	err = ops->init(set, &desc, nla);
	if (err < 0)
		goto err_set_init;

	err = nft_trans_set_add(&ctx, NFT_MSG_NEWSET, set);
	if (err < 0)
		goto err_set_trans;

	list_add_tail_rcu(&set->list, &table->sets);
	table->use++;
	return 0;

err_set_trans:
	ops->destroy(set);
err_set_init:
	for (i = 0; i < set->num_exprs; i++)
		nft_expr_destroy(&ctx, set->exprs[i]);
err_set_alloc_name:
	kfree(set->name);
err_set_name:
	kvfree(set);
	return err;
}