crypto_recv(
	struct peer *peer,	/* peer structure pointer */
	struct recvbuf *rbufp	/* packet buffer pointer */
	)
{
	const EVP_MD *dp;	/* message digest algorithm */
	u_int32	*pkt;		/* receive packet pointer */
	struct autokey *ap, *bp; /* autokey pointer */
	struct exten *ep, *fp;	/* extension pointers */
	struct cert_info *xinfo; /* certificate info pointer */
	int	has_mac;	/* length of MAC field */
	int	authlen;	/* offset of MAC field */
	associd_t associd;	/* association ID */
	tstamp_t fstamp = 0;	/* filestamp */
	u_int	len;		/* extension field length */
	u_int	code;		/* extension field opcode */
	u_int	vallen = 0;	/* value length */
	X509	*cert;		/* X509 certificate */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	keyid_t	cookie;		/* crumbles */
	int	hismode;	/* packet mode */
	int	rval = XEVNT_OK;
	const u_char *puch;
	u_int32 temp32;

	/*
	 * Initialize. Note that the packet has already been checked for
	 * valid format and extension field lengths. First extract the
	 * field length, command code and association ID in host byte
	 * order. These are used with all commands and modes. Then check
	 * the version number, which must be 2, and length, which must
	 * be at least 8 for requests and VALUE_LEN (24) for responses.
	 * Packets that fail either test sink without a trace. The
	 * association ID is saved only if nonzero.
	 */
	authlen = LEN_PKT_NOMAC;
	hismode = (int)PKT_MODE((&rbufp->recv_pkt)->li_vn_mode);
	while ((has_mac = rbufp->recv_length - authlen) > (int)MAX_MAC_LEN) {
		pkt = (u_int32 *)&rbufp->recv_pkt + authlen / 4;
		ep = (struct exten *)pkt;
		code = ntohl(ep->opcode) & 0xffff0000;
		len = ntohl(ep->opcode) & 0x0000ffff;
		// HMS: Why pkt[1] instead of ep->associd ?
		associd = (associd_t)ntohl(pkt[1]);
		rval = XEVNT_OK;
		DPRINTF(1, ("crypto_recv: flags 0x%x ext offset %d len %u code 0x%x associd %d\n",
			    peer->crypto, authlen, len, code >> 16,
			    associd));

		/*
		 * Check version number and field length. If bad,
		 * quietly ignore the packet.
		 */
		if (((code >> 24) & 0x3f) != CRYPTO_VN || len < 8) {
			sys_badlength++;
			code |= CRYPTO_ERROR;
		}

		if (len >= VALUE_LEN) {
			fstamp = ntohl(ep->fstamp);
			vallen = ntohl(ep->vallen);
			/*
			 * Bug 2761: I hope this isn't too early...
			 */
			if (   vallen == 0
			    || len - VALUE_LEN < vallen)
				return XEVNT_LEN;
		}
		switch (code) {

		/*
		 * Install status word, host name, signature scheme and
		 * association ID. In OpenSSL the signature algorithm is
		 * bound to the digest algorithm, so the NID completely
		 * defines the signature scheme. Note the request and
		 * response are identical, but neither is validated by
		 * signature. The request is processed here only in
		 * symmetric modes. The server name field might be
		 * useful to implement access controls in future.
		 */
		case CRYPTO_ASSOC:

			/*
			 * If our state machine is running when this
			 * message arrives, the other fellow might have
			 * restarted. However, this could be an
			 * intruder, so just clamp the poll interval and
			 * find out for ourselves. Otherwise, pass the
			 * extension field to the transmit side.
			 */
			if (peer->crypto & CRYPTO_FLAG_CERT) {
				rval = XEVNT_ERR;
				break;
			}
			if (peer->cmmd) {
				if (peer->assoc != associd) {
					rval = XEVNT_ERR;
					break;
				}
			}
			fp = emalloc(len);
			memcpy(fp, ep, len);
			fp->associd = htonl(peer->associd);
			peer->cmmd = fp;
			/* fall through */

		case CRYPTO_ASSOC | CRYPTO_RESP:

			/*
			 * Discard the message if it has already been
			 * stored or the message has been amputated.
			 */
			if (peer->crypto) {
				if (peer->assoc != associd)
					rval = XEVNT_ERR;
				break;
			}
			INSIST(len >= VALUE_LEN);
			if (vallen == 0 || vallen > MAXHOSTNAME ||
			    len - VALUE_LEN < vallen) {
				rval = XEVNT_LEN;
				break;
			}
			DPRINTF(1, ("crypto_recv: ident host 0x%x %d server 0x%x %d\n",
				    crypto_flags, peer->associd, fstamp,
				    peer->assoc));
			temp32 = crypto_flags & CRYPTO_FLAG_MASK;

			/*
			 * If the client scheme is PC, the server scheme
			 * must be PC. The public key and identity are
			 * presumed valid, so we skip the certificate
			 * and identity exchanges and move immediately
			 * to the cookie exchange which confirms the
			 * server signature.
			 */
			if (crypto_flags & CRYPTO_FLAG_PRIV) {
				if (!(fstamp & CRYPTO_FLAG_PRIV)) {
					rval = XEVNT_KEY;
					break;
				}
				fstamp |= CRYPTO_FLAG_CERT |
				    CRYPTO_FLAG_VRFY | CRYPTO_FLAG_SIGN;

			/*
			 * It is an error if either peer supports
			 * identity, but the other does not.
			 */
			} else if (hismode == MODE_ACTIVE || hismode ==
			    MODE_PASSIVE) {
				if ((temp32 && !(fstamp &
				    CRYPTO_FLAG_MASK)) ||
				    (!temp32 && (fstamp &
				    CRYPTO_FLAG_MASK))) {
					rval = XEVNT_KEY;
					break;
				}
			}

			/*
			 * Discard the message if the signature digest
			 * NID is not supported.
			 */
			temp32 = (fstamp >> 16) & 0xffff;
			dp =
			    (const EVP_MD *)EVP_get_digestbynid(temp32);
			if (dp == NULL) {
				rval = XEVNT_MD;
				break;
			}

			/*
			 * Save status word, host name and message
			 * digest/signature type. If this is from a
			 * broadcast and the association ID has changed,
			 * request the autokey values.
			 */
			peer->assoc = associd;
			if (hismode == MODE_SERVER)
				fstamp |= CRYPTO_FLAG_AUTO;
			if (!(fstamp & CRYPTO_FLAG_TAI))
				fstamp |= CRYPTO_FLAG_LEAP;
			RAND_bytes((u_char *)&peer->hcookie, 4);
			peer->crypto = fstamp;
			peer->digest = dp;
			if (peer->subject != NULL)
				free(peer->subject);
			peer->subject = emalloc(vallen + 1);
			memcpy(peer->subject, ep->pkt, vallen);
			peer->subject[vallen] = '\0';
			if (peer->issuer != NULL)
				free(peer->issuer);
			peer->issuer = estrdup(peer->subject);
			snprintf(statstr, sizeof(statstr),
			    "assoc %d %d host %s %s", peer->associd,
			    peer->assoc, peer->subject,
			    OBJ_nid2ln(temp32));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * Decode X509 certificate in ASN.1 format and extract
		 * the data containing, among other things, subject
		 * name and public key. In the default identification
		 * scheme, the certificate trail is followed to a self
		 * signed trusted certificate.
		 */
		case CRYPTO_CERT | CRYPTO_RESP:

			/*
			 * Discard the message if empty or invalid.
			 */
			if (len < VALUE_LEN)
				break;

			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * Scan the certificate list to delete old
			 * versions and link the newest version first on
			 * the list. Then, verify the signature. If the
			 * certificate is bad or missing, just ignore
			 * it.
			 */
			if ((xinfo = cert_install(ep, peer)) == NULL) {
				rval = XEVNT_CRT;
				break;
			}
			if ((rval = cert_hike(peer, xinfo)) != XEVNT_OK)
				break;

			/*
			 * We plug in the public key and lifetime from
			 * the first certificate received. However, note
			 * that this certificate might not be signed by
			 * the server, so we can't check the
			 * signature/digest NID.
			 */
			if (peer->pkey == NULL) {
				puch = xinfo->cert.ptr;
				cert = d2i_X509(NULL, &puch,
				    ntohl(xinfo->cert.vallen));
				peer->pkey = X509_get_pubkey(cert);
				X509_free(cert);
			}
			peer->flash &= ~TEST8;
			temp32 = xinfo->nid;
			snprintf(statstr, sizeof(statstr),
			    "cert %s %s 0x%x %s (%u) fs %u",
			    xinfo->subject, xinfo->issuer, xinfo->flags,
			    OBJ_nid2ln(temp32), temp32,
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * Schnorr (IFF) identity scheme. This scheme is
		 * designed for use with shared secret server group keys
		 * and where the certificate may be generated by a third
		 * party. The client sends a challenge to the server,
		 * which performs a calculation and returns the result.
		 * A positive result is possible only if both client and
		 * server contain the same secret group key.
		 */
		case CRYPTO_IFF | CRYPTO_RESP:

			/*
			 * Discard the message if invalid.
			 */
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * If the challenge matches the response, the
			 * server public key, signature and identity are
			 * all verified at the same time. The server is
			 * declared trusted, so we skip further
			 * certificate exchanges and move immediately to
			 * the cookie exchange.
			 */
			if ((rval = crypto_iff(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_VRFY;
			peer->flash &= ~TEST8;
			snprintf(statstr, sizeof(statstr), "iff %s fs %u",
			    peer->issuer, ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * Guillou-Quisquater (GQ) identity scheme. This scheme
		 * is designed for use with public certificates carrying
		 * the GQ public key in an extension field. The client
		 * sends a challenge to the server, which performs a
		 * calculation and returns the result. A positive result
		 * is possible only if both client and server contain
		 * the same group key and the server has the matching GQ
		 * private key.
		 */
		case CRYPTO_GQ | CRYPTO_RESP:

			/*
			 * Discard the message if invalid
			 */
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * If the challenge matches the response, the
			 * server public key, signature and identity are
			 * all verified at the same time. The server is
			 * declared trusted, so we skip further
			 * certificate exchanges and move immediately to
			 * the cookie exchange.
			 */
			if ((rval = crypto_gq(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_VRFY;
			peer->flash &= ~TEST8;
			snprintf(statstr, sizeof(statstr), "gq %s fs %u",
			    peer->issuer, ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * Mu-Varadharajan (MV) identity scheme. This scheme is
		 * designed for use with three levels of trust, trusted
		 * host, server and client. The trusted host key is
		 * opaque to servers and clients; the server keys are
		 * opaque to clients and each client key is different.
		 * Client keys can be revoked without requiring new key
		 * generations.
		 */
		case CRYPTO_MV | CRYPTO_RESP:

			/*
			 * Discard the message if invalid.
			 */
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * If the challenge matches the response, the
			 * server public key, signature and identity are
			 * all verified at the same time. The server is
			 * declared trusted, so we skip further
			 * certificate exchanges and move immediately to
			 * the cookie exchange.
			 */
			if ((rval = crypto_mv(ep, peer)) != XEVNT_OK)
				break;

			peer->crypto |= CRYPTO_FLAG_VRFY;
			peer->flash &= ~TEST8;
			snprintf(statstr, sizeof(statstr), "mv %s fs %u",
			    peer->issuer, ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;


		/*
		 * Cookie response in client and symmetric modes. If the
		 * cookie bit is set, the working cookie is the EXOR of
		 * the current and new values.
		 */
		case CRYPTO_COOK | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or signature
			 * not verified with respect to the cookie
			 * values.
			 */
			if ((rval = crypto_verify(ep, &peer->cookval,
			    peer)) != XEVNT_OK)
				break;

			/*
			 * Decrypt the cookie, hunting all the time for
			 * errors.
			 */
			if (vallen == (u_int)EVP_PKEY_size(host_pkey)) {
				u_int32 *cookiebuf = malloc(
				    RSA_size(host_pkey->pkey.rsa));
				if (!cookiebuf) {
					rval = XEVNT_CKY;
					break;
				}

				if (RSA_private_decrypt(vallen,
				    (u_char *)ep->pkt,
				    (u_char *)cookiebuf,
				    host_pkey->pkey.rsa,
				    RSA_PKCS1_OAEP_PADDING) != 4) {
					rval = XEVNT_CKY;
					free(cookiebuf);
					break;
				} else {
					cookie = ntohl(*cookiebuf);
					free(cookiebuf);
				}
			} else {
				rval = XEVNT_CKY;
				break;
			}

			/*
			 * Install cookie values and light the cookie
			 * bit. If this is not broadcast client mode, we
			 * are done here.
			 */
			key_expire(peer);
			if (hismode == MODE_ACTIVE || hismode ==
			    MODE_PASSIVE)
				peer->pcookie = peer->hcookie ^ cookie;
			else
				peer->pcookie = cookie;
			peer->crypto |= CRYPTO_FLAG_COOK;
			peer->flash &= ~TEST8;
			snprintf(statstr, sizeof(statstr),
			    "cook %x ts %u fs %u", peer->pcookie,
			    ntohl(ep->tstamp), ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * Install autokey values in broadcast client and
		 * symmetric modes. We have to do this every time the
		 * sever/peer cookie changes or a new keylist is
		 * rolled. Ordinarily, this is automatic as this message
		 * is piggybacked on the first NTP packet sent upon
		 * either of these events. Note that a broadcast client
		 * or symmetric peer can receive this response without a
		 * matching request.
		 */
		case CRYPTO_AUTO | CRYPTO_RESP:

			/*
			 * Discard the message if invalid or signature
			 * not verified with respect to the receive
			 * autokey values.
			 */
			if ((rval = crypto_verify(ep, &peer->recval,
			    peer)) != XEVNT_OK) 
				break;

			/*
			 * Discard the message if a broadcast client and
			 * the association ID does not match. This might
			 * happen if a broacast server restarts the
			 * protocol. A protocol restart will occur at
			 * the next ASSOC message.
			 */
			if ((peer->cast_flags & MDF_BCLNT) &&
			    peer->assoc != associd)
				break;

			/*
			 * Install autokey values and light the
			 * autokey bit. This is not hard.
			 */
			if (ep->tstamp == 0)
				break;

			if (peer->recval.ptr == NULL)
				peer->recval.ptr =
				    emalloc(sizeof(struct autokey));
			bp = (struct autokey *)peer->recval.ptr;
			peer->recval.tstamp = ep->tstamp;
			peer->recval.fstamp = ep->fstamp;
			ap = (struct autokey *)ep->pkt;
			bp->seq = ntohl(ap->seq);
			bp->key = ntohl(ap->key);
			peer->pkeyid = bp->key;
			peer->crypto |= CRYPTO_FLAG_AUTO;
			peer->flash &= ~TEST8;
			snprintf(statstr, sizeof(statstr), 
			    "auto seq %d key %x ts %u fs %u", bp->seq,
			    bp->key, ntohl(ep->tstamp),
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;
	
		/*
		 * X509 certificate sign response. Validate the
		 * certificate signed by the server and install. Later
		 * this can be provided to clients of this server in
		 * lieu of the self signed certificate in order to
		 * validate the public key.
		 */
		case CRYPTO_SIGN | CRYPTO_RESP:

			/*
			 * Discard the message if invalid.
			 */
			if ((rval = crypto_verify(ep, NULL, peer)) !=
			    XEVNT_OK)
				break;

			/*
			 * Scan the certificate list to delete old
			 * versions and link the newest version first on
			 * the list.
			 */
			if ((xinfo = cert_install(ep, peer)) == NULL) {
				rval = XEVNT_CRT;
				break;
			}
			peer->crypto |= CRYPTO_FLAG_SIGN;
			peer->flash &= ~TEST8;
			temp32 = xinfo->nid;
			snprintf(statstr, sizeof(statstr),
			    "sign %s %s 0x%x %s (%u) fs %u",
			    xinfo->subject, xinfo->issuer, xinfo->flags,
			    OBJ_nid2ln(temp32), temp32,
			    ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * Install leapseconds values. While the leapsecond
		 * values epoch, TAI offset and values expiration epoch
		 * are retained, only the current TAI offset is provided
		 * via the kernel to other applications.
		 */
		case CRYPTO_LEAP | CRYPTO_RESP:
			/*
			 * Discard the message if invalid. We can't
			 * compare the value timestamps here, as they
			 * can be updated by different servers.
			 */
			rval = crypto_verify(ep, NULL, peer);
			if ((rval   != XEVNT_OK          ) ||
			    (vallen != 3*sizeof(uint32_t))  )
				break;

			/* Check if we can update the basic TAI offset
			 * for our current leap frame. This is a hack
			 * and ignores the time stamps in the autokey
			 * message.
			 */
			if (sys_leap != LEAP_NOTINSYNC)
				leapsec_autokey_tai(ntohl(ep->pkt[0]),
						    rbufp->recv_time.l_ui, NULL);
			tai_leap.tstamp = ep->tstamp;
			tai_leap.fstamp = ep->fstamp;
			crypto_update();
			mprintf_event(EVNT_TAI, peer,
				      "%d seconds", ntohl(ep->pkt[0]));
			peer->crypto |= CRYPTO_FLAG_LEAP;
			peer->flash &= ~TEST8;
			snprintf(statstr, sizeof(statstr),
				 "leap TAI offset %d at %u expire %u fs %u",
				 ntohl(ep->pkt[0]), ntohl(ep->pkt[1]),
				 ntohl(ep->pkt[2]), ntohl(ep->fstamp));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			break;

		/*
		 * We come here in symmetric modes for miscellaneous
		 * commands that have value fields but are processed on
		 * the transmit side. All we need do here is check for
		 * valid field length. Note that ASSOC is handled
		 * separately.
		 */
		case CRYPTO_CERT:
		case CRYPTO_IFF:
		case CRYPTO_GQ:
		case CRYPTO_MV:
		case CRYPTO_COOK:
		case CRYPTO_SIGN:
			if (len < VALUE_LEN) {
				rval = XEVNT_LEN;
				break;
			}
			/* fall through */

		/*
		 * We come here in symmetric modes for requests
		 * requiring a response (above plus AUTO and LEAP) and
		 * for responses. If a request, save the extension field
		 * for later; invalid requests will be caught on the
		 * transmit side. If an error or invalid response,
		 * declare a protocol error.
		 */
		default:
			if (code & (CRYPTO_RESP | CRYPTO_ERROR)) {
				rval = XEVNT_ERR;
			} else if (peer->cmmd == NULL) {
				fp = emalloc(len);
				memcpy(fp, ep, len);
				peer->cmmd = fp;
			}
		}

		/*
		 * The first error found terminates the extension field
		 * scan and we return the laundry to the caller.
		 */
		if (rval != XEVNT_OK) {
			snprintf(statstr, sizeof(statstr),
			    "%04x %d %02x %s", htonl(ep->opcode),
			    associd, rval, eventstr(rval));
			record_crypto_stats(&peer->srcadr, statstr);
			DPRINTF(1, ("crypto_recv: %s\n", statstr));
			return (rval);
		}
		authlen += (len + 3) / 4 * 4;
	}
	return (rval);
}