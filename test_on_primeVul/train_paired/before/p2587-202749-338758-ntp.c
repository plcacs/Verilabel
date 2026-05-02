receive(
	struct recvbuf *rbufp
	)
{
	register struct peer *peer;	/* peer structure pointer */
	register struct pkt *pkt;	/* receive packet pointer */
	u_char	hisversion;		/* packet version */
	u_char	hisleap;		/* packet leap indicator */
	u_char	hismode;		/* packet mode */
	u_char	hisstratum;		/* packet stratum */
	u_short	restrict_mask;		/* restrict bits */
	int	has_mac;		/* length of MAC field */
	int	authlen;		/* offset of MAC field */
	int	is_authentic = 0;	/* cryptosum ok */
	int	retcode = AM_NOMATCH;	/* match code */
	keyid_t	skeyid = 0;		/* key IDs */
	u_int32	opcode = 0;		/* extension field opcode */
	sockaddr_u *dstadr_sin; 	/* active runway */
	struct peer *peer2;		/* aux peer structure pointer */
	endpt *	match_ep;		/* newpeer() local address */
	l_fp	p_org;			/* origin timestamp */
	l_fp	p_rec;			/* receive timestamp */
	l_fp	p_xmt;			/* transmit timestamp */
#ifdef AUTOKEY
	char	hostname[NTP_MAXSTRLEN + 1];
	char	*groupname = NULL;
	struct autokey *ap;		/* autokey structure pointer */
	int	rval;			/* cookie snatcher */
	keyid_t	pkeyid = 0, tkeyid = 0;	/* key IDs */
#endif	/* AUTOKEY */
#ifdef HAVE_NTP_SIGND
	static unsigned char zero_key[16];
#endif /* HAVE_NTP_SIGND */

	/*
	 * Monitor the packet and get restrictions. Note that the packet
	 * length for control and private mode packets must be checked
	 * by the service routines. Some restrictions have to be handled
	 * later in order to generate a kiss-o'-death packet.
	 */
	/*
	 * Bogus port check is before anything, since it probably
	 * reveals a clogging attack.
	 */
	sys_received++;
	if (0 == SRCPORT(&rbufp->recv_srcadr)) {
		sys_badlength++;
		return;				/* bogus port */
	}
	restrict_mask = restrictions(&rbufp->recv_srcadr);
	DPRINTF(2, ("receive: at %ld %s<-%s flags %x restrict %03x\n",
		    current_time, stoa(&rbufp->dstadr->sin),
		    stoa(&rbufp->recv_srcadr),
		    rbufp->dstadr->flags, restrict_mask));
	pkt = &rbufp->recv_pkt;
	hisversion = PKT_VERSION(pkt->li_vn_mode);
	hisleap = PKT_LEAP(pkt->li_vn_mode);
	hismode = (int)PKT_MODE(pkt->li_vn_mode);
	hisstratum = PKT_TO_STRATUM(pkt->stratum);
	if (restrict_mask & RES_IGNORE) {
		sys_restricted++;
		return;				/* ignore everything */
	}
	if (hismode == MODE_PRIVATE) {
		if (!ntp_mode7 || (restrict_mask & RES_NOQUERY)) {
			sys_restricted++;
			return;			/* no query private */
		}
		process_private(rbufp, ((restrict_mask &
		    RES_NOMODIFY) == 0));
		return;
	}
	if (hismode == MODE_CONTROL) {
		if (restrict_mask & RES_NOQUERY) {
			sys_restricted++;
			return;			/* no query control */
		}
		process_control(rbufp, restrict_mask);
		return;
	}
	if (restrict_mask & RES_DONTSERVE) {
		sys_restricted++;
		return;				/* no time serve */
	}

	/*
	 * This is for testing. If restricted drop ten percent of
	 * surviving packets.
	 */
	if (restrict_mask & RES_FLAKE) {
		if ((double)ntp_random() / 0x7fffffff < .1) {
			sys_restricted++;
			return;			/* no flakeway */
		}
	}

	/*
	 * Version check must be after the query packets, since they
	 * intentionally use an early version.
	 */
	if (hisversion == NTP_VERSION) {
		sys_newversion++;		/* new version */
	} else if (!(restrict_mask & RES_VERSION) && hisversion >=
	    NTP_OLDVERSION) {
		sys_oldversion++;		/* previous version */
	} else {
		sys_badlength++;
		return;				/* old version */
	}

	/*
	 * Figure out his mode and validate the packet. This has some
	 * legacy raunch that probably should be removed. In very early
	 * NTP versions mode 0 was equivalent to what later versions
	 * would interpret as client mode.
	 */
	if (hismode == MODE_UNSPEC) {
		if (hisversion == NTP_OLDVERSION) {
			hismode = MODE_CLIENT;
		} else {
			sys_badlength++;
			return;                 /* invalid mode */
		}
	}

	/*
	 * Parse the extension field if present. We figure out whether
	 * an extension field is present by measuring the MAC size. If
	 * the number of words following the packet header is 0, no MAC
	 * is present and the packet is not authenticated. If 1, the
	 * packet is a crypto-NAK; if 3, the packet is authenticated
	 * with DES; if 5, the packet is authenticated with MD5; if 6,
	 * the packet is authenticated with SHA. If 2 or * 4, the packet
	 * is a runt and discarded forthwith. If greater than 6, an
	 * extension field is present, so we subtract the length of the
	 * field and go around again.
	 */
	authlen = LEN_PKT_NOMAC;
	has_mac = rbufp->recv_length - authlen;
	while (has_mac > 0) {
		u_int32	len;
#ifdef AUTOKEY
		u_int32	hostlen;
		struct exten *ep;
#endif /*AUTOKEY */

		if (has_mac % 4 != 0 || has_mac < (int)MIN_MAC_LEN) {
			sys_badlength++;
			return;			/* bad length */
		}
		if (has_mac <= (int)MAX_MAC_LEN) {
			skeyid = ntohl(((u_int32 *)pkt)[authlen / 4]);
			break;

		} else {
			opcode = ntohl(((u_int32 *)pkt)[authlen / 4]);
			len = opcode & 0xffff;
			if (len % 4 != 0 || len < 4 || (int)len +
			    authlen > rbufp->recv_length) {
				sys_badlength++;
				return;		/* bad length */
			}
#ifdef AUTOKEY
			/*
			 * Extract calling group name for later.  If
			 * sys_groupname is non-NULL, there must be
			 * a group name provided to elicit a response.
			 */
			if ((opcode & 0x3fff0000) == CRYPTO_ASSOC &&
			    sys_groupname != NULL) {
				ep = (struct exten *)&((u_int32 *)pkt)[authlen / 4];
				hostlen = ntohl(ep->vallen);
				if (hostlen >= sizeof(hostname) ||
				    hostlen > len -
				    offsetof(struct exten, pkt)) {
					sys_badlength++;
					return;		/* bad length */
				}
				memcpy(hostname, &ep->pkt, hostlen);
				hostname[hostlen] = '\0';
				groupname = strchr(hostname, '@');
				if (groupname == NULL) {
					sys_declined++;
					return;
				}
				groupname++;
			}
#endif /* AUTOKEY */
			authlen += len;
			has_mac -= len;
		}
	}

	/*
	 * If has_mac is < 0 we had a malformed packet.
	 */
	if (has_mac < 0) {
		sys_badlength++;
		return;		/* bad length */
	}

	/*
	 * If authentication required, a MAC must be present.
	 */
	if (restrict_mask & RES_DONTTRUST && has_mac == 0) {
		sys_restricted++;
		return;				/* access denied */
	}

	/*
	 * Update the MRU list and finger the cloggers. It can be a
	 * little expensive, so turn it off for production use.
	 * RES_LIMITED and RES_KOD will be cleared in the returned
	 * restrict_mask unless one or both actions are warranted.
	 */
	restrict_mask = ntp_monitor(rbufp, restrict_mask);
	if (restrict_mask & RES_LIMITED) {
		sys_limitrejected++;
		if (!(restrict_mask & RES_KOD) || MODE_BROADCAST ==
		    hismode || MODE_SERVER == hismode) {
			if (MODE_SERVER == hismode)
				DPRINTF(1, ("Possibly self-induced rate limiting of MODE_SERVER from %s\n",
					stoa(&rbufp->recv_srcadr)));
			return;			/* rate exceeded */
		}
		if (hismode == MODE_CLIENT)
			fast_xmit(rbufp, MODE_SERVER, skeyid,
			    restrict_mask);
		else
			fast_xmit(rbufp, MODE_ACTIVE, skeyid,
			    restrict_mask);
		return;				/* rate exceeded */
	}
	restrict_mask &= ~RES_KOD;

	/*
	 * We have tossed out as many buggy packets as possible early in
	 * the game to reduce the exposure to a clogging attack. Now we
	 * have to burn some cycles to find the association and
	 * authenticate the packet if required. Note that we burn only
	 * digest cycles, again to reduce exposure. There may be no
	 * matching association and that's okay.
	 *
	 * More on the autokey mambo. Normally the local interface is
	 * found when the association was mobilized with respect to a
	 * designated remote address. We assume packets arriving from
	 * the remote address arrive via this interface and the local
	 * address used to construct the autokey is the unicast address
	 * of the interface. However, if the sender is a broadcaster,
	 * the interface broadcast address is used instead.
	 * Notwithstanding this technobabble, if the sender is a
	 * multicaster, the broadcast address is null, so we use the
	 * unicast address anyway. Don't ask.
	 */
	peer = findpeer(rbufp,  hismode, &retcode);
	dstadr_sin = &rbufp->dstadr->sin;
	NTOHL_FP(&pkt->org, &p_org);
	NTOHL_FP(&pkt->rec, &p_rec);
	NTOHL_FP(&pkt->xmt, &p_xmt);

	/*
	 * Authentication is conditioned by three switches:
	 *
	 * NOPEER  (RES_NOPEER) do not mobilize an association unless
	 *         authenticated
	 * NOTRUST (RES_DONTTRUST) do not allow access unless
	 *         authenticated (implies NOPEER)
	 * enable  (sys_authenticate) master NOPEER switch, by default
	 *         on
	 *
	 * The NOPEER and NOTRUST can be specified on a per-client basis
	 * using the restrict command. The enable switch if on implies
	 * NOPEER for all clients. There are four outcomes:
	 *
	 * NONE    The packet has no MAC.
	 * OK      the packet has a MAC and authentication succeeds
	 * ERROR   the packet has a MAC and authentication fails
	 * CRYPTO  crypto-NAK. The MAC has four octets only.
	 *
	 * Note: The AUTH(x, y) macro is used to filter outcomes. If x
	 * is zero, acceptable outcomes of y are NONE and OK. If x is
	 * one, the only acceptable outcome of y is OK.
	 */

	if (has_mac == 0) {
		restrict_mask &= ~RES_MSSNTP;
		is_authentic = AUTH_NONE; /* not required */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d len %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode,
			    authlen);
#endif
	} else if (has_mac == 4) {
		restrict_mask &= ~RES_MSSNTP;
		is_authentic = AUTH_CRYPTO; /* crypto-NAK */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d keyid %08x len %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, skeyid,
			    authlen + has_mac, is_authentic);
