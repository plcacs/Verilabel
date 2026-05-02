tcp_sack_option(struct tcpcb *tp, struct tcphdr *th, u_char *cp, int optlen)
{
	int tmp_olen;
	u_char *tmp_cp;
	struct sackhole *cur, *p, *temp;

	if (!tp->sack_enable)
		return;
	/* SACK without ACK doesn't make sense. */
	if ((th->th_flags & TH_ACK) == 0)
	       return;
	/* Make sure the ACK on this segment is in [snd_una, snd_max]. */
	if (SEQ_LT(th->th_ack, tp->snd_una) ||
	    SEQ_GT(th->th_ack, tp->snd_max))
		return;
	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	if (optlen <= 2 || (optlen - 2) % TCPOLEN_SACK != 0)
		return;
	/* Note: TCPOLEN_SACK must be 2*sizeof(tcp_seq) */
	tmp_cp = cp + 2;
	tmp_olen = optlen - 2;
	tcpstat_inc(tcps_sack_rcv_opts);
	if (tp->snd_numholes < 0)
		tp->snd_numholes = 0;
	if (tp->t_maxseg == 0)
		panic("tcp_sack_option"); /* Should never happen */
	while (tmp_olen > 0) {
		struct sackblk sack;

		memcpy(&sack.start, tmp_cp, sizeof(tcp_seq));
		sack.start = ntohl(sack.start);
		memcpy(&sack.end, tmp_cp + sizeof(tcp_seq), sizeof(tcp_seq));
		sack.end = ntohl(sack.end);
		tmp_olen -= TCPOLEN_SACK;
		tmp_cp += TCPOLEN_SACK;
		if (SEQ_LEQ(sack.end, sack.start))
			continue; /* bad SACK fields */
		if (SEQ_LEQ(sack.end, tp->snd_una))
			continue; /* old block */
		if (SEQ_GT(th->th_ack, tp->snd_una)) {
			if (SEQ_LT(sack.start, th->th_ack))
				continue;
		}
		if (SEQ_GT(sack.end, tp->snd_max))
			continue;
		if (tp->snd_holes == NULL) { /* first hole */
			tp->snd_holes = (struct sackhole *)
			    pool_get(&sackhl_pool, PR_NOWAIT);
			if (tp->snd_holes == NULL) {
				/* ENOBUFS, so ignore SACKed block for now*/
				goto done;
			}
			cur = tp->snd_holes;
			cur->start = th->th_ack;
			cur->end = sack.start;
			cur->rxmit = cur->start;
			cur->next = NULL;
			tp->snd_numholes = 1;
			tp->rcv_lastsack = sack.end;
			/*
			 * dups is at least one.  If more data has been
			 * SACKed, it can be greater than one.
			 */
			cur->dups = min(tcprexmtthresh,
			    ((sack.end - cur->end)/tp->t_maxseg));
			if (cur->dups < 1)
				cur->dups = 1;
			continue; /* with next sack block */
		}
		/* Go thru list of holes:  p = previous,  cur = current */
		p = cur = tp->snd_holes;
		while (cur) {
			if (SEQ_LEQ(sack.end, cur->start))
				/* SACKs data before the current hole */
				break; /* no use going through more holes */
			if (SEQ_GEQ(sack.start, cur->end)) {
				/* SACKs data beyond the current hole */
				cur->dups++;
				if (((sack.end - cur->end)/tp->t_maxseg) >=
				    tcprexmtthresh)
					cur->dups = tcprexmtthresh;
				p = cur;
				cur = cur->next;
				continue;
			}
			if (SEQ_LEQ(sack.start, cur->start)) {
				/* Data acks at least the beginning of hole */
				if (SEQ_GEQ(sack.end, cur->end)) {
					/* Acks entire hole, so delete hole */
					if (p != cur) {
						p->next = cur->next;
						pool_put(&sackhl_pool, cur);
						cur = p->next;
					} else {
						cur = cur->next;
						pool_put(&sackhl_pool, p);
						p = cur;
						tp->snd_holes = p;
					}
					tp->snd_numholes--;
					continue;
				}
				/* otherwise, move start of hole forward */
				cur->start = sack.end;
				cur->rxmit = SEQ_MAX(cur->rxmit, cur->start);
				p = cur;
				cur = cur->next;
				continue;
			}
			/* move end of hole backward */
			if (SEQ_GEQ(sack.end, cur->end)) {
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur->dups++;
				if (((sack.end - cur->end)/tp->t_maxseg) >=
				    tcprexmtthresh)
					cur->dups = tcprexmtthresh;
				p = cur;
				cur = cur->next;
				continue;
			}
			if (SEQ_LT(cur->start, sack.start) &&
			    SEQ_GT(cur->end, sack.end)) {
				/*
				 * ACKs some data in middle of a hole; need to
				 * split current hole
				 */
				temp = (struct sackhole *)
				    pool_get(&sackhl_pool, PR_NOWAIT);
				if (temp == NULL)
					goto done; /* ENOBUFS */
				temp->next = cur->next;
				temp->start = sack.end;
				temp->end = cur->end;
				temp->dups = cur->dups;
				temp->rxmit = SEQ_MAX(cur->rxmit, temp->start);
				cur->end = sack.start;
				cur->rxmit = SEQ_MIN(cur->rxmit, cur->end);
				cur->dups++;
				if (((sack.end - cur->end)/tp->t_maxseg) >=
					tcprexmtthresh)
					cur->dups = tcprexmtthresh;
				cur->next = temp;
				p = temp;
				cur = p->next;
				tp->snd_numholes++;
			}
		}
		/* At this point, p points to the last hole on the list */
		if (SEQ_LT(tp->rcv_lastsack, sack.start)) {
			/*
			 * Need to append new hole at end.
			 * Last hole is p (and it's not NULL).
			 */
			temp = (struct sackhole *)
			    pool_get(&sackhl_pool, PR_NOWAIT);
			if (temp == NULL)
				goto done; /* ENOBUFS */
			temp->start = tp->rcv_lastsack;
			temp->end = sack.start;
			temp->dups = min(tcprexmtthresh,
			    ((sack.end - sack.start)/tp->t_maxseg));
			if (temp->dups < 1)
				temp->dups = 1;
			temp->rxmit = temp->start;
			temp->next = 0;
			p->next = temp;
			tp->rcv_lastsack = sack.end;
			tp->snd_numholes++;
		}
	}
done:
	return;
}