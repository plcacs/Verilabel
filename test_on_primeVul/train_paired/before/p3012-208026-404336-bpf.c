__htab_map_lookup_and_delete_batch(struct bpf_map *map,
				   const union bpf_attr *attr,
				   union bpf_attr __user *uattr,
				   bool do_delete, bool is_lru_map,
				   bool is_percpu)
{
	struct bpf_htab *htab = container_of(map, struct bpf_htab, map);
	u32 bucket_cnt, total, key_size, value_size, roundup_key_size;
	void *keys = NULL, *values = NULL, *value, *dst_key, *dst_val;
	void __user *uvalues = u64_to_user_ptr(attr->batch.values);
	void __user *ukeys = u64_to_user_ptr(attr->batch.keys);
	void __user *ubatch = u64_to_user_ptr(attr->batch.in_batch);
	u32 batch, max_count, size, bucket_size;
	struct htab_elem *node_to_free = NULL;
	u64 elem_map_flags, map_flags;
	struct hlist_nulls_head *head;
	struct hlist_nulls_node *n;
	unsigned long flags = 0;
	bool locked = false;
	struct htab_elem *l;
	struct bucket *b;
	int ret = 0;

	elem_map_flags = attr->batch.elem_flags;
	if ((elem_map_flags & ~BPF_F_LOCK) ||
	    ((elem_map_flags & BPF_F_LOCK) && !map_value_has_spin_lock(map)))
		return -EINVAL;

	map_flags = attr->batch.flags;
	if (map_flags)
		return -EINVAL;

	max_count = attr->batch.count;
	if (!max_count)
		return 0;

	if (put_user(0, &uattr->batch.count))
		return -EFAULT;

	batch = 0;
	if (ubatch && copy_from_user(&batch, ubatch, sizeof(batch)))
		return -EFAULT;

	if (batch >= htab->n_buckets)
		return -ENOENT;

	key_size = htab->map.key_size;
	roundup_key_size = round_up(htab->map.key_size, 8);
	value_size = htab->map.value_size;
	size = round_up(value_size, 8);
	if (is_percpu)
		value_size = size * num_possible_cpus();
	total = 0;
	/* while experimenting with hash tables with sizes ranging from 10 to
	 * 1000, it was observed that a bucket can have upto 5 entries.
	 */
	bucket_size = 5;

alloc:
	/* We cannot do copy_from_user or copy_to_user inside
	 * the rcu_read_lock. Allocate enough space here.
	 */
	keys = kvmalloc(key_size * bucket_size, GFP_USER | __GFP_NOWARN);
	values = kvmalloc(value_size * bucket_size, GFP_USER | __GFP_NOWARN);
	if (!keys || !values) {
		ret = -ENOMEM;
		goto after_loop;
	}

again:
	bpf_disable_instrumentation();
	rcu_read_lock();
again_nocopy:
	dst_key = keys;
	dst_val = values;
	b = &htab->buckets[batch];
	head = &b->head;
	/* do not grab the lock unless need it (bucket_cnt > 0). */
	if (locked) {
		ret = htab_lock_bucket(htab, b, batch, &flags);
		if (ret)
			goto next_batch;
	}

	bucket_cnt = 0;
	hlist_nulls_for_each_entry_rcu(l, n, head, hash_node)
		bucket_cnt++;

	if (bucket_cnt && !locked) {
		locked = true;
		goto again_nocopy;
	}

	if (bucket_cnt > (max_count - total)) {
		if (total == 0)
			ret = -ENOSPC;
		/* Note that since bucket_cnt > 0 here, it is implicit
		 * that the locked was grabbed, so release it.
		 */
		htab_unlock_bucket(htab, b, batch, flags);
		rcu_read_unlock();
		bpf_enable_instrumentation();
		goto after_loop;
	}

	if (bucket_cnt > bucket_size) {
		bucket_size = bucket_cnt;
		/* Note that since bucket_cnt > 0 here, it is implicit
		 * that the locked was grabbed, so release it.
		 */
		htab_unlock_bucket(htab, b, batch, flags);
		rcu_read_unlock();
		bpf_enable_instrumentation();
		kvfree(keys);
		kvfree(values);
		goto alloc;
	}

	/* Next block is only safe to run if you have grabbed the lock */
	if (!locked)
		goto next_batch;

	hlist_nulls_for_each_entry_safe(l, n, head, hash_node) {
		memcpy(dst_key, l->key, key_size);

		if (is_percpu) {
			int off = 0, cpu;
			void __percpu *pptr;

			pptr = htab_elem_get_ptr(l, map->key_size);
			for_each_possible_cpu(cpu) {
				bpf_long_memcpy(dst_val + off,
						per_cpu_ptr(pptr, cpu), size);
				off += size;
			}
		} else {
			value = l->key + roundup_key_size;
			if (elem_map_flags & BPF_F_LOCK)
				copy_map_value_locked(map, dst_val, value,
						      true);
			else
				copy_map_value(map, dst_val, value);
			check_and_init_map_lock(map, dst_val);
		}
		if (do_delete) {
			hlist_nulls_del_rcu(&l->hash_node);

			/* bpf_lru_push_free() will acquire lru_lock, which
			 * may cause deadlock. See comments in function
			 * prealloc_lru_pop(). Let us do bpf_lru_push_free()
			 * after releasing the bucket lock.
			 */
			if (is_lru_map) {
				l->batch_flink = node_to_free;
				node_to_free = l;
			} else {
				free_htab_elem(htab, l);
			}
		}
		dst_key += key_size;
		dst_val += value_size;
	}

	htab_unlock_bucket(htab, b, batch, flags);
	locked = false;

	while (node_to_free) {
		l = node_to_free;
		node_to_free = node_to_free->batch_flink;
		bpf_lru_push_free(&htab->lru, &l->lru_node);
	}

next_batch:
	/* If we are not copying data, we can go to next bucket and avoid
	 * unlocking the rcu.
	 */
	if (!bucket_cnt && (batch + 1 < htab->n_buckets)) {
		batch++;
		goto again_nocopy;
	}

	rcu_read_unlock();
	bpf_enable_instrumentation();
	if (bucket_cnt && (copy_to_user(ukeys + total * key_size, keys,
	    key_size * bucket_cnt) ||
	    copy_to_user(uvalues + total * value_size, values,
	    value_size * bucket_cnt))) {
		ret = -EFAULT;
		goto after_loop;
	}

	total += bucket_cnt;
	batch++;
	if (batch >= htab->n_buckets) {
		ret = -ENOENT;
		goto after_loop;
	}
	goto again;

after_loop:
	if (ret == -EFAULT)
		goto out;

	/* copy # of entries and next batch */
	ubatch = u64_to_user_ptr(attr->batch.out_batch);
	if (copy_to_user(ubatch, &batch, sizeof(batch)) ||
	    put_user(total, &uattr->batch.count))
		ret = -EFAULT;

out:
	kvfree(keys);
	kvfree(values);
	return ret;
}