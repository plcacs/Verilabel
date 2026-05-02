process_rs(struct Interface *iface, unsigned char *msg, int len,
	struct sockaddr_in6 *addr)
{
	double delay;
	double next;
	struct timeval tv;
	uint8_t *opt_str;

	/* validation */
	len -= sizeof(struct nd_router_solicit);

	opt_str = (uint8_t *)(msg + sizeof(struct nd_router_solicit));

	while (len > 0)
	{
		int optlen;

		if (len < 2)
		{
			flog(LOG_WARNING, "trailing garbage in RS");
			return;
		}

		optlen = (opt_str[1] << 3);

		if (optlen == 0)
		{
			flog(LOG_WARNING, "zero length option in RS");
			return;
		}
		else if (optlen > len)
		{
			flog(LOG_WARNING, "option length greater than total length in RS");
			return;
		}

		if (*opt_str == ND_OPT_SOURCE_LINKADDR &&
		    IN6_IS_ADDR_UNSPECIFIED(&addr->sin6_addr)) {
			flog(LOG_WARNING, "received icmpv6 RS packet with unspecified source address and there is a lladdr option");
			return;
		}

		len -= optlen;
		opt_str += optlen;
	}

	gettimeofday(&tv, NULL);

	delay = MAX_RA_DELAY_TIME * rand() / (RAND_MAX +1.0);

	if (iface->UnicastOnly) {
		send_ra_forall(iface, &addr->sin6_addr);
	}
	else if ( timevaldiff(&tv, &iface->last_multicast) / 1000.0 < iface->MinDelayBetweenRAs ) {
		/* last RA was sent only a few moments ago, don't send another immediately. */
		next = iface->MinDelayBetweenRAs - (tv.tv_sec + tv.tv_usec / 1000000.0) + (iface->last_multicast.tv_sec + iface->last_multicast.tv_usec / 1000000.0) + delay/1000.0;
		iface->next_multicast = next_timeval(next);
	}
	else {
		/* no RA sent in a while, send a multicast reply */
		send_ra_forall(iface, NULL);
		next = rand_between(iface->MinRtrAdvInterval, iface->MaxRtrAdvInterval);
		iface->next_multicast = next_timeval(next);
	}
}