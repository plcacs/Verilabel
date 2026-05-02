struct bpf_map *bpf_map_meta_alloc(int inner_map_ufd)
{
	struct bpf_map *inner_map, *inner_map_meta;
	struct fd f;

	f = fdget(inner_map_ufd);
	inner_map = __bpf_map_get(f);
	if (IS_ERR(inner_map))
		return inner_map;

	/* prog_array->owner_prog_type and owner_jited
	 * is a runtime binding.  Doing static check alone
	 * in the verifier is not enough.
	 */
	if (inner_map->map_type == BPF_MAP_TYPE_PROG_ARRAY ||
	    inner_map->map_type == BPF_MAP_TYPE_CGROUP_STORAGE ||
	    inner_map->map_type == BPF_MAP_TYPE_PERCPU_CGROUP_STORAGE) {
		fdput(f);
		return ERR_PTR(-ENOTSUPP);
	}

	/* Does not support >1 level map-in-map */
	if (inner_map->inner_map_meta) {
		fdput(f);
		return ERR_PTR(-EINVAL);
	}

	inner_map_meta = kzalloc(sizeof(*inner_map_meta), GFP_USER);
	if (!inner_map_meta) {
		fdput(f);
		return ERR_PTR(-ENOMEM);
	}

	inner_map_meta->map_type = inner_map->map_type;
	inner_map_meta->key_size = inner_map->key_size;
	inner_map_meta->value_size = inner_map->value_size;
	inner_map_meta->map_flags = inner_map->map_flags;
	inner_map_meta->ops = inner_map->ops;
	inner_map_meta->max_entries = inner_map->max_entries;

	fdput(f);
	return inner_map_meta;
}