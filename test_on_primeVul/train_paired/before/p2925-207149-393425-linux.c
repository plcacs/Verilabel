struct sock *sk_clone_lock(const struct sock *sk, const gfp_t priority)
{
	struct sock *newsk;
	bool is_charged = true;

	newsk = sk_prot_alloc(sk->sk_prot, priority, sk->sk_family);
	if (newsk != NULL) {
		struct sk_filter *filter;

		sock_copy(newsk, sk);

		/* SANITY */
		if (likely(newsk->sk_net_refcnt))
			get_net(sock_net(newsk));
		sk_node_init(&newsk->sk_node);
		sock_lock_init(newsk);
		bh_lock_sock(newsk);
		newsk->sk_backlog.head	= newsk->sk_backlog.tail = NULL;
		newsk->sk_backlog.len = 0;

		atomic_set(&newsk->sk_rmem_alloc, 0);
		/*
		 * sk_wmem_alloc set to one (see sk_free() and sock_wfree())
		 */
		refcount_set(&newsk->sk_wmem_alloc, 1);
		atomic_set(&newsk->sk_omem_alloc, 0);
		sk_init_common(newsk);

		newsk->sk_dst_cache	= NULL;
		newsk->sk_dst_pending_confirm = 0;
		newsk->sk_wmem_queued	= 0;
		newsk->sk_forward_alloc = 0;
		atomic_set(&newsk->sk_drops, 0);
		newsk->sk_send_head	= NULL;
		newsk->sk_userlocks	= sk->sk_userlocks & ~SOCK_BINDPORT_LOCK;
		atomic_set(&newsk->sk_zckey, 0);

		sock_reset_flag(newsk, SOCK_DONE);

		filter = rcu_dereference_protected(newsk->sk_filter, 1);
		if (filter != NULL)
			/* though it's an empty new sock, the charging may fail
			 * if sysctl_optmem_max was changed between creation of
			 * original socket and cloning
			 */
			is_charged = sk_filter_charge(newsk, filter);

		if (unlikely(!is_charged || xfrm_sk_clone_policy(newsk, sk))) {
			/* We need to make sure that we don't uncharge the new
			 * socket if we couldn't charge it in the first place
			 * as otherwise we uncharge the parent's filter.
			 */
			if (!is_charged)
				RCU_INIT_POINTER(newsk->sk_filter, NULL);
			sk_free_unlock_clone(newsk);
			newsk = NULL;
			goto out;
		}
		RCU_INIT_POINTER(newsk->sk_reuseport_cb, NULL);

		newsk->sk_err	   = 0;
		newsk->sk_err_soft = 0;
		newsk->sk_priority = 0;
		newsk->sk_incoming_cpu = raw_smp_processor_id();
		atomic64_set(&newsk->sk_cookie, 0);

		mem_cgroup_sk_alloc(newsk);
		cgroup_sk_alloc(&newsk->sk_cgrp_data);

		/*
		 * Before updating sk_refcnt, we must commit prior changes to memory
		 * (Documentation/RCU/rculist_nulls.txt for details)
		 */
		smp_wmb();
		refcount_set(&newsk->sk_refcnt, 2);

		/*
		 * Increment the counter in the same struct proto as the master
		 * sock (sk_refcnt_debug_inc uses newsk->sk_prot->socks, that
		 * is the same as sk->sk_prot->socks, as this field was copied
		 * with memcpy).
		 *
		 * This _changes_ the previous behaviour, where
		 * tcp_create_openreq_child always was incrementing the
		 * equivalent to tcp_prot->socks (inet_sock_nr), so this have
		 * to be taken into account in all callers. -acme
		 */
		sk_refcnt_debug_inc(newsk);
		sk_set_socket(newsk, NULL);
		newsk->sk_wq = NULL;

		if (newsk->sk_prot->sockets_allocated)
			sk_sockets_allocated_inc(newsk);

		if (sock_needs_netstamp(sk) &&
		    newsk->sk_flags & SK_FLAGS_TIMESTAMP)
			net_enable_timestamp();
	}
out:
	return newsk;
}