static int sco_sock_sendmsg(struct socket *sock, struct msghdr *msg,
			    size_t len)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	int err;

	BT_DBG("sock %p, sk %p", sock, sk);

	err = sock_error(sk);
	if (err)
		return err;

	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;

	skb = bt_skb_sendmsg(sk, msg, len, len, 0, 0);
	if (IS_ERR_OR_NULL(skb))
		return PTR_ERR(skb);

	lock_sock(sk);

	if (sk->sk_state == BT_CONNECTED)
		err = sco_send_frame(sk, skb);
	else
		err = -ENOTCONN;

	release_sock(sk);
	if (err)
		kfree_skb(skb);
	return err;
}