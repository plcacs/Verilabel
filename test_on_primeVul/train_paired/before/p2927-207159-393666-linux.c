int af_alg_release(struct socket *sock)
{
	if (sock->sk)
		sock_put(sock->sk);
	return 0;
}