int hidp_connection_add(struct hidp_connadd_req *req,
			struct socket *ctrl_sock,
			struct socket *intr_sock)
{
	struct hidp_session *session;
	struct l2cap_conn *conn;
	struct l2cap_chan *chan = l2cap_pi(ctrl_sock->sk)->chan;
	int ret;

	ret = hidp_verify_sockets(ctrl_sock, intr_sock);
	if (ret)
		return ret;

	conn = NULL;
	l2cap_chan_lock(chan);
	if (chan->conn)
		conn = l2cap_conn_get(chan->conn);
	l2cap_chan_unlock(chan);

	if (!conn)
		return -EBADFD;

	ret = hidp_session_new(&session, &chan->dst, ctrl_sock,
			       intr_sock, req, conn);
	if (ret)
		goto out_conn;

	ret = l2cap_register_user(conn, &session->user);
	if (ret)
		goto out_session;

	ret = 0;

out_session:
	hidp_session_put(session);
out_conn:
	l2cap_conn_put(conn);
	return ret;
}