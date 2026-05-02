static struct rds_connection *__rds_conn_create(struct net *net,
						const struct in6_addr *laddr,
						const struct in6_addr *faddr,
						struct rds_transport *trans,
						gfp_t gfp, u8 tos,
						int is_outgoing,
						int dev_if)
{
	struct rds_connection *conn, *parent = NULL;
	struct hlist_head *head = rds_conn_bucket(laddr, faddr);
	struct rds_transport *loop_trans;
	unsigned long flags;
	int ret, i;
	int npaths = (trans->t_mp_capable ? RDS_MPATH_WORKERS : 1);

	rcu_read_lock();
	conn = rds_conn_lookup(net, head, laddr, faddr, trans, tos, dev_if);
	if (conn &&
	    conn->c_loopback &&
	    conn->c_trans != &rds_loop_transport &&
	    ipv6_addr_equal(laddr, faddr) &&
	    !is_outgoing) {
		/* This is a looped back IB connection, and we're
		 * called by the code handling the incoming connect.
		 * We need a second connection object into which we
		 * can stick the other QP. */
		parent = conn;
		conn = parent->c_passive;
	}
	rcu_read_unlock();
	if (conn)
		goto out;

	conn = kmem_cache_zalloc(rds_conn_slab, gfp);
	if (!conn) {
		conn = ERR_PTR(-ENOMEM);
		goto out;
	}
	conn->c_path = kcalloc(npaths, sizeof(struct rds_conn_path), gfp);
	if (!conn->c_path) {
		kmem_cache_free(rds_conn_slab, conn);
		conn = ERR_PTR(-ENOMEM);
		goto out;
	}

	INIT_HLIST_NODE(&conn->c_hash_node);
	conn->c_laddr = *laddr;
	conn->c_isv6 = !ipv6_addr_v4mapped(laddr);
	conn->c_faddr = *faddr;
	conn->c_dev_if = dev_if;
	conn->c_tos = tos;

#if IS_ENABLED(CONFIG_IPV6)
	/* If the local address is link local, set c_bound_if to be the
	 * index used for this connection.  Otherwise, set it to 0 as
	 * the socket is not bound to an interface.  c_bound_if is used
	 * to look up a socket when a packet is received
	 */
	if (ipv6_addr_type(laddr) & IPV6_ADDR_LINKLOCAL)
		conn->c_bound_if = dev_if;
	else
#endif
		conn->c_bound_if = 0;

	rds_conn_net_set(conn, net);

	ret = rds_cong_get_maps(conn);
	if (ret) {
		kfree(conn->c_path);
		kmem_cache_free(rds_conn_slab, conn);
		conn = ERR_PTR(ret);
		goto out;
	}

	/*
	 * This is where a connection becomes loopback.  If *any* RDS sockets
	 * can bind to the destination address then we'd rather the messages
	 * flow through loopback rather than either transport.
	 */
	loop_trans = rds_trans_get_preferred(net, faddr, conn->c_dev_if);
	if (loop_trans) {
		rds_trans_put(loop_trans);
		conn->c_loopback = 1;
		if (trans->t_prefer_loopback) {
			if (likely(is_outgoing)) {
				/* "outgoing" connection to local address.
				 * Protocol says it wants the connection
				 * handled by the loopback transport.
				 * This is what TCP does.
				 */
				trans = &rds_loop_transport;
			} else {
				/* No transport currently in use
				 * should end up here, but if it
				 * does, reset/destroy the connection.
				 */
				kmem_cache_free(rds_conn_slab, conn);
				conn = ERR_PTR(-EOPNOTSUPP);
				goto out;
			}
		}
	}

	conn->c_trans = trans;

	init_waitqueue_head(&conn->c_hs_waitq);
	for (i = 0; i < npaths; i++) {
		__rds_conn_path_init(conn, &conn->c_path[i],
				     is_outgoing);
		conn->c_path[i].cp_index = i;
	}
	rcu_read_lock();
	if (rds_destroy_pending(conn))
		ret = -ENETDOWN;
	else
		ret = trans->conn_alloc(conn, GFP_ATOMIC);
	if (ret) {
		rcu_read_unlock();
		kfree(conn->c_path);
		kmem_cache_free(rds_conn_slab, conn);
		conn = ERR_PTR(ret);
		goto out;
	}

	rdsdebug("allocated conn %p for %pI6c -> %pI6c over %s %s\n",
		 conn, laddr, faddr,
		 strnlen(trans->t_name, sizeof(trans->t_name)) ?
		 trans->t_name : "[unknown]", is_outgoing ? "(outgoing)" : "");

	/*
	 * Since we ran without holding the conn lock, someone could
	 * have created the same conn (either normal or passive) in the
	 * interim. We check while holding the lock. If we won, we complete
	 * init and return our conn. If we lost, we rollback and return the
	 * other one.
	 */
	spin_lock_irqsave(&rds_conn_lock, flags);
	if (parent) {
		/* Creating passive conn */
		if (parent->c_passive) {
			trans->conn_free(conn->c_path[0].cp_transport_data);
			kfree(conn->c_path);
			kmem_cache_free(rds_conn_slab, conn);
			conn = parent->c_passive;
		} else {
			parent->c_passive = conn;
			rds_cong_add_conn(conn);
			rds_conn_count++;
		}
	} else {
		/* Creating normal conn */
		struct rds_connection *found;

		found = rds_conn_lookup(net, head, laddr, faddr, trans,
					tos, dev_if);
		if (found) {
			struct rds_conn_path *cp;
			int i;

			for (i = 0; i < npaths; i++) {
				cp = &conn->c_path[i];
				/* The ->conn_alloc invocation may have
				 * allocated resource for all paths, so all
				 * of them may have to be freed here.
				 */
				if (cp->cp_transport_data)
					trans->conn_free(cp->cp_transport_data);
			}
			kfree(conn->c_path);
			kmem_cache_free(rds_conn_slab, conn);
			conn = found;
		} else {
			conn->c_my_gen_num = rds_gen_num;
			conn->c_peer_gen_num = 0;
			hlist_add_head_rcu(&conn->c_hash_node, head);
			rds_cong_add_conn(conn);
			rds_conn_count++;
		}
	}
	spin_unlock_irqrestore(&rds_conn_lock, flags);
	rcu_read_unlock();

out:
	return conn;
}