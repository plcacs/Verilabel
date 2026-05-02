process_ra(struct Interface *iface, unsigned char *msg, int len,
	struct sockaddr_in6 *addr)
{
	struct nd_router_advert *radvert;
	char addr_str[INET6_ADDRSTRLEN];
	uint8_t *opt_str;

	print_addr(&addr->sin6_addr, addr_str);

	radvert = (struct nd_router_advert *) msg;

	if ((radvert->nd_ra_curhoplimit && iface->AdvCurHopLimit) &&
	   (radvert->nd_ra_curhoplimit != iface->AdvCurHopLimit))
	{
		flog(LOG_WARNING, "our AdvCurHopLimit on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	if ((radvert->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) && !iface->AdvManagedFlag)
	{
		flog(LOG_WARNING, "our AdvManagedFlag on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	if ((radvert->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) && !iface->AdvOtherConfigFlag)
	{
		flog(LOG_WARNING, "our AdvOtherConfigFlag on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	/* note: we don't check the default router preference here, because they're likely different */

	if ((radvert->nd_ra_reachable && iface->AdvReachableTime) &&
	   (ntohl(radvert->nd_ra_reachable) != iface->AdvReachableTime))
	{
		flog(LOG_WARNING, "our AdvReachableTime on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	if ((radvert->nd_ra_retransmit && iface->AdvRetransTimer) &&
	   (ntohl(radvert->nd_ra_retransmit) != iface->AdvRetransTimer))
	{
		flog(LOG_WARNING, "our AdvRetransTimer on %s doesn't agree with %s",
			iface->Name, addr_str);
	}

	len -= sizeof(struct nd_router_advert);

	if (len == 0)
		return;

	opt_str = (uint8_t *)(msg + sizeof(struct nd_router_advert));

	while (len > 0)
	{
		int optlen;
		struct nd_opt_prefix_info *pinfo;
		struct nd_opt_rdnss_info_local *rdnssinfo;
		struct nd_opt_dnssl_info_local *dnsslinfo;
		struct nd_opt_mtu *mtu;
		struct AdvPrefix *prefix;
		struct AdvRDNSS *rdnss;
		char prefix_str[INET6_ADDRSTRLEN];
		char rdnss_str[INET6_ADDRSTRLEN];
		char suffix[256];
		unsigned int offset, label_len;
		uint32_t preferred, valid, count;

		if (len < 2)
		{
			flog(LOG_ERR, "trailing garbage in RA on %s from %s",
				iface->Name, addr_str);
			break;
		}

		optlen = (opt_str[1] << 3);

		if (optlen == 0)
		{
			flog(LOG_ERR, "zero length option in RA on %s from %s",
				iface->Name, addr_str);
			break;
		}
		else if (optlen > len)
		{
			flog(LOG_ERR, "option length greater than total"
				" length in RA on %s from %s",
				iface->Name, addr_str);
			break;
		}

		switch (*opt_str)
		{
		case ND_OPT_MTU:
			mtu = (struct nd_opt_mtu *)opt_str;

			if (iface->AdvLinkMTU && (ntohl(mtu->nd_opt_mtu_mtu) != iface->AdvLinkMTU))
			{
				flog(LOG_WARNING, "our AdvLinkMTU on %s doesn't agree with %s",
					iface->Name, addr_str);
			}
			break;
		case ND_OPT_PREFIX_INFORMATION:
			pinfo = (struct nd_opt_prefix_info *) opt_str;
			preferred = ntohl(pinfo->nd_opt_pi_preferred_time);
			valid = ntohl(pinfo->nd_opt_pi_valid_time);

			prefix = iface->AdvPrefixList;
			while (prefix)
			{
				if (prefix->enabled &&
				    (prefix->PrefixLen == pinfo->nd_opt_pi_prefix_len) &&
				    addr_match(&prefix->Prefix, &pinfo->nd_opt_pi_prefix,
				    	 prefix->PrefixLen))
				{
					print_addr(&prefix->Prefix, prefix_str);

					if (!prefix->DecrementLifetimesFlag && valid != prefix->AdvValidLifetime)
					{
						flog(LOG_WARNING, "our AdvValidLifetime on"
						 " %s for %s doesn't agree with %s",
						 iface->Name,
						 prefix_str,
						 addr_str
						 );
					}
					if (!prefix->DecrementLifetimesFlag && preferred != prefix->AdvPreferredLifetime)
					{
						flog(LOG_WARNING, "our AdvPreferredLifetime on"
						 " %s for %s doesn't agree with %s",
						 iface->Name,
						 prefix_str,
						 addr_str
						 );
					}
				}

				prefix = prefix->next;
			}
			break;
		case ND_OPT_ROUTE_INFORMATION:
			/* not checked: these will very likely vary a lot */
			break;
		case ND_OPT_SOURCE_LINKADDR:
			/* not checked */
			break;
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_REDIRECTED_HEADER:
			flog(LOG_ERR, "invalid option %d in RA on %s from %s",
				(int)*opt_str, iface->Name, addr_str);
			break;
		/* Mobile IPv6 extensions */
		case ND_OPT_RTR_ADV_INTERVAL:
		case ND_OPT_HOME_AGENT_INFO:
			/* not checked */
			break;
		case ND_OPT_RDNSS_INFORMATION:
			rdnssinfo = (struct nd_opt_rdnss_info_local *) opt_str;
			count = rdnssinfo->nd_opt_rdnssi_len;

			/* Check the RNDSS addresses received */
			switch (count) {
				case 7:
					rdnss = iface->AdvRDNSSList;
					if (!check_rdnss_presence(rdnss, &rdnssinfo->nd_opt_rdnssi_addr3 )) {
						/* no match found in iface->AdvRDNSSList */
						print_addr(&rdnssinfo->nd_opt_rdnssi_addr3, rdnss_str);
						flog(LOG_WARNING, "RDNSS address %s received on %s from %s is not advertised by us",
							rdnss_str, iface->Name, addr_str);
					}
					/* FALLTHROUGH */
				case 5:
					rdnss = iface->AdvRDNSSList;
					if (!check_rdnss_presence(rdnss, &rdnssinfo->nd_opt_rdnssi_addr2 )) {
						/* no match found in iface->AdvRDNSSList */
						print_addr(&rdnssinfo->nd_opt_rdnssi_addr2, rdnss_str);
						flog(LOG_WARNING, "RDNSS address %s received on %s from %s is not advertised by us",
							rdnss_str, iface->Name, addr_str);
					}
					/* FALLTHROUGH */
				case 3:
					rdnss = iface->AdvRDNSSList;
					if (!check_rdnss_presence(rdnss, &rdnssinfo->nd_opt_rdnssi_addr1 )) {
						/* no match found in iface->AdvRDNSSList */
						print_addr(&rdnssinfo->nd_opt_rdnssi_addr1, rdnss_str);
						flog(LOG_WARNING, "RDNSS address %s received on %s from %s is not advertised by us",
							rdnss_str, iface->Name, addr_str);
					}

					break;
				default:
					flog(LOG_ERR, "invalid len %i in RDNSS option on %s from %s",
							count, iface->Name, addr_str);
			}

			break;
		case ND_OPT_DNSSL_INFORMATION:
			dnsslinfo = (struct nd_opt_dnssl_info_local *) opt_str;
			suffix[0] = '\0';
			for (offset = 0; offset < (dnsslinfo->nd_opt_dnssli_len-1)*8;) {
				label_len = dnsslinfo->nd_opt_dnssli_suffixes[offset++];

				if (label_len == 0) {
					/*
					 * Ignore empty suffixes. They're
					 * probably just padding...
					 */
					if (suffix[0] == '\0')
						continue;

					if (!check_dnssl_presence(iface->AdvDNSSLList, suffix)) {
						flog(LOG_WARNING, "DNSSL suffix %s received on %s from %s is not advertised by us",
							suffix, iface->Name, addr_str);
					}

					suffix[0] = '\0';
					continue;
				}

				/*
				 * 1) must not overflow int: label + 2, offset + label_len
				 * 2) last byte of dnssli_suffix must not overflow opt_str + len
				 */
				if ((sizeof(suffix) - strlen(suffix)) < (label_len + 2) ||
				    label_len > label_len + 2 ||
				    &dnsslinfo->nd_opt_dnssli_suffixes[offset+label_len] - (char*)opt_str >= len ||
				    offset + label_len < offset) {
					flog(LOG_ERR, "oversized suffix in DNSSL option on %s from %s",
							iface->Name, addr_str);
					break;
				}

				if (suffix[0] != '\0')
					strcat(suffix, ".");
				strncat(suffix, &dnsslinfo->nd_opt_dnssli_suffixes[offset], label_len);
				offset += label_len;
			}
			break;
		default:
			dlog(LOG_DEBUG, 1, "unknown option %d in RA on %s from %s",
				(int)*opt_str, iface->Name, addr_str);
			break;
		}

		len -= optlen;
		opt_str += optlen;
	}
}