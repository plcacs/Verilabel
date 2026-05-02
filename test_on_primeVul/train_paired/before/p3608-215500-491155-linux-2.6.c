static int rsvp_dump(struct tcf_proto *tp, unsigned long fh,
		     struct sk_buff *skb, struct tcmsg *t)
{
	struct rsvp_filter *f = (struct rsvp_filter*)fh;
	struct rsvp_session *s;
	unsigned char	 *b = skb->tail;
	struct rtattr *rta;
	struct tc_rsvp_pinfo pinfo;

	if (f == NULL)
		return skb->len;
	s = f->sess;

	t->tcm_handle = f->handle;


	rta = (struct rtattr*)b;
	RTA_PUT(skb, TCA_OPTIONS, 0, NULL);

	RTA_PUT(skb, TCA_RSVP_DST, sizeof(s->dst), &s->dst);
	pinfo.dpi = s->dpi;
	pinfo.spi = f->spi;
	pinfo.protocol = s->protocol;
	pinfo.tunnelid = s->tunnelid;
	pinfo.tunnelhdr = f->tunnelhdr;
	RTA_PUT(skb, TCA_RSVP_PINFO, sizeof(pinfo), &pinfo);
	if (f->res.classid)
		RTA_PUT(skb, TCA_RSVP_CLASSID, 4, &f->res.classid);
	if (((f->handle>>8)&0xFF) != 16)
		RTA_PUT(skb, TCA_RSVP_SRC, sizeof(f->src), f->src);

	if (tcf_exts_dump(skb, &f->exts, &rsvp_ext_map) < 0)
		goto rtattr_failure;

	rta->rta_len = skb->tail - b;

	if (tcf_exts_dump_stats(skb, &f->exts, &rsvp_ext_map) < 0)
		goto rtattr_failure;
	return skb->len;

rtattr_failure:
	skb_trim(skb, b - skb->data);
	return -1;
}