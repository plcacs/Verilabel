static struct nlattr *reserve_sfa_size(struct sw_flow_actions **sfa,
				       int attr_len, bool log)
{

	struct sw_flow_actions *acts;
	int new_acts_size;
	size_t req_size = NLA_ALIGN(attr_len);
	int next_offset = offsetof(struct sw_flow_actions, actions) +
					(*sfa)->actions_len;

	if (req_size <= (ksize(*sfa) - next_offset))
		goto out;

	new_acts_size = max(next_offset + req_size, ksize(*sfa) * 2);

	if (new_acts_size > MAX_ACTIONS_BUFSIZE) {
		if ((MAX_ACTIONS_BUFSIZE - next_offset) < req_size) {
			OVS_NLERR(log, "Flow action size exceeds max %u",
				  MAX_ACTIONS_BUFSIZE);
			return ERR_PTR(-EMSGSIZE);
		}
		new_acts_size = MAX_ACTIONS_BUFSIZE;
	}

	acts = nla_alloc_flow_actions(new_acts_size);
	if (IS_ERR(acts))
		return (void *)acts;

	memcpy(acts->actions, (*sfa)->actions, (*sfa)->actions_len);
	acts->actions_len = (*sfa)->actions_len;
	acts->orig_len = (*sfa)->orig_len;
	kfree(*sfa);
	*sfa = acts;

out:
	(*sfa)->actions_len += req_size;
	return  (struct nlattr *) ((unsigned char *)(*sfa) + next_offset);
}