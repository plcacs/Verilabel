void process_packet_tail(struct msg_digest *md)
{
	struct state *st = md->st;
	enum state_kind from_state = md->v1_from_state;
	const struct state_v1_microcode *smc = md->smc;
	bool new_iv_set = md->new_iv_set;
	bool self_delete = FALSE;

	if (md->hdr.isa_flags & ISAKMP_FLAGS_v1_ENCRYPTION) {
		endpoint_buf b;
		dbg("received encrypted packet from %s", str_endpoint(&md->sender, &b));

		if (st == NULL) {
			libreswan_log(
				"discarding encrypted message for an unknown ISAKMP SA");
			return;
		}
		if (st->st_skeyid_e_nss == NULL) {
			loglog(RC_LOG_SERIOUS,
				"discarding encrypted message because we haven't yet negotiated keying material");
			return;
		}

		/* Mark as encrypted */
		md->encrypted = TRUE;

		/* do the specified decryption
		 *
		 * IV is from st->st_iv or (if new_iv_set) st->st_new_iv.
		 * The new IV is placed in st->st_new_iv
		 *
		 * See RFC 2409 "IKE" Appendix B
		 *
		 * XXX The IV should only be updated really if the packet
		 * is successfully processed.
		 * We should keep this value, check for a success return
		 * value from the parsing routines and then replace.
		 *
		 * Each post phase 1 exchange generates IVs from
		 * the last phase 1 block, not the last block sent.
		 */
		const struct encrypt_desc *e = st->st_oakley.ta_encrypt;

		if (pbs_left(&md->message_pbs) % e->enc_blocksize != 0) {
			loglog(RC_LOG_SERIOUS, "malformed message: not a multiple of encryption blocksize");
			return;
		}

		/* XXX Detect weak keys */

		/* grab a copy of raw packet (for duplicate packet detection) */
		md->raw_packet = clone_bytes_as_chunk(md->packet_pbs.start,
						      pbs_room(&md->packet_pbs),
						      "raw packet");

		/* Decrypt everything after header */
		if (!new_iv_set) {
			if (st->st_v1_iv.len == 0) {
				init_phase2_iv(st, &md->hdr.isa_msgid);
			} else {
				/* use old IV */
				restore_new_iv(st, st->st_v1_iv);
			}
		}

		passert(st->st_v1_new_iv.len >= e->enc_blocksize);
		st->st_v1_new_iv.len = e->enc_blocksize;   /* truncate */

		if (DBGP(DBG_CRYPT)) {
			DBG_log("decrypting %u bytes using algorithm %s",
				(unsigned) pbs_left(&md->message_pbs),
				st->st_oakley.ta_encrypt->common.fqn);
			DBG_dump_hunk("IV before:", st->st_v1_new_iv);
		}
		e->encrypt_ops->do_crypt(e, md->message_pbs.cur,
					 pbs_left(&md->message_pbs),
					 st->st_enc_key_nss,
					 st->st_v1_new_iv.ptr, FALSE);
		if (DBGP(DBG_CRYPT)) {
			DBG_dump_hunk("IV after:", st->st_v1_new_iv);
			DBG_log("decrypted payload (starts at offset %td):",
				md->message_pbs.cur - md->message_pbs.roof);
			DBG_dump(NULL, md->message_pbs.start,
				 md->message_pbs.roof - md->message_pbs.start);
		}
	} else {
		/* packet was not encryped -- should it have been? */

		if (smc->flags & SMF_INPUT_ENCRYPTED) {
			loglog(RC_LOG_SERIOUS,
			       "packet rejected: should have been encrypted");
			SEND_NOTIFICATION(INVALID_FLAGS);
			return;
		}
	}

	/* Digest the message.
	 * Padding must be removed to make hashing work.
	 * Padding comes from encryption (so this code must be after decryption).
	 * Padding rules are described before the definition of
	 * struct isakmp_hdr in packet.h.
	 */
	{
		enum next_payload_types_ikev1 np = md->hdr.isa_np;
		lset_t needed = smc->req_payloads;
		const char *excuse =
			LIN(SMF_PSK_AUTH | SMF_FIRST_ENCRYPTED_INPUT,
			    smc->flags) ?
			"probable authentication failure (mismatch of preshared secrets?): "
			:
			"";

		while (np != ISAKMP_NEXT_NONE) {
			struct_desc *sd = v1_payload_desc(np);

			if (md->digest_roof >= elemsof(md->digest)) {
				loglog(RC_LOG_SERIOUS,
				       "more than %zu payloads in message; ignored",
				       elemsof(md->digest));
				if (!md->encrypted) {
					SEND_NOTIFICATION(PAYLOAD_MALFORMED);
				}
				return;
			}
			struct payload_digest *const pd = md->digest + md->digest_roof;

			/*
			 * only do this in main mode. In aggressive mode, there
			 * is no negotiation of NAT-T method. Get it right.
			 */
			if (st != NULL && st->st_connection != NULL &&
			    (st->st_connection->policy & POLICY_AGGRESSIVE) == LEMPTY)
			{
				switch (np) {
				case ISAKMP_NEXT_NATD_RFC:
				case ISAKMP_NEXT_NATOA_RFC:
					if ((st->hidden_variables.st_nat_traversal & NAT_T_WITH_RFC_VALUES) == LEMPTY) {
						/*
						 * don't accept NAT-D/NAT-OA reloc directly in message,
						 * unless we're using NAT-T RFC
						 */
						DBG(DBG_NATT,
						    DBG_log("st_nat_traversal was: %s",
							    bitnamesof(natt_bit_names,
								       st->hidden_variables.st_nat_traversal)));
						sd = NULL;
					}
					break;
				default:
					break;
				}
			}

			if (sd == NULL) {
				/* payload type is out of range or requires special handling */
				switch (np) {
				case ISAKMP_NEXT_ID:
					/* ??? two kinds of ID payloads */
					sd = (IS_PHASE1(from_state) ||
					      IS_PHASE15(from_state)) ?
						&isakmp_identification_desc :
						&isakmp_ipsec_identification_desc;
					break;

				case ISAKMP_NEXT_NATD_DRAFTS: /* out of range */
					/*
					 * ISAKMP_NEXT_NATD_DRAFTS was a private use type before RFC-3947.
					 * Since it has the same format as ISAKMP_NEXT_NATD_RFC,
					 * just rewrite np and sd, and carry on.
					 */
					np = ISAKMP_NEXT_NATD_RFC;
					sd = &isakmp_nat_d_drafts;
					break;

				case ISAKMP_NEXT_NATOA_DRAFTS: /* out of range */
					/* NAT-OA was a private use type before RFC-3947 -- same format */
					np = ISAKMP_NEXT_NATOA_RFC;
					sd = &isakmp_nat_oa_drafts;
					break;

				case ISAKMP_NEXT_SAK: /* or ISAKMP_NEXT_NATD_BADDRAFTS */
					/*
					 * Official standards say that this is ISAKMP_NEXT_SAK,
					 * a part of Group DOI, something we don't implement.
					 * Old non-updated Cisco gear abused this number in ancient NAT drafts.
					 * We ignore (rather than reject) this in support of people
					 * with crufty Cisco machines.
					 */
					loglog(RC_LOG_SERIOUS,
						"%smessage with unsupported payload ISAKMP_NEXT_SAK (or ISAKMP_NEXT_NATD_BADDRAFTS) ignored",
						excuse);
					/*
					 * Hack to discard payload, whatever it was.
					 * Since we are skipping the rest of the loop
					 * body we must do some things ourself:
					 * - demarshall the payload
					 * - grab the next payload number (np)
					 * - don't keep payload (don't increment pd)
					 * - skip rest of loop body
					 */
					if (!in_struct(&pd->payload, &isakmp_ignore_desc, &md->message_pbs,
						       &pd->pbs)) {
						loglog(RC_LOG_SERIOUS,
						       "%smalformed payload in packet",
						       excuse);
						if (!md->encrypted) {
							SEND_NOTIFICATION(PAYLOAD_MALFORMED);
						}
						return;
					}
					np = pd->payload.generic.isag_np;
					/* NOTE: we do not increment pd! */
					continue;  /* skip rest of the loop */

				default:
					loglog(RC_LOG_SERIOUS,
						"%smessage ignored because it contains an unknown or unexpected payload type (%s) at the outermost level",
					       excuse,
					       enum_show(&ikev1_payload_names, np));
					if (!md->encrypted) {
						SEND_NOTIFICATION(INVALID_PAYLOAD_TYPE);
					}
					return;
				}
				passert(sd != NULL);
			}

			passert(np < LELEM_ROOF);

			{
				lset_t s = LELEM(np);

				if (LDISJOINT(s,
					      needed | smc->opt_payloads |
					      LELEM(ISAKMP_NEXT_VID) |
					      LELEM(ISAKMP_NEXT_N) |
					      LELEM(ISAKMP_NEXT_D) |
					      LELEM(ISAKMP_NEXT_CR) |
					      LELEM(ISAKMP_NEXT_CERT))) {
					loglog(RC_LOG_SERIOUS,
						"%smessage ignored because it contains a payload type (%s) unexpected by state %s",
						excuse,
						enum_show(&ikev1_payload_names, np),
						finite_states[smc->state]->name);
					if (!md->encrypted) {
						SEND_NOTIFICATION(INVALID_PAYLOAD_TYPE);
					}
					return;
				}

				DBG(DBG_PARSING,
				    DBG_log("got payload 0x%" PRIxLSET"  (%s) needed: 0x%" PRIxLSET " opt: 0x%" PRIxLSET,
					    s, enum_show(&ikev1_payload_names, np),
					    needed, smc->opt_payloads));
				needed &= ~s;
			}

			/*
			 * Read in the payload recording what type it
			 * should be
			 */
			pd->payload_type = np;
			if (!in_struct(&pd->payload, sd, &md->message_pbs,
				       &pd->pbs)) {
				loglog(RC_LOG_SERIOUS,
				       "%smalformed payload in packet",
				       excuse);
				if (!md->encrypted) {
					SEND_NOTIFICATION(PAYLOAD_MALFORMED);
				}
				return;
			}

			/* do payload-type specific debugging */
			switch (np) {
			case ISAKMP_NEXT_ID:
			case ISAKMP_NEXT_NATOA_RFC:
				/* dump ID section */
				DBG(DBG_PARSING,
				    DBG_dump("     obj: ", pd->pbs.cur,
					     pbs_left(&pd->pbs)));
				break;
			default:
				break;
			}


			/*
			 * Place payload at the end of the chain for this type.
			 * This code appears in ikev1.c and ikev2.c.
			 */
			{
				/* np is a proper subscript for chain[] */
				passert(np < elemsof(md->chain));
				struct payload_digest **p = &md->chain[np];

				while (*p != NULL)
					p = &(*p)->next;
				*p = pd;
				pd->next = NULL;
			}

			np = pd->payload.generic.isag_np;
			md->digest_roof++;

			/* since we've digested one payload happily, it is probably
			 * the case that any decryption worked.  So we will not suggest
			 * encryption failure as an excuse for subsequent payload
			 * problems.
			 */
			excuse = "";
		}

		DBG(DBG_PARSING, {
			    if (pbs_left(&md->message_pbs) != 0)
				    DBG_log("removing %d bytes of padding",
					    (int) pbs_left(&md->message_pbs));
		    });

		md->message_pbs.roof = md->message_pbs.cur;

		/* check that all mandatory payloads appeared */

		if (needed != 0) {
			loglog(RC_LOG_SERIOUS,
			       "message for %s is missing payloads %s",
			       finite_states[from_state]->name,
			       bitnamesof(payload_name_ikev1, needed));
			if (!md->encrypted) {
				SEND_NOTIFICATION(PAYLOAD_MALFORMED);
			}
			return;
		}
	}

	if (!check_v1_HASH(smc->hash_type, smc->message, st, md)) {
		/*SEND_NOTIFICATION(INVALID_HASH_INFORMATION);*/
		return;
	}

	/* more sanity checking: enforce most ordering constraints */

	if (IS_PHASE1(from_state) || IS_PHASE15(from_state)) {
		/* rfc2409: The Internet Key Exchange (IKE), 5 Exchanges:
		 * "The SA payload MUST precede all other payloads in a phase 1 exchange."
		 */
		if (md->chain[ISAKMP_NEXT_SA] != NULL &&
		    md->hdr.isa_np != ISAKMP_NEXT_SA) {
			loglog(RC_LOG_SERIOUS,
			       "malformed Phase 1 message: does not start with an SA payload");
			if (!md->encrypted) {
				SEND_NOTIFICATION(PAYLOAD_MALFORMED);
			}
			return;
		}
	} else if (IS_QUICK(from_state)) {
		/* rfc2409: The Internet Key Exchange (IKE), 5.5 Phase 2 - Quick Mode
		 *
		 * "In Quick Mode, a HASH payload MUST immediately follow the ISAKMP
		 *  header and a SA payload MUST immediately follow the HASH."
		 * [NOTE: there may be more than one SA payload, so this is not
		 *  totally reasonable.  Probably all SAs should be so constrained.]
		 *
		 * "If ISAKMP is acting as a client negotiator on behalf of another
		 *  party, the identities of the parties MUST be passed as IDci and
		 *  then IDcr."
		 *
		 * "With the exception of the HASH, SA, and the optional ID payloads,
		 *  there are no payload ordering restrictions on Quick Mode."
		 */

		if (md->hdr.isa_np != ISAKMP_NEXT_HASH) {
			loglog(RC_LOG_SERIOUS,
			       "malformed Quick Mode message: does not start with a HASH payload");
			if (!md->encrypted) {
				SEND_NOTIFICATION(PAYLOAD_MALFORMED);
			}
			return;
		}

		{
			struct payload_digest *p;
			int i;

			p = md->chain[ISAKMP_NEXT_SA];
			i = 1;
			while (p != NULL) {
				if (p != &md->digest[i]) {
					loglog(RC_LOG_SERIOUS,
					       "malformed Quick Mode message: SA payload is in wrong position");
					if (!md->encrypted) {
						SEND_NOTIFICATION(PAYLOAD_MALFORMED);
					}
					return;
				}
				p = p->next;
				i++;
			}
		}

		/* rfc2409: The Internet Key Exchange (IKE), 5.5 Phase 2 - Quick Mode:
		 * "If ISAKMP is acting as a client negotiator on behalf of another
		 *  party, the identities of the parties MUST be passed as IDci and
		 *  then IDcr."
		 */
		{
			struct payload_digest *id = md->chain[ISAKMP_NEXT_ID];

			if (id != NULL) {
				if (id->next == NULL ||
				    id->next->next != NULL) {
					loglog(RC_LOG_SERIOUS,
						"malformed Quick Mode message: if any ID payload is present, there must be exactly two");
					SEND_NOTIFICATION(PAYLOAD_MALFORMED);
					return;
				}
				if (id + 1 != id->next) {
					loglog(RC_LOG_SERIOUS,
						"malformed Quick Mode message: the ID payloads are not adjacent");
					SEND_NOTIFICATION(PAYLOAD_MALFORMED);
					return;
				}
			}
		}
	}

	/*
	 * Ignore payloads that we don't handle:
	 */
	/* XXX Handle Notifications */
	{
		struct payload_digest *p = md->chain[ISAKMP_NEXT_N];

		while (p != NULL) {
			switch (p->payload.notification.isan_type) {
			case R_U_THERE:
			case R_U_THERE_ACK:
			case ISAKMP_N_CISCO_LOAD_BALANCE:
			case PAYLOAD_MALFORMED:
			case INVALID_MESSAGE_ID:
			case IPSEC_RESPONDER_LIFETIME:
				if (md->hdr.isa_xchg == ISAKMP_XCHG_INFO) {
					/* these are handled later on in informational() */
					break;
				}
				/* FALL THROUGH */
			default:
				if (st == NULL) {
					DBG(DBG_CONTROL, DBG_log(
					       "ignoring informational payload %s, no corresponding state",
					       enum_show(& ikev1_notify_names,
							 p->payload.notification.isan_type)));
				} else {
					loglog(RC_LOG_SERIOUS,
					       "ignoring informational payload %s, msgid=%08" PRIx32 ", length=%d",
					       enum_show(&ikev1_notify_names,
							 p->payload.notification.isan_type),
					       st->st_v1_msgid.id,
					       p->payload.notification.isan_length);
					DBG_dump_pbs(&p->pbs);
				}
			}
			if (DBGP(DBG_BASE)) {
				DBG_dump("info:", p->pbs.cur,
					 pbs_left(&p->pbs));
			}

			p = p->next;
		}

		p = md->chain[ISAKMP_NEXT_D];
		while (p != NULL) {
			self_delete |= accept_delete(md, p);
			if (DBGP(DBG_BASE)) {
				DBG_dump("del:", p->pbs.cur,
					 pbs_left(&p->pbs));
			}
			if (md->st != st) {
				pexpect(md->st == NULL);
				dbg("zapping ST as accept_delete() zapped MD.ST");
				st = md->st;
			}
			p = p->next;
		}

		p = md->chain[ISAKMP_NEXT_VID];
		while (p != NULL) {
			handle_vendorid(md, (char *)p->pbs.cur,
					pbs_left(&p->pbs), FALSE);
			p = p->next;
		}
	}

	if (self_delete) {
		accept_self_delete(md);
		st = md->st;
		/* note: st ought to be NULL from here on */
	}

	pexpect(st == md->st);
	statetime_t start = statetime_start(md->st);
	/*
	 * XXX: danger - the .informational() processor deletes ST;
	 * and then tunnels this loss through MD.ST.
	 */
	complete_v1_state_transition(md, smc->processor(st, md));
	statetime_stop(&start, "%s()", __func__);
	/* our caller will release_any_md(mdp); */
}