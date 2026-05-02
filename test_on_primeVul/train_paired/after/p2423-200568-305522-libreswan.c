void process_packet(struct msg_digest **mdp)
{
	struct msg_digest *md = *mdp;
	struct state *st = NULL;
	int vmaj, vmin;
	enum state_kind from_state = STATE_UNDEFINED;   /* state we started in */

#define SEND_NOTIFICATION(t) { \
		if (st) \
			send_notification_from_state(st, from_state, t); \
		else \
			send_notification_from_md(md, t); }

	if (!in_struct(&md->hdr, &isakmp_hdr_desc, &md->packet_pbs,
		       &md->message_pbs)) {
		/*
		 * The packet was very badly mangled. We can't be sure of any
		 * content - not even to look for major version number!
		 * So we'll just silently drop it
		 */
		libreswan_log("Received packet with mangled IKE header - dropped");
		SEND_NOTIFICATION(PAYLOAD_MALFORMED);
		return;
	}

	if (md->packet_pbs.roof < md->message_pbs.roof) {
		/* I don't think this can happen if in_struct() did not fail */
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
		});
	}

	vmaj = md->hdr.isa_version >> ISA_MAJ_SHIFT;
	vmin = md->hdr.isa_version & ISA_MIN_MASK;

	switch (vmaj) {
	case ISAKMP_MAJOR_VERSION:
		if (vmin > ISAKMP_MINOR_VERSION) {
			/* RFC2408 3.1 ISAKMP Header Format:
			 *
			 * Minor Version (4 bits) - indicates the minor
			 * version of the ISAKMP protocol in use.
			 * Implementations based on this version of the
			 * ISAKMP Internet-Draft MUST set the Minor
			 * Version to 0.  Implementations based on
			 * previous versions of ISAKMP Internet- Drafts
			 * MUST set the Minor Version to 1.
			 * Implementations SHOULD never accept packets
			 * with a minor version number larger than its
			 * own, given the major version numbers are
			 * identical.
			 */
			libreswan_log("ignoring packet with IKEv1 minor version number %d greater than %d", vmin, ISAKMP_MINOR_VERSION);
			SEND_NOTIFICATION(INVALID_MINOR_VERSION);
			return;
		}
		DBG(DBG_CONTROL,
		    DBG_log(" processing version=%u.%u packet with exchange type=%s (%d)",
			    vmaj, vmin,
			    enum_name(&exchange_names_ikev1orv2, md->hdr.isa_xchg),
			    md->hdr.isa_xchg));
		process_v1_packet(mdp);
		break;

	case IKEv2_MAJOR_VERSION:
		if (vmin != IKEv2_MINOR_VERSION) {
			/*
			 * Unlike IKEv1, for IKEv2 we are supposed to try and
			 * continue on unknown minors
			 */
			libreswan_log("ignoring unknown IKEv2 minor version number %d", vmin);
		}
		DBG(DBG_CONTROL,
		    DBG_log(" processing version=%u.%u packet with exchange type=%s (%d)",
			    vmaj, vmin,
			    enum_name(&exchange_names_ikev1orv2, md->hdr.isa_xchg),
			    md->hdr.isa_xchg));
		process_v2_packet(mdp);
		break;

	default:
		libreswan_log("Unexpected IKE major '%d'",vmaj);
		SEND_NOTIFICATION(INVALID_MAJOR_VERSION);
		return;
	}
#undef SEND_NOTIFICATION
}