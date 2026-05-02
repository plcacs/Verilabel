static inline void tcp_check_send_head(struct sock *sk, struct sk_buff *skb_unlinked)
{
	if (sk->sk_send_head == skb_unlinked)
		sk->sk_send_head = NULL;
	if (tcp_sk(sk)->highest_sack == skb_unlinked)
		tcp_sk(sk)->highest_sack = NULL;
}