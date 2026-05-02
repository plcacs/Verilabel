struct nft_flow_rule *nft_flow_rule_create(struct net *net,
					   const struct nft_rule *rule)
{
	struct nft_offload_ctx *ctx;
	struct nft_flow_rule *flow;
	int num_actions = 0, err;
	struct nft_expr *expr;

	expr = nft_expr_first(rule);
	while (nft_expr_more(rule, expr)) {
		if (expr->ops->offload_flags & NFT_OFFLOAD_F_ACTION)
			num_actions++;

		expr = nft_expr_next(expr);
	}

	if (num_actions == 0)
		return ERR_PTR(-EOPNOTSUPP);

	flow = nft_flow_rule_alloc(num_actions);
	if (!flow)
		return ERR_PTR(-ENOMEM);

	expr = nft_expr_first(rule);

	ctx = kzalloc(sizeof(struct nft_offload_ctx), GFP_KERNEL);
	if (!ctx) {
		err = -ENOMEM;
		goto err_out;
	}
	ctx->net = net;
	ctx->dep.type = NFT_OFFLOAD_DEP_UNSPEC;

	while (nft_expr_more(rule, expr)) {
		if (!expr->ops->offload) {
			err = -EOPNOTSUPP;
			goto err_out;
		}
		err = expr->ops->offload(ctx, flow, expr);
		if (err < 0)
			goto err_out;

		expr = nft_expr_next(expr);
	}
	nft_flow_rule_transfer_vlan(ctx, flow);

	flow->proto = ctx->dep.l3num;
	kfree(ctx);

	return flow;
err_out:
	kfree(ctx);
	nft_flow_rule_destroy(flow);

	return ERR_PTR(err);
}