#endif

#ifdef HAVE_NTP_SIGND
		/*
		 * If the signature is 20 bytes long, the last 16 of
		 * which are zero, then this is a Microsoft client
		 * wanting AD-style authentication of the server's
		 * reply.
		 *
		 * This is described in Microsoft's WSPP docs, in MS-SNTP:
		 * http://msdn.microsoft.com/en-us/library/cc212930.aspx
		 */
	} else if (has_mac == MAX_MD5_LEN && (restrict_mask & RES_MSSNTP) &&
	   (retcode == AM_FXMIT || retcode == AM_NEWPASS) &&
	   (memcmp(zero_key, (char *)pkt + authlen + 4, MAX_MD5_LEN - 4) ==
	   0)) {
		is_authentic = AUTH_NONE;
#endif /* HAVE_NTP_SIGND */

	} else {
		restrict_mask &= ~RES_MSSNTP;
#ifdef AUTOKEY
		/*
		 * For autokey modes, generate the session key
		 * and install in the key cache. Use the socket
		 * broadcast or unicast address as appropriate.
		 */
		if (crypto_flags && skeyid > NTP_MAXKEY) {

			/*
			 * More on the autokey dance (AKD). A cookie is
			 * constructed from public and private values.
			 * For broadcast packets, the cookie is public
			 * (zero). For packets that match no
			 * association, the cookie is hashed from the
			 * addresses and private value. For server
			 * packets, the cookie was previously obtained
			 * from the server. For symmetric modes, the
			 * cookie was previously constructed using an
			 * agreement protocol; however, should PKI be
			 * unavailable, we construct a fake agreement as
			 * the EXOR of the peer and host cookies.
			 *
			 * hismode	ephemeral	persistent
			 * =======================================
			 * active	0		cookie#
			 * passive	0%		cookie#
			 * client	sys cookie	0%
			 * server	0%		sys cookie
			 * broadcast	0		0
			 *
			 * # if unsync, 0
			 * % can't happen
			 */
			if (has_mac < (int)MAX_MD5_LEN) {
				sys_badauth++;
				return;
			}
			if (hismode == MODE_BROADCAST) {

				/*
				 * For broadcaster, use the interface
				 * broadcast address when available;
				 * otherwise, use the unicast address
				 * found when the association was
				 * mobilized. However, if this is from
				 * the wildcard interface, game over.
				 */
				if (crypto_flags && rbufp->dstadr ==
				    ANY_INTERFACE_CHOOSE(&rbufp->recv_srcadr)) {
					sys_restricted++;
					return;	     /* no wildcard */
				}
				pkeyid = 0;
				if (!SOCK_UNSPEC(&rbufp->dstadr->bcast))
					dstadr_sin =
					    &rbufp->dstadr->bcast;
			} else if (peer == NULL) {
				pkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin, 0,
				    sys_private, 0);
			} else {
				pkeyid = peer->pcookie;
			}

			/*
			 * The session key includes both the public
			 * values and cookie. In case of an extension
			 * field, the cookie used for authentication
			 * purposes is zero. Note the hash is saved for
			 * use later in the autokey mambo.
			 */
			if (authlen > (int)LEN_PKT_NOMAC && pkeyid != 0) {
				session_key(&rbufp->recv_srcadr,
				    dstadr_sin, skeyid, 0, 2);
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    skeyid, pkeyid, 0);
			} else {
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    skeyid, pkeyid, 2);
			}

		}
