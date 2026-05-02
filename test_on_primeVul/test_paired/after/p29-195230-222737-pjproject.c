void pjmedia_rtcp_xr_rx_rtcp_xr( pjmedia_rtcp_xr_session *sess,
				 const void *pkt,
				 pj_size_t size)
{
    const pjmedia_rtcp_xr_pkt	      *rtcp_xr = (pjmedia_rtcp_xr_pkt*) pkt;
    const pjmedia_rtcp_xr_rb_rr_time  *rb_rr_time = NULL;
    const pjmedia_rtcp_xr_rb_dlrr     *rb_dlrr = NULL;
    const pjmedia_rtcp_xr_rb_stats    *rb_stats = NULL;
    const pjmedia_rtcp_xr_rb_voip_mtc *rb_voip_mtc = NULL;
    const pjmedia_rtcp_xr_rb_header   *rb_hdr = (pjmedia_rtcp_xr_rb_header*) 
						rtcp_xr->buf;
    unsigned pkt_len, rb_len;

    if (rtcp_xr->common.pt != RTCP_XR)
	return;

    pkt_len = pj_ntohs((pj_uint16_t)rtcp_xr->common.length);

    if ((pkt_len + 1) > (size / 4))
	return;

    /* Parse report rpt_types */
    while ((pj_int32_t*)rb_hdr < (pj_int32_t*)pkt + pkt_len)
    {	
	rb_len = pj_ntohs((pj_uint16_t)rb_hdr->length);

	/* Just skip any block with length == 0 (no report content) */
	if (rb_len) {
	    switch (rb_hdr->bt) {
		case BT_RR_TIME:
		    if ((char*)rb_hdr + sizeof(*rb_rr_time) <=
			(char*)pkt + size) 
		    {
			rb_rr_time = (pjmedia_rtcp_xr_rb_rr_time*)rb_hdr;
		    }
		    break;
		case BT_DLRR:
		    if ((char*)rb_hdr + sizeof(*rb_dlrr) <=
			(char*)pkt + size)
		    {
			rb_dlrr = (pjmedia_rtcp_xr_rb_dlrr*)rb_hdr;
		    }
		    break;
		case BT_STATS:
		    if ((char*)rb_hdr + sizeof(*rb_stats) <=
			(char*)pkt + size)
		    {
			rb_stats = (pjmedia_rtcp_xr_rb_stats*)rb_hdr;
		    }
		    break;
		case BT_VOIP_METRICS:
		    if ((char*)rb_hdr + sizeof(*rb_voip_mtc) <=
			(char*)pkt + size)
		    {
			rb_voip_mtc = (pjmedia_rtcp_xr_rb_voip_mtc*)rb_hdr;
		    }
		    break;
		default:
		    break;
	    }
	}
	rb_hdr = (pjmedia_rtcp_xr_rb_header*)
		 ((pj_int32_t*)rb_hdr + rb_len + 1);
    }

    /* Receiving RR Time */
    if (rb_rr_time) {
	/* Save LRR from NTP timestamp of the RR time block report */
	sess->rx_lrr = ((pj_ntohl(rb_rr_time->ntp_sec) & 0x0000FFFF) << 16) | 
		       ((pj_ntohl(rb_rr_time->ntp_frac) >> 16) & 0xFFFF);

	/* Calculate RR arrival time for DLRR */
	pj_get_timestamp(&sess->rx_lrr_time);

	TRACE_((sess->name, "Rx RTCP SR: ntp_ts=%p", sess->rx_lrr,
	       (pj_uint32_t)(sess->rx_lrr_time.u64*65536/
			     sess->rtcp_session->ts_freq.u64)));
    }

    /* Receiving DLRR */
    if (rb_dlrr) {
	pj_uint32_t lrr, now, dlrr;
	pj_uint64_t eedelay;
	pjmedia_rtcp_ntp_rec ntp;

	/* LRR is the middle 32bit of NTP. It has 1/65536 second 
	 * resolution 
	 */
	lrr = pj_ntohl(rb_dlrr->item.lrr);

	/* DLRR is delay since LRR, also in 1/65536 resolution */
	dlrr = pj_ntohl(rb_dlrr->item.dlrr);

	/* Get current time, and convert to 1/65536 resolution */
	pjmedia_rtcp_get_ntp_time(sess->rtcp_session, &ntp);
	now = ((ntp.hi & 0xFFFF) << 16) + (ntp.lo >> 16);

	/* End-to-end delay is (now-lrr-dlrr) */
	eedelay = now - lrr - dlrr;

	/* Convert end to end delay to usec (keeping the calculation in
         * 64bit space)::
	 *   sess->ee_delay = (eedelay * 1000) / 65536;
	 */
	if (eedelay < 4294) {
	    eedelay = (eedelay * 1000000) >> 16;
	} else {
	    eedelay = (eedelay * 1000) >> 16;
	    eedelay *= 1000;
	}

	TRACE_((sess->name, "Rx RTCP XR DLRR: lrr=%p, dlrr=%p (%d:%03dms), "
			   "now=%p, rtt=%p",
		lrr, dlrr, dlrr/65536, (dlrr%65536)*1000/65536,
		now, (pj_uint32_t)eedelay));
	
	/* Only save calculation if "now" is greater than lrr, or
	 * otherwise rtt will be invalid 
	 */
	if (now-dlrr >= lrr) {
	    unsigned rtt = (pj_uint32_t)eedelay;
	    
	    /* Check that eedelay value really makes sense. 
	     * We allow up to 30 seconds RTT!
	     */
	    if (eedelay <= 30 * 1000 * 1000UL) {
		/* "Normalize" rtt value that is exceptionally high.
		 * For such values, "normalize" the rtt to be three times
		 * the average value.
		 */
		if (rtt>((unsigned)sess->stat.rtt.mean*3) && sess->stat.rtt.n!=0)
		{
		    unsigned orig_rtt = rtt;
		    rtt = (unsigned)sess->stat.rtt.mean*3;
		    PJ_LOG(5,(sess->name, 
			      "RTT value %d usec is normalized to %d usec",
			      orig_rtt, rtt));
		}
    	
		TRACE_((sess->name, "RTCP RTT is set to %d usec", rtt));
		pj_math_stat_update(&sess->stat.rtt, rtt);
	    }
	} else {
	    PJ_LOG(5, (sess->name, "Internal RTCP NTP clock skew detected: "
				   "lrr=%p, now=%p, dlrr=%p (%d:%03dms), "
				   "diff=%d",
				   lrr, now, dlrr, dlrr/65536,
				   (dlrr%65536)*1000/65536,
				   dlrr-(now-lrr)));
	}
    }

    /* Receiving Statistics Summary */
    if (rb_stats) {
	pj_uint8_t flags = rb_stats->header.specific;

	pj_bzero(&sess->stat.tx.stat_sum, sizeof(sess->stat.tx.stat_sum));

	/* Range of packets sequence reported in this blocks */
	sess->stat.tx.stat_sum.begin_seq = pj_ntohs(rb_stats->begin_seq);
	sess->stat.tx.stat_sum.end_seq   = pj_ntohs(rb_stats->end_seq);

	/* Get flags of valid fields */
	sess->stat.tx.stat_sum.l = (flags & (1 << 7)) != 0;
	sess->stat.tx.stat_sum.d = (flags & (1 << 6)) != 0;
	sess->stat.tx.stat_sum.j = (flags & (1 << 5)) != 0;
	sess->stat.tx.stat_sum.t = (flags & (3 << 3)) != 0;

	/* Fetch the reports info */
	if (sess->stat.tx.stat_sum.l) {
	    sess->stat.tx.stat_sum.lost = pj_ntohl(rb_stats->lost);
	}

	if (sess->stat.tx.stat_sum.d) {
	    sess->stat.tx.stat_sum.dup = pj_ntohl(rb_stats->dup);
	}

	if (sess->stat.tx.stat_sum.j) {
	    sess->stat.tx.stat_sum.jitter.min = pj_ntohl(rb_stats->jitter_min);
	    sess->stat.tx.stat_sum.jitter.max = pj_ntohl(rb_stats->jitter_max);
	    sess->stat.tx.stat_sum.jitter.mean= pj_ntohl(rb_stats->jitter_mean);
	    pj_math_stat_set_stddev(&sess->stat.tx.stat_sum.jitter, 
				    pj_ntohl(rb_stats->jitter_dev));
	}

	if (sess->stat.tx.stat_sum.t) {
	    sess->stat.tx.stat_sum.toh.min = rb_stats->toh_min;
	    sess->stat.tx.stat_sum.toh.max = rb_stats->toh_max;
	    sess->stat.tx.stat_sum.toh.mean= rb_stats->toh_mean;
	    pj_math_stat_set_stddev(&sess->stat.tx.stat_sum.toh, 
				    pj_ntohl(rb_stats->toh_dev));
	}

	pj_gettimeofday(&sess->stat.tx.stat_sum.update);
    }

    /* Receiving VoIP Metrics */
    if (rb_voip_mtc) {
	sess->stat.tx.voip_mtc.loss_rate = rb_voip_mtc->loss_rate;
	sess->stat.tx.voip_mtc.discard_rate = rb_voip_mtc->discard_rate;
	sess->stat.tx.voip_mtc.burst_den = rb_voip_mtc->burst_den;
	sess->stat.tx.voip_mtc.gap_den = rb_voip_mtc->gap_den;
	sess->stat.tx.voip_mtc.burst_dur = pj_ntohs(rb_voip_mtc->burst_dur);
	sess->stat.tx.voip_mtc.gap_dur = pj_ntohs(rb_voip_mtc->gap_dur);
	sess->stat.tx.voip_mtc.rnd_trip_delay = 
					pj_ntohs(rb_voip_mtc->rnd_trip_delay);
	sess->stat.tx.voip_mtc.end_sys_delay = 
					pj_ntohs(rb_voip_mtc->end_sys_delay);
	/* signal & noise level encoded in two's complement form */
	sess->stat.tx.voip_mtc.signal_lvl = (pj_int8_t)
				    ((rb_voip_mtc->signal_lvl > 127)?
				     ((int)rb_voip_mtc->signal_lvl - 256) : 
				     rb_voip_mtc->signal_lvl);
	sess->stat.tx.voip_mtc.noise_lvl  = (pj_int8_t)
				    ((rb_voip_mtc->noise_lvl > 127)?
				     ((int)rb_voip_mtc->noise_lvl - 256) : 
				     rb_voip_mtc->noise_lvl);
	sess->stat.tx.voip_mtc.rerl = rb_voip_mtc->rerl;
	sess->stat.tx.voip_mtc.gmin = rb_voip_mtc->gmin;
	sess->stat.tx.voip_mtc.r_factor = rb_voip_mtc->r_factor;
	sess->stat.tx.voip_mtc.ext_r_factor = rb_voip_mtc->ext_r_factor;
	sess->stat.tx.voip_mtc.mos_lq = rb_voip_mtc->mos_lq;
	sess->stat.tx.voip_mtc.mos_cq = rb_voip_mtc->mos_cq;
	sess->stat.tx.voip_mtc.rx_config = rb_voip_mtc->rx_config;
	sess->stat.tx.voip_mtc.jb_nom = pj_ntohs(rb_voip_mtc->jb_nom);
	sess->stat.tx.voip_mtc.jb_max = pj_ntohs(rb_voip_mtc->jb_max);
	sess->stat.tx.voip_mtc.jb_abs_max = pj_ntohs(rb_voip_mtc->jb_abs_max);

	pj_gettimeofday(&sess->stat.tx.voip_mtc.update);
    }
}