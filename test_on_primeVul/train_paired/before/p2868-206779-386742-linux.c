struct sock *tcp_create_openreq_child(const struct sock *sk,
				      struct request_sock *req,
				      struct sk_buff *skb)
{
	struct sock *newsk = inet_csk_clone_lock(sk, req, GFP_ATOMIC);

	if (newsk) {
		const struct inet_request_sock *ireq = inet_rsk(req);
		struct tcp_request_sock *treq = tcp_rsk(req);
		struct inet_connection_sock *newicsk = inet_csk(newsk);
		struct tcp_sock *newtp = tcp_sk(newsk);

		/* Now setup tcp_sock */
		newtp->pred_flags = 0;

		newtp->rcv_wup = newtp->copied_seq =
		newtp->rcv_nxt = treq->rcv_isn + 1;
		newtp->segs_in = 1;

		newtp->snd_sml = newtp->snd_una =
		newtp->snd_nxt = newtp->snd_up = treq->snt_isn + 1;

		tcp_prequeue_init(newtp);
		INIT_LIST_HEAD(&newtp->tsq_node);

		tcp_init_wl(newtp, treq->rcv_isn);

		newtp->srtt_us = 0;
		newtp->mdev_us = jiffies_to_usecs(TCP_TIMEOUT_INIT);
		minmax_reset(&newtp->rtt_min, tcp_time_stamp, ~0U);
		newicsk->icsk_rto = TCP_TIMEOUT_INIT;
		newicsk->icsk_ack.lrcvtime = tcp_time_stamp;

		newtp->packets_out = 0;
		newtp->retrans_out = 0;
		newtp->sacked_out = 0;
		newtp->fackets_out = 0;
		newtp->snd_ssthresh = TCP_INFINITE_SSTHRESH;
		newtp->tlp_high_seq = 0;
		newtp->lsndtime = treq->snt_synack.stamp_jiffies;
		newsk->sk_txhash = treq->txhash;
		newtp->last_oow_ack_time = 0;
		newtp->total_retrans = req->num_retrans;

		/* So many TCP implementations out there (incorrectly) count the
		 * initial SYN frame in their delayed-ACK and congestion control
		 * algorithms that we must have the following bandaid to talk
		 * efficiently to them.  -DaveM
		 */
		newtp->snd_cwnd = TCP_INIT_CWND;
		newtp->snd_cwnd_cnt = 0;

		/* There's a bubble in the pipe until at least the first ACK. */
		newtp->app_limited = ~0U;

		tcp_init_xmit_timers(newsk);
		newtp->write_seq = newtp->pushed_seq = treq->snt_isn + 1;

		newtp->rx_opt.saw_tstamp = 0;

		newtp->rx_opt.dsack = 0;
		newtp->rx_opt.num_sacks = 0;

		newtp->urg_data = 0;

		if (sock_flag(newsk, SOCK_KEEPOPEN))
			inet_csk_reset_keepalive_timer(newsk,
						       keepalive_time_when(newtp));

		newtp->rx_opt.tstamp_ok = ireq->tstamp_ok;
		if ((newtp->rx_opt.sack_ok = ireq->sack_ok) != 0) {
			if (sysctl_tcp_fack)
				tcp_enable_fack(newtp);
		}
		newtp->window_clamp = req->rsk_window_clamp;
		newtp->rcv_ssthresh = req->rsk_rcv_wnd;
		newtp->rcv_wnd = req->rsk_rcv_wnd;
		newtp->rx_opt.wscale_ok = ireq->wscale_ok;
		if (newtp->rx_opt.wscale_ok) {
			newtp->rx_opt.snd_wscale = ireq->snd_wscale;
			newtp->rx_opt.rcv_wscale = ireq->rcv_wscale;
		} else {
			newtp->rx_opt.snd_wscale = newtp->rx_opt.rcv_wscale = 0;
			newtp->window_clamp = min(newtp->window_clamp, 65535U);
		}
		newtp->snd_wnd = (ntohs(tcp_hdr(skb)->window) <<
				  newtp->rx_opt.snd_wscale);
		newtp->max_window = newtp->snd_wnd;

		if (newtp->rx_opt.tstamp_ok) {
			newtp->rx_opt.ts_recent = req->ts_recent;
			newtp->rx_opt.ts_recent_stamp = get_seconds();
			newtp->tcp_header_len = sizeof(struct tcphdr) + TCPOLEN_TSTAMP_ALIGNED;
		} else {
			newtp->rx_opt.ts_recent_stamp = 0;
			newtp->tcp_header_len = sizeof(struct tcphdr);
		}
		newtp->tsoffset = treq->ts_off;
#ifdef CONFIG_TCP_MD5SIG
		newtp->md5sig_info = NULL;	/*XXX*/
		if (newtp->af_specific->md5_lookup(sk, newsk))
			newtp->tcp_header_len += TCPOLEN_MD5SIG_ALIGNED;
#endif
		if (skb->len >= TCP_MSS_DEFAULT + newtp->tcp_header_len)
			newicsk->icsk_ack.last_seg_size = skb->len - newtp->tcp_header_len;
		newtp->rx_opt.mss_clamp = req->mss;
		tcp_ecn_openreq_child(newtp, req);
		newtp->fastopen_rsk = NULL;
		newtp->syn_data_acked = 0;
		newtp->rack.mstamp.v64 = 0;
		newtp->rack.advanced = 0;

		__TCP_INC_STATS(sock_net(sk), TCP_MIB_PASSIVEOPENS);
	}
	return newsk;
}