#endif	/* AUTOKEY */

		/*
		 * Compute the cryptosum. Note a clogging attack may
		 * succeed in bloating the key cache. If an autokey,
		 * purge it immediately, since we won't be needing it
		 * again. If the packet is authentic, it can mobilize an
		 * association. Note that there is no key zero.
		 */
		if (!authdecrypt(skeyid, (u_int32 *)pkt, authlen,
		    has_mac))
			is_authentic = AUTH_ERROR;
		else
			is_authentic = AUTH_OK;
#ifdef AUTOKEY
		if (crypto_flags && skeyid > NTP_MAXKEY)
			authtrust(skeyid, 0);
#endif	/* AUTOKEY */
#ifdef DEBUG
		if (debug)
			printf(
			    "receive: at %ld %s<-%s mode %d keyid %08x len %d auth %d\n",
			    current_time, stoa(dstadr_sin),
			    stoa(&rbufp->recv_srcadr), hismode, skeyid,
			    authlen + has_mac, is_authentic);
#endif
	}

	/*
	 * The association matching rules are implemented by a set of
	 * routines and an association table. A packet matching an
	 * association is processed by the peer process for that
	 * association. If there are no errors, an ephemeral association
	 * is mobilized: a broadcast packet mobilizes a broadcast client
	 * aassociation; a manycast server packet mobilizes a manycast
	 * client association; a symmetric active packet mobilizes a
	 * symmetric passive association.
	 */
	switch (retcode) {

	/*
	 * This is a client mode packet not matching any association. If
	 * an ordinary client, simply toss a server mode packet back
	 * over the fence. If a manycast client, we have to work a
	 * little harder.
	 */
	case AM_FXMIT:

		/*
		 * If authentication OK, send a server reply; otherwise,
		 * send a crypto-NAK.
		 */
		if (!(rbufp->dstadr->flags & INT_MCASTOPEN)) {
			if (AUTH(restrict_mask & RES_DONTTRUST,
			   is_authentic)) {
				fast_xmit(rbufp, MODE_SERVER, skeyid,
				    restrict_mask);
			} else if (is_authentic == AUTH_ERROR) {
				fast_xmit(rbufp, MODE_SERVER, 0,
				    restrict_mask);
				sys_badauth++;
			} else {
				sys_restricted++;
			}
			return;			/* hooray */
		}

		/*
		 * This must be manycast. Do not respond if not
		 * configured as a manycast server.
		 */
		if (!sys_manycastserver) {
			sys_restricted++;
			return;			/* not enabled */
		}

#ifdef AUTOKEY
		/*
		 * Do not respond if not the same group.
		 */
		if (group_test(groupname, NULL)) {
			sys_declined++;
			return;
		}
#endif /* AUTOKEY */

		/*
		 * Do not respond if we are not synchronized or our
		 * stratum is greater than the manycaster or the
		 * manycaster has already synchronized to us.
		 */
		if (sys_leap == LEAP_NOTINSYNC || sys_stratum >=
		    hisstratum || (!sys_cohort && sys_stratum ==
		    hisstratum + 1) || rbufp->dstadr->addr_refid ==
		    pkt->refid) {
			sys_declined++;
			return;			/* no help */
		}

		/*
		 * Respond only if authentication succeeds. Don't do a
		 * crypto-NAK, as that would not be useful.
		 */
		if (AUTH(restrict_mask & RES_DONTTRUST, is_authentic))
			fast_xmit(rbufp, MODE_SERVER, skeyid,
			    restrict_mask);
		return;				/* hooray */

	/*
	 * This is a server mode packet returned in response to a client
	 * mode packet sent to a multicast group address (for
	 * manycastclient) or to a unicast address (for pool). The
	 * origin timestamp is a good nonce to reliably associate the
	 * reply with what was sent. If there is no match, that's
	 * curious and could be an intruder attempting to clog, so we
	 * just ignore it.
	 *
	 * If the packet is authentic and the manycastclient or pool
	 * association is found, we mobilize a client association and
	 * copy pertinent variables from the manycastclient or pool
	 * association to the new client association. If not, just
	 * ignore the packet.
	 *
	 * There is an implosion hazard at the manycast client, since
	 * the manycast servers send the server packet immediately. If
	 * the guy is already here, don't fire up a duplicate.
	 */
	case AM_MANYCAST:

#ifdef AUTOKEY
		/*
		 * Do not respond if not the same group.
		 */
		if (group_test(groupname, NULL)) {
			sys_declined++;
			return;
		}
#endif /* AUTOKEY */
		if ((peer2 = findmanycastpeer(rbufp)) == NULL) {
			sys_restricted++;
			return;			/* not enabled */
		}
		if (!AUTH((!(peer2->cast_flags & MDF_POOL) &&
		    sys_authenticate) | (restrict_mask & (RES_NOPEER |
		    RES_DONTTRUST)), is_authentic)) {
			sys_restricted++;
			return;			/* access denied */
		}

		/*
		 * Do not respond if unsynchronized or stratum is below
		 * the floor or at or above the ceiling.
		 */
		if (hisleap == LEAP_NOTINSYNC || hisstratum <
		    sys_floor || hisstratum >= sys_ceiling) {
			sys_declined++;
			return;			/* no help */
		}
		peer = newpeer(&rbufp->recv_srcadr, NULL, rbufp->dstadr,
			       MODE_CLIENT, hisversion, peer2->minpoll,
			       peer2->maxpoll, FLAG_PREEMPT |
			       (FLAG_IBURST & peer2->flags), MDF_UCAST |
			       MDF_UCLNT, 0, skeyid, sys_ident);
		if (NULL == peer) {
			sys_declined++;
			return;			/* ignore duplicate  */
		}

		/*
		 * After each ephemeral pool association is spun,
		 * accelerate the next poll for the pool solicitor so
		 * the pool will fill promptly.
		 */
		if (peer2->cast_flags & MDF_POOL)
			peer2->nextdate = current_time + 1;

		/*
		 * Further processing of the solicitation response would
		 * simply detect its origin timestamp as bogus for the
		 * brand-new association (it matches the prototype
		 * association) and tinker with peer->nextdate delaying
		 * first sync.
		 */
		return;		/* solicitation response handled */

	/*
	 * This is the first packet received from a broadcast server. If
	 * the packet is authentic and we are enabled as broadcast
	 * client, mobilize a broadcast client association. We don't
	 * kiss any frogs here.
	 */
	case AM_NEWBCL:

#ifdef AUTOKEY
		/*
		 * Do not respond if not the same group.
		 */
		if (group_test(groupname, sys_ident)) {
			sys_declined++;
			return;
		}
#endif /* AUTOKEY */
		if (sys_bclient == 0) {
			sys_restricted++;
			return;			/* not enabled */
		}
		if (!AUTH(sys_authenticate | (restrict_mask &
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic)) {
			sys_restricted++;
			return;			/* access denied */
		}

		/*
		 * Do not respond if unsynchronized or stratum is below
		 * the floor or at or above the ceiling.
		 */
		if (hisleap == LEAP_NOTINSYNC || hisstratum <
		    sys_floor || hisstratum >= sys_ceiling) {
			sys_declined++;
			return;			/* no help */
		}

#ifdef AUTOKEY
		/*
		 * Do not respond if Autokey and the opcode is not a
		 * CRYPTO_ASSOC response with association ID.
		 */
		if (crypto_flags && skeyid > NTP_MAXKEY && (opcode &
		    0xffff0000) != (CRYPTO_ASSOC | CRYPTO_RESP)) {
			sys_declined++;
			return;			/* protocol error */
		}
#endif	/* AUTOKEY */

		/*
		 * Broadcasts received via a multicast address may
		 * arrive after a unicast volley has begun
		 * with the same remote address.  newpeer() will not
		 * find duplicate associations on other local endpoints
		 * if a non-NULL endpoint is supplied.  multicastclient
		 * ephemeral associations are unique across all local
		 * endpoints.
		 */
		if (!(INT_MCASTOPEN & rbufp->dstadr->flags))
			match_ep = rbufp->dstadr;
		else
			match_ep = NULL;

		/*
		 * Determine whether to execute the initial volley.
		 */
		if (sys_bdelay != 0) {
#ifdef AUTOKEY
			/*
			 * If a two-way exchange is not possible,
			 * neither is Autokey.
			 */
			if (crypto_flags && skeyid > NTP_MAXKEY) {
				sys_restricted++;
				return;		/* no autokey */
			}
#endif	/* AUTOKEY */

			/*
			 * Do not execute the volley. Start out in
			 * broadcast client mode.
			 */
			peer = newpeer(&rbufp->recv_srcadr, NULL,
			    match_ep, MODE_BCLIENT, hisversion,
			    pkt->ppoll, pkt->ppoll, FLAG_PREEMPT,
			    MDF_BCLNT, 0, skeyid, sys_ident);
			if (NULL == peer) {
				sys_restricted++;
				return;		/* ignore duplicate */

			} else {
				peer->delay = sys_bdelay;
			}
			break;
		}

		/*
		 * Execute the initial volley in order to calibrate the
		 * propagation delay and run the Autokey protocol.
		 *
		 * Note that the minpoll is taken from the broadcast
		 * packet, normally 6 (64 s) and that the poll interval
		 * is fixed at this value.
		 */
		peer = newpeer(&rbufp->recv_srcadr, NULL, match_ep,
		    MODE_CLIENT, hisversion, pkt->ppoll, pkt->ppoll,
		    FLAG_BC_VOL | FLAG_IBURST | FLAG_PREEMPT, MDF_BCLNT,
		    0, skeyid, sys_ident);
		if (NULL == peer) {
			sys_restricted++;
			return;			/* ignore duplicate */
		}
#ifdef AUTOKEY
		if (skeyid > NTP_MAXKEY)
			crypto_recv(peer, rbufp);
#endif	/* AUTOKEY */

		return;				/* hooray */

	/*
	 * This is the first packet received from a symmetric active
	 * peer. If the packet is authentic and the first he sent,
	 * mobilize a passive association. If not, kiss the frog.
	 */
	case AM_NEWPASS:

#ifdef AUTOKEY
		/*
		 * Do not respond if not the same group.
		 */
		if (group_test(groupname, sys_ident)) {
			sys_declined++;
			return;
		}
#endif /* AUTOKEY */
		if (!AUTH(sys_authenticate | (restrict_mask &
		    (RES_NOPEER | RES_DONTTRUST)), is_authentic)) {

			/*
			 * If authenticated but cannot mobilize an
			 * association, send a symmetric passive
			 * response without mobilizing an association.
			 * This is for drat broken Windows clients. See
			 * Microsoft KB 875424 for preferred workaround.
			 */
			if (AUTH(restrict_mask & RES_DONTTRUST,
			    is_authentic)) {
				fast_xmit(rbufp, MODE_PASSIVE, skeyid,
				    restrict_mask);
				return;			/* hooray */
			}
			if (is_authentic == AUTH_ERROR) {
				fast_xmit(rbufp, MODE_ACTIVE, 0,
				    restrict_mask);
				sys_restricted++;
				return;
			}
		}

		/*
		 * Do not respond if synchronized and if stratum is
		 * below the floor or at or above the ceiling. Note,
		 * this allows an unsynchronized peer to synchronize to
		 * us. It would be very strange if he did and then was
		 * nipped, but that could only happen if we were
		 * operating at the top end of the range.  It also means
		 * we will spin an ephemeral association in response to
		 * MODE_ACTIVE KoDs, which will time out eventually.
		 */
		if (hisleap != LEAP_NOTINSYNC && (hisstratum <
		    sys_floor || hisstratum >= sys_ceiling)) {
			sys_declined++;
			return;			/* no help */
		}

		/*
		 * The message is correctly authenticated and allowed.
		 * Mobilize a symmetric passive association.
		 */
		if ((peer = newpeer(&rbufp->recv_srcadr, NULL,
		    rbufp->dstadr, MODE_PASSIVE, hisversion, pkt->ppoll,
		    NTP_MAXDPOLL, 0, MDF_UCAST, 0, skeyid,
		    sys_ident)) == NULL) {
			sys_declined++;
			return;			/* ignore duplicate */
		}
		break;


	/*
	 * Process regular packet. Nothing special.
	 */
	case AM_PROCPKT:

#ifdef AUTOKEY
		/*
		 * Do not respond if not the same group.
		 */
		if (group_test(groupname, peer->ident)) {
			sys_declined++;
			return;
		}
#endif /* AUTOKEY */
		break;

	/*
	 * A passive packet matches a passive association. This is
	 * usually the result of reconfiguring a client on the fly. As
	 * this association might be legitimate and this packet an
	 * attempt to deny service, just ignore it.
	 */
	case AM_ERR:
		sys_declined++;
		return;

	/*
	 * For everything else there is the bit bucket.
	 */
	default:
		sys_declined++;
		return;
	}

#ifdef AUTOKEY
	/*
	 * If the association is configured for Autokey, the packet must
	 * have a public key ID; if not, the packet must have a
	 * symmetric key ID.
	 */
	if (is_authentic != AUTH_CRYPTO && (((peer->flags &
	    FLAG_SKEY) && skeyid <= NTP_MAXKEY) || (!(peer->flags &
	    FLAG_SKEY) && skeyid > NTP_MAXKEY))) {
		sys_badauth++;
		return;
	}
#endif	/* AUTOKEY */
	peer->received++;
	peer->flash &= ~PKT_TEST_MASK;
	if (peer->flags & FLAG_XBOGUS) {
		peer->flags &= ~FLAG_XBOGUS;
		peer->flash |= TEST3;
	}

	/*
	 * Next comes a rigorous schedule of timestamp checking. If the
	 * transmit timestamp is zero, the server has not initialized in
	 * interleaved modes or is horribly broken.
	 */
	if (L_ISZERO(&p_xmt)) {
		peer->flash |= TEST3;			/* unsynch */

	/*
	 * If the transmit timestamp duplicates a previous one, the
	 * packet is a replay. This prevents the bad guys from replaying
	 * the most recent packet, authenticated or not.
	 */
	} else if (L_ISEQU(&peer->xmt, &p_xmt)) {
		peer->flash |= TEST1;			/* duplicate */
		peer->oldpkt++;
		return;

	/*
	 * If this is a broadcast mode packet, skip further checking. If
	 * an initial volley, bail out now and let the client do its
	 * stuff. If the origin timestamp is nonzero, this is an
	 * interleaved broadcast. so restart the protocol.
	 */
	} else if (hismode == MODE_BROADCAST) {
		if (!L_ISZERO(&p_org) && !(peer->flags & FLAG_XB)) {
			peer->flags |= FLAG_XB;
			peer->aorg = p_xmt;
			peer->borg = rbufp->recv_time;
			report_event(PEVNT_XLEAVE, peer, NULL);
			return;
		}

	/*
	 * Check for bogus packet in basic mode. If found, switch to
	 * interleaved mode and resynchronize, but only after confirming
	 * the packet is not bogus in symmetric interleaved mode.
	 */
	} else if (peer->flip == 0) {
		if (!L_ISEQU(&p_org, &peer->aorg)) {
			peer->bogusorg++;
			peer->flash |= TEST2;	/* bogus */
			if (!L_ISZERO(&peer->dst) && L_ISEQU(&p_org,
			    &peer->dst)) {
				peer->flip = 1;
				report_event(PEVNT_XLEAVE, peer, NULL);
			}
		} else {
			L_CLR(&peer->aorg);
		}

	/*
	 * Check for valid nonzero timestamp fields.
	 */
	} else if (L_ISZERO(&p_org) || L_ISZERO(&p_rec) ||
	    L_ISZERO(&peer->dst)) {
		peer->flash |= TEST3;		/* unsynch */

	/*
	 * Check for bogus packet in interleaved symmetric mode. This
	 * can happen if a packet is lost, duplicated or crossed. If
	 * found, flip and resynchronize.
	 */
	} else if (!L_ISZERO(&peer->dst) && !L_ISEQU(&p_org,
	    &peer->dst)) {
		peer->bogusorg++;
		peer->flags |= FLAG_XBOGUS;
		peer->flash |= TEST2;		/* bogus */
	}

	/*
	 * If this is a crypto_NAK, the server cannot authenticate a
	 * client packet. The server might have just changed keys. Clear
	 * the association and restart the protocol.
	 */
	if (is_authentic == AUTH_CRYPTO) {
		report_event(PEVNT_AUTH, peer, "crypto_NAK");
		peer->flash |= TEST5;		/* bad auth */
		peer->badauth++;
		if (peer->flags & FLAG_PREEMPT) {
			unpeer(peer);
			return;
		}
#ifdef AUTOKEY
		if (peer->crypto)
			peer_clear(peer, "AUTH");
#endif	/* AUTOKEY */
		return;

	/*
	 * If the digest fails or it's missing for authenticated
	 * associations, the client cannot authenticate a server
	 * reply to a client packet previously sent. The loopback check
	 * is designed to avoid a bait-and-switch attack, which was
	 * possible in past versions. If symmetric modes, return a
	 * crypto-NAK. The peer should restart the protocol.
	 */
	} else if (!AUTH(peer->keyid || has_mac ||
			 (restrict_mask & RES_DONTTRUST), is_authentic)) {
		report_event(PEVNT_AUTH, peer, "digest");
		peer->flash |= TEST5;		/* bad auth */
		peer->badauth++;
		if (has_mac &&
		    (hismode == MODE_ACTIVE || hismode == MODE_PASSIVE))
			fast_xmit(rbufp, MODE_ACTIVE, 0, restrict_mask);
		if (peer->flags & FLAG_PREEMPT) {
			unpeer(peer);
			return;
		}
#ifdef AUTOKEY
		if (peer->crypto)
			peer_clear(peer, "AUTH");
#endif	/* AUTOKEY */
		return;
	}

	/*
	 * Update the state variables.
	 */
	if (peer->flip == 0) {
		if (hismode != MODE_BROADCAST)
			peer->rec = p_xmt;
		peer->dst = rbufp->recv_time;
	}
	peer->xmt = p_xmt;

	/*
	 * Set the peer ppoll to the maximum of the packet ppoll and the
	 * peer minpoll. If a kiss-o'-death, set the peer minpoll to
	 * this maximum and advance the headway to give the sender some
	 * headroom. Very intricate.
	 */
	peer->ppoll = max(peer->minpoll, pkt->ppoll);
	if (hismode == MODE_SERVER && hisleap == LEAP_NOTINSYNC &&
	    hisstratum == STRATUM_UNSPEC && memcmp(&pkt->refid,
	    "RATE", 4) == 0) {
		peer->selbroken++;
		report_event(PEVNT_RATE, peer, NULL);
		if (pkt->ppoll > peer->minpoll)
			peer->minpoll = peer->ppoll;
		peer->burst = peer->retry = 0;
		peer->throttle = (NTP_SHIFT + 1) * (1 << peer->minpoll);
		poll_update(peer, pkt->ppoll);
		return;				/* kiss-o'-death */
	}

	/*
	 * That was hard and I am sweaty, but the packet is squeaky
	 * clean. Get on with real work.
	 */
	peer->timereceived = current_time;
	if (is_authentic == AUTH_OK)
		peer->flags |= FLAG_AUTHENTIC;
	else
		peer->flags &= ~FLAG_AUTHENTIC;

#ifdef AUTOKEY
	/*
	 * More autokey dance. The rules of the cha-cha are as follows:
	 *
	 * 1. If there is no key or the key is not auto, do nothing.
	 *
	 * 2. If this packet is in response to the one just previously
	 *    sent or from a broadcast server, do the extension fields.
	 *    Otherwise, assume bogosity and bail out.
	 *
	 * 3. If an extension field contains a verified signature, it is
	 *    self-authenticated and we sit the dance.
	 *
	 * 4. If this is a server reply, check only to see that the
	 *    transmitted key ID matches the received key ID.
	 *
	 * 5. Check to see that one or more hashes of the current key ID
	 *    matches the previous key ID or ultimate original key ID
	 *    obtained from the broadcaster or symmetric peer. If no
	 *    match, sit the dance and call for new autokey values.
	 *
	 * In case of crypto error, fire the orchestra, stop dancing and
	 * restart the protocol.
	 */
	if (peer->flags & FLAG_SKEY) {
		/*
		 * Decrement remaining autokey hashes. This isn't
		 * perfect if a packet is lost, but results in no harm.
		 */
		ap = (struct autokey *)peer->recval.ptr;
		if (ap != NULL) {
			if (ap->seq > 0)
				ap->seq--;
		}
		peer->flash |= TEST8;
		rval = crypto_recv(peer, rbufp);
		if (rval == XEVNT_OK) {
			peer->unreach = 0;
		} else {
			if (rval == XEVNT_ERR) {
				report_event(PEVNT_RESTART, peer,
				    "crypto error");
				peer_clear(peer, "CRYP");
				peer->flash |= TEST9;	/* bad crypt */
				if (peer->flags & FLAG_PREEMPT)
					unpeer(peer);
			}
			return;
		}

		/*
		 * If server mode, verify the receive key ID matches
		 * the transmit key ID.
		 */
		if (hismode == MODE_SERVER) {
			if (skeyid == peer->keyid)
				peer->flash &= ~TEST8;

		/*
		 * If an extension field is present, verify only that it
		 * has been correctly signed. We don't need a sequence
		 * check here, but the sequence continues.
		 */
		} else if (!(peer->flash & TEST8)) {
			peer->pkeyid = skeyid;

		/*
		 * Now the fun part. Here, skeyid is the current ID in
		 * the packet, pkeyid is the ID in the last packet and
		 * tkeyid is the hash of skeyid. If the autokey values
		 * have not been received, this is an automatic error.
		 * If so, check that the tkeyid matches pkeyid. If not,
		 * hash tkeyid and try again. If the number of hashes
		 * exceeds the number remaining in the sequence, declare
		 * a successful failure and refresh the autokey values.
		 */
		} else if (ap != NULL) {
			int i;

			for (i = 0; ; i++) {
				if (tkeyid == peer->pkeyid ||
				    tkeyid == ap->key) {
					peer->flash &= ~TEST8;
					peer->pkeyid = skeyid;
					ap->seq -= i;
					break;
				}
				if (i > ap->seq) {
					peer->crypto &=
					    ~CRYPTO_FLAG_AUTO;
					break;
				}
				tkeyid = session_key(
				    &rbufp->recv_srcadr, dstadr_sin,
				    tkeyid, pkeyid, 0);
			}
			if (peer->flash & TEST8)
				report_event(PEVNT_AUTH, peer, "keylist");
		}
		if (!(peer->crypto & CRYPTO_FLAG_PROV)) /* test 9 */
			peer->flash |= TEST8;	/* bad autokey */

		/*
		 * The maximum lifetime of the protocol is about one
		 * week before restarting the Autokey protocol to
		 * refresh certificates and leapseconds values.
		 */
		if (current_time > peer->refresh) {
			report_event(PEVNT_RESTART, peer,
			    "crypto refresh");
			peer_clear(peer, "TIME");
			return;
		}
	}
#endif	/* AUTOKEY */

	/*
	 * The dance is complete and the flash bits have been lit. Toss
	 * the packet over the fence for processing, which may light up
	 * more flashers.
	 */
	process_packet(peer, pkt, rbufp->recv_length);

	/*
	 * In interleaved mode update the state variables. Also adjust the
	 * transmit phase to avoid crossover.
	 */
	if (peer->flip != 0) {
		peer->rec = p_rec;
		peer->dst = rbufp->recv_time;
		if (peer->nextdate - current_time < (1U << min(peer->ppoll,
		    peer->hpoll)) / 2)
			peer->nextdate++;
		else
			peer->nextdate--;
	}
}