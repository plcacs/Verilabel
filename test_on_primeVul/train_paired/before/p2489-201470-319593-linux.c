void xfrm_state_fini(struct net *net)
{
	unsigned int sz;

	flush_work(&net->xfrm.state_hash_work);
	flush_work(&xfrm_state_gc_work);
	xfrm_state_flush(net, IPSEC_PROTO_ANY, false, true);

	WARN_ON(!list_empty(&net->xfrm.state_all));

	sz = (net->xfrm.state_hmask + 1) * sizeof(struct hlist_head);
	WARN_ON(!hlist_empty(net->xfrm.state_byspi));
	xfrm_hash_free(net->xfrm.state_byspi, sz);
	WARN_ON(!hlist_empty(net->xfrm.state_bysrc));
	xfrm_hash_free(net->xfrm.state_bysrc, sz);
	WARN_ON(!hlist_empty(net->xfrm.state_bydst));
	xfrm_hash_free(net->xfrm.state_bydst, sz);
}