static int bgp_capability_msg_parse(struct peer *peer, uint8_t *pnt,
				    bgp_size_t length)
{
	uint8_t *end;
	struct capability_mp_data mpc;
	struct capability_header *hdr;
	uint8_t action;
	iana_afi_t pkt_afi;
	afi_t afi;
	iana_safi_t pkt_safi;
	safi_t safi;

	end = pnt + length;

	while (pnt < end) {
		/* We need at least action, capability code and capability
		 * length. */
		if (pnt + 3 > end) {
			zlog_info("%s Capability length error", peer->host);
			bgp_notify_send(peer, BGP_NOTIFY_CEASE,
					BGP_NOTIFY_SUBCODE_UNSPECIFIC);
			return BGP_Stop;
		}
		action = *pnt;
		hdr = (struct capability_header *)(pnt + 1);

		/* Action value check.  */
		if (action != CAPABILITY_ACTION_SET
		    && action != CAPABILITY_ACTION_UNSET) {
			zlog_info("%s Capability Action Value error %d",
				  peer->host, action);
			bgp_notify_send(peer, BGP_NOTIFY_CEASE,
					BGP_NOTIFY_SUBCODE_UNSPECIFIC);
			return BGP_Stop;
		}

		if (bgp_debug_neighbor_events(peer))
			zlog_debug(
				"%s CAPABILITY has action: %d, code: %u, length %u",
				peer->host, action, hdr->code, hdr->length);

		/* Capability length check. */
		if ((pnt + hdr->length + 3) > end) {
			zlog_info("%s Capability length error", peer->host);
			bgp_notify_send(peer, BGP_NOTIFY_CEASE,
					BGP_NOTIFY_SUBCODE_UNSPECIFIC);
			return BGP_Stop;
		}

		/* Fetch structure to the byte stream. */
		memcpy(&mpc, pnt + 3, sizeof(struct capability_mp_data));
		pnt += hdr->length + 3;

		/* We know MP Capability Code. */
		if (hdr->code == CAPABILITY_CODE_MP) {
			pkt_afi = ntohs(mpc.afi);
			pkt_safi = mpc.safi;

			/* Ignore capability when override-capability is set. */
			if (CHECK_FLAG(peer->flags,
				       PEER_FLAG_OVERRIDE_CAPABILITY))
				continue;

			/* Convert AFI, SAFI to internal values. */
			if (bgp_map_afi_safi_iana2int(pkt_afi, pkt_safi, &afi,
						      &safi)) {
				if (bgp_debug_neighbor_events(peer))
					zlog_debug(
						"%s Dynamic Capability MP_EXT afi/safi invalid (%s/%s)",
						peer->host,
						iana_afi2str(pkt_afi),
						iana_safi2str(pkt_safi));
				continue;
			}

			/* Address family check.  */
			if (bgp_debug_neighbor_events(peer))
				zlog_debug(
					"%s CAPABILITY has %s MP_EXT CAP for afi/safi: %s/%s",
					peer->host,
					action == CAPABILITY_ACTION_SET
						? "Advertising"
						: "Removing",
					iana_afi2str(pkt_afi),
					iana_safi2str(pkt_safi));

			if (action == CAPABILITY_ACTION_SET) {
				peer->afc_recv[afi][safi] = 1;
				if (peer->afc[afi][safi]) {
					peer->afc_nego[afi][safi] = 1;
					bgp_announce_route(peer, afi, safi,
							   false);
				}
			} else {
				peer->afc_recv[afi][safi] = 0;
				peer->afc_nego[afi][safi] = 0;

				if (peer_active_nego(peer))
					bgp_clear_route(peer, afi, safi);
				else
					return BGP_Stop;
			}
		} else {
			flog_warn(
				EC_BGP_UNRECOGNIZED_CAPABILITY,
				"%s unrecognized capability code: %d - ignored",
				peer->host, hdr->code);
		}
	}

	/* No FSM action necessary */
	return BGP_PACKET_NOOP;
}