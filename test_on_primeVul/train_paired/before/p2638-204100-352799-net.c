static void ppp_cp_parse_cr(struct net_device *dev, u16 pid, u8 id,
			    unsigned int req_len, const u8 *data)
{
	static u8 const valid_accm[6] = { LCP_OPTION_ACCM, 6, 0, 0, 0, 0 };
	const u8 *opt;
	u8 *out;
	unsigned int len = req_len, nak_len = 0, rej_len = 0;

	if (!(out = kmalloc(len, GFP_ATOMIC))) {
		dev->stats.rx_dropped++;
		return;	/* out of memory, ignore CR packet */
	}

	for (opt = data; len; len -= opt[1], opt += opt[1]) {
		if (len < 2 || len < opt[1]) {
			dev->stats.rx_errors++;
			kfree(out);
			return; /* bad packet, drop silently */
		}

		if (pid == PID_LCP)
			switch (opt[0]) {
			case LCP_OPTION_MRU:
				continue; /* MRU always OK and > 1500 bytes? */

			case LCP_OPTION_ACCM: /* async control character map */
				if (!memcmp(opt, valid_accm,
					    sizeof(valid_accm)))
					continue;
				if (!rej_len) { /* NAK it */
					memcpy(out + nak_len, valid_accm,
					       sizeof(valid_accm));
					nak_len += sizeof(valid_accm);
					continue;
				}
				break;
			case LCP_OPTION_MAGIC:
				if (opt[1] != 6 || (!opt[2] && !opt[3] &&
						    !opt[4] && !opt[5]))
					break; /* reject invalid magic number */
				continue;
			}
		/* reject this option */
		memcpy(out + rej_len, opt, opt[1]);
		rej_len += opt[1];
	}

	if (rej_len)
		ppp_cp_event(dev, pid, RCR_BAD, CP_CONF_REJ, id, rej_len, out);
	else if (nak_len)
		ppp_cp_event(dev, pid, RCR_BAD, CP_CONF_NAK, id, nak_len, out);
	else
		ppp_cp_event(dev, pid, RCR_GOOD, CP_CONF_ACK, id, req_len, data);

	kfree(out);
}