ctnetlink_parse_tuple_filter(const struct nlattr * const cda[],
			      struct nf_conntrack_tuple *tuple, u32 type,
			      u_int8_t l3num, struct nf_conntrack_zone *zone,
			      u_int32_t flags)
{
	struct nlattr *tb[CTA_TUPLE_MAX+1];
	int err;

	memset(tuple, 0, sizeof(*tuple));

	err = nla_parse_nested_deprecated(tb, CTA_TUPLE_MAX, cda[type],
					  tuple_nla_policy, NULL);
	if (err < 0)
		return err;


	tuple->src.l3num = l3num;

	if (flags & CTA_FILTER_FLAG(CTA_IP_DST) ||
	    flags & CTA_FILTER_FLAG(CTA_IP_SRC)) {
		if (!tb[CTA_TUPLE_IP])
			return -EINVAL;

		err = ctnetlink_parse_tuple_ip(tb[CTA_TUPLE_IP], tuple, flags);
		if (err < 0)
			return err;
	}

	if (flags & CTA_FILTER_FLAG(CTA_PROTO_NUM)) {
		if (!tb[CTA_TUPLE_PROTO])
			return -EINVAL;

		err = ctnetlink_parse_tuple_proto(tb[CTA_TUPLE_PROTO], tuple, flags);
		if (err < 0)
			return err;
	} else if (flags & CTA_FILTER_FLAG(ALL_CTA_PROTO)) {
		/* Can't manage proto flags without a protonum  */
		return -EINVAL;
	}

	if ((flags & CTA_FILTER_FLAG(CTA_TUPLE_ZONE)) && tb[CTA_TUPLE_ZONE]) {
		if (!zone)
			return -EINVAL;

		err = ctnetlink_parse_tuple_zone(tb[CTA_TUPLE_ZONE],
						 type, zone);
		if (err < 0)
			return err;
	}

	/* orig and expect tuples get DIR_ORIGINAL */
	if (type == CTA_TUPLE_REPLY)
		tuple->dst.dir = IP_CT_DIR_REPLY;
	else
		tuple->dst.dir = IP_CT_DIR_ORIGINAL;

	return 0;
}