int mpol_parse_str(char *str, struct mempolicy **mpol)
{
	struct mempolicy *new = NULL;
	unsigned short mode_flags;
	nodemask_t nodes;
	char *nodelist = strchr(str, ':');
	char *flags = strchr(str, '=');
	int err = 1, mode;

	if (flags)
		*flags++ = '\0';	/* terminate mode string */

	if (nodelist) {
		/* NUL-terminate mode or flags string */
		*nodelist++ = '\0';
		if (nodelist_parse(nodelist, nodes))
			goto out;
		if (!nodes_subset(nodes, node_states[N_MEMORY]))
			goto out;
	} else
		nodes_clear(nodes);

	mode = match_string(policy_modes, MPOL_MAX, str);
	if (mode < 0)
		goto out;

	switch (mode) {
	case MPOL_PREFERRED:
		/*
		 * Insist on a nodelist of one node only
		 */
		if (nodelist) {
			char *rest = nodelist;
			while (isdigit(*rest))
				rest++;
			if (*rest)
				goto out;
		}
		break;
	case MPOL_INTERLEAVE:
		/*
		 * Default to online nodes with memory if no nodelist
		 */
		if (!nodelist)
			nodes = node_states[N_MEMORY];
		break;
	case MPOL_LOCAL:
		/*
		 * Don't allow a nodelist;  mpol_new() checks flags
		 */
		if (nodelist)
			goto out;
		mode = MPOL_PREFERRED;
		break;
	case MPOL_DEFAULT:
		/*
		 * Insist on a empty nodelist
		 */
		if (!nodelist)
			err = 0;
		goto out;
	case MPOL_BIND:
		/*
		 * Insist on a nodelist
		 */
		if (!nodelist)
			goto out;
	}

	mode_flags = 0;
	if (flags) {
		/*
		 * Currently, we only support two mutually exclusive
		 * mode flags.
		 */
		if (!strcmp(flags, "static"))
			mode_flags |= MPOL_F_STATIC_NODES;
		else if (!strcmp(flags, "relative"))
			mode_flags |= MPOL_F_RELATIVE_NODES;
		else
			goto out;
	}

	new = mpol_new(mode, mode_flags, &nodes);
	if (IS_ERR(new))
		goto out;

	/*
	 * Save nodes for mpol_to_str() to show the tmpfs mount options
	 * for /proc/mounts, /proc/pid/mounts and /proc/pid/mountinfo.
	 */
	if (mode != MPOL_PREFERRED)
		new->v.nodes = nodes;
	else if (nodelist)
		new->v.preferred_node = first_node(nodes);
	else
		new->flags |= MPOL_F_LOCAL;

	/*
	 * Save nodes for contextualization: this will be used to "clone"
	 * the mempolicy in a specific context [cpuset] at a later time.
	 */
	new->w.user_nodemask = nodes;

	err = 0;

out:
	/* Restore string for error message */
	if (nodelist)
		*--nodelist = ':';
	if (flags)
		*--flags = '=';
	if (!err)
		*mpol = new;
	return err;
}