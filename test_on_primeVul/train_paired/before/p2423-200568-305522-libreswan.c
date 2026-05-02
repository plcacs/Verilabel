void process_packet(struct msg_digest **mdp)
{
	struct msg_digest *md = *mdp;
	struct state *st = NULL;
	int maj, min;
	enum state_kind from_state = STATE_UNDEFINED;   /* state we started in */
	struct isakmp_hdr *hdr;

#define SEND_NOTIFICATION(t) { \
		if (st) \
			send_notification_from_state(st, from_state, t); \
		else \
			send_notification_from_md(md, t); }

	if (!in_struct(&md->hdr, &isakmp_hdr_desc, &md->packet_pbs,
		       &md->message_pbs)) {
		/* Identify specific failures:
		 * - bad ISAKMP major/minor version numbers
		 */
		if (md->packet_pbs.roof - md->packet_pbs.cur >=
		    (ptrdiff_t)isakmp_hdr_desc.size) {
			hdr = (struct isakmp_hdr *)md->packet_pbs.cur;
			maj = (hdr->isa_version >> ISA_MAJ_SHIFT);
			min = (hdr->isa_version & ISA_MIN_MASK);

			if ( maj != ISAKMP_MAJOR_VERSION &&
			     maj != IKEv2_MAJOR_VERSION) {
				/* We don't know IKEv3+ */
				SEND_NOTIFICATION(INVALID_MAJOR_VERSION);
				return;
			} else if (maj == ISAKMP_MAJOR_VERSION && min !=
				   ISAKMP_MINOR_VERSION) {
					/* We only know IKEv1 1.0 */
					SEND_NOTIFICATION(INVALID_MINOR_VERSION);
					return;
			}
			/* As per RFC 4306/5996, accept unknown IKEv2 minor */
		} else {
			libreswan_log("received packet size (%lu) is smaller than "
				"an IKE header - packet dropped", md->packet_pbs.roof - md->packet_pbs.cur);
			SEND_NOTIFICATION(PAYLOAD_MALFORMED);
			return;
		}
	}

	if (md->packet_pbs.roof < md->message_pbs.roof) {
		libreswan_log(
			"received packet size (%u) is smaller than from "
			"size specified in ISAKMP HDR (%u) - packet dropped",
			(unsigned) pbs_room(&md->packet_pbs),
			md->hdr.isa_length);
		/* abort processing corrupt packet */
		return;
	} else if (md->packet_pbs.roof > md->message_pbs.roof) {
		/*
		 * Some (old?) versions of the Cisco VPN client send an additional
		 * 16 bytes of zero bytes - Complain but accept it
		 */
		DBG(DBG_CONTROL, {
			DBG_log(
			"size (%u) in received packet is larger than the size "
			"specified in ISAKMP HDR (%u) - ignoring extraneous bytes",
			(unsigned) pbs_room(&md->packet_pbs),
			md->hdr.isa_length);
			DBG_dump("extraneous bytes:", md->message_pbs.roof,
				md->packet_pbs.roof - md->message_pbs.roof);
		/* continue */
		});
	}

	maj = (md->hdr.isa_version >> ISA_MAJ_SHIFT);
	min = (md->hdr.isa_version & ISA_MIN_MASK);

	DBG(DBG_CONTROL,
	    DBG_log(
		    " processing version=%u.%u packet with exchange type=%s (%d)",
		    maj, min,
		    enum_name(&exchange_names_ikev1orv2, md->hdr.isa_xchg),
		    md->hdr.isa_xchg));

	switch (maj) {
	case ISAKMP_MAJOR_VERSION:
		process_v1_packet(mdp);
		break;

	case IKEv2_MAJOR_VERSION:
		process_v2_packet(mdp);
		break;

	default:
		/*
		 * We should never get here? - above we only accept v1 or v2
		 */
		libreswan_log("Unexpected IKE major '%d'",maj);
		SEND_NOTIFICATION(PAYLOAD_MALFORMED);
		return;
	}
}