int cgroup1_parse_param(struct fs_context *fc, struct fs_parameter *param)
{
	struct cgroup_fs_context *ctx = cgroup_fc2context(fc);
	struct cgroup_subsys *ss;
	struct fs_parse_result result;
	int opt, i;

	opt = fs_parse(fc, cgroup1_fs_parameters, param, &result);
	if (opt == -ENOPARAM) {
		if (strcmp(param->key, "source") == 0) {
			if (fc->source)
				return invalf(fc, "Multiple sources not supported");
			fc->source = param->string;
			param->string = NULL;
			return 0;
		}
		for_each_subsys(ss, i) {
			if (strcmp(param->key, ss->legacy_name))
				continue;
			if (!cgroup_ssid_enabled(i) || cgroup1_ssid_disabled(i))
				return invalfc(fc, "Disabled controller '%s'",
					       param->key);
			ctx->subsys_mask |= (1 << i);
			return 0;
		}
		return invalfc(fc, "Unknown subsys name '%s'", param->key);
	}
	if (opt < 0)
		return opt;

	switch (opt) {
	case Opt_none:
		/* Explicitly have no subsystems */
		ctx->none = true;
		break;
	case Opt_all:
		ctx->all_ss = true;
		break;
	case Opt_noprefix:
		ctx->flags |= CGRP_ROOT_NOPREFIX;
		break;
	case Opt_clone_children:
		ctx->cpuset_clone_children = true;
		break;
	case Opt_cpuset_v2_mode:
		ctx->flags |= CGRP_ROOT_CPUSET_V2_MODE;
		break;
	case Opt_xattr:
		ctx->flags |= CGRP_ROOT_XATTR;
		break;
	case Opt_release_agent:
		/* Specifying two release agents is forbidden */
		if (ctx->release_agent)
			return invalfc(fc, "release_agent respecified");
		ctx->release_agent = param->string;
		param->string = NULL;
		break;
	case Opt_name:
		/* blocked by boot param? */
		if (cgroup_no_v1_named)
			return -ENOENT;
		/* Can't specify an empty name */
		if (!param->size)
			return invalfc(fc, "Empty name");
		if (param->size > MAX_CGROUP_ROOT_NAMELEN - 1)
			return invalfc(fc, "Name too long");
		/* Must match [\w.-]+ */
		for (i = 0; i < param->size; i++) {
			char c = param->string[i];
			if (isalnum(c))
				continue;
			if ((c == '.') || (c == '-') || (c == '_'))
				continue;
			return invalfc(fc, "Invalid name");
		}
		/* Specifying two names is forbidden */
		if (ctx->name)
			return invalfc(fc, "name respecified");
		ctx->name = param->string;
		param->string = NULL;
		break;
	}
	return 0;
}