crypto_xmit(
	struct peer *peer,	/* peer structure pointer */
	struct pkt *xpkt,	/* transmit packet pointer */
	struct recvbuf *rbufp,	/* receive buffer pointer */
	int	start,		/* offset to extension field */
	struct exten *ep,	/* extension pointer */
	keyid_t cookie		/* session cookie */
	)
{
	struct exten *fp;	/* extension pointers */
	struct cert_info *cp, *xp, *yp; /* cert info/value pointer */
	sockaddr_u *srcadr_sin; /* source address */
	u_int32	*pkt;		/* packet pointer */
	u_int	opcode;		/* extension field opcode */
	char	certname[MAXHOSTNAME + 1]; /* subject name buffer */
	char	statstr[NTP_MAXSTRLEN]; /* statistics for filegen */
	tstamp_t tstamp;
	struct calendar tscal;
	u_int	vallen;
	struct value vtemp;
	associd_t associd;
	int	rval;
	int	len;
	keyid_t tcookie;

	/*
	 * Generate the requested extension field request code, length
	 * and association ID. If this is a response and the host is not
	 * synchronized, light the error bit and go home.
	 */
	pkt = (u_int32 *)xpkt + start / 4;
	fp = (struct exten *)pkt;
	opcode = ntohl(ep->opcode);
	if (peer != NULL) {
		srcadr_sin = &peer->srcadr;
		if (!(opcode & CRYPTO_RESP))
			peer->opcode = ep->opcode;
	} else {
		srcadr_sin = &rbufp->recv_srcadr;
	}
	associd = (associd_t) ntohl(ep->associd);
	len = 8;
	fp->opcode = htonl((opcode & 0xffff0000) | len);
	fp->associd = ep->associd;
	rval = XEVNT_OK;
	tstamp = crypto_time();
	switch (opcode & 0xffff0000) {

	/*
	 * Send association request and response with status word and
	 * host name. Note, this message is not signed and the filestamp
	 * contains only the status word.
	 */
	case CRYPTO_ASSOC:
	case CRYPTO_ASSOC | CRYPTO_RESP:
		len = crypto_send(fp, &hostval, start);
		fp->fstamp = htonl(crypto_flags);
		break;

	/*
	 * Send certificate request. Use the values from the extension
	 * field.
	 */
	case CRYPTO_CERT:
		memset(&vtemp, 0, sizeof(vtemp));
		vtemp.tstamp = ep->tstamp;
		vtemp.fstamp = ep->fstamp;
		vtemp.vallen = ep->vallen;
		vtemp.ptr = (u_char *)ep->pkt;
		len = crypto_send(fp, &vtemp, start);
		break;

	/*
	 * Send sign request. Use the host certificate, which is self-
	 * signed and may or may not be trusted.
	 */
	case CRYPTO_SIGN:
		(void)ntpcal_ntp_to_date(&tscal, tstamp, NULL);
		if ((calcomp(&tscal, &(cert_host->first)) < 0)
		|| (calcomp(&tscal, &(cert_host->last)) > 0))
			rval = XEVNT_PER;
		else
			len = crypto_send(fp, &cert_host->cert, start);
		break;

	/*
	 * Send certificate response. Use the name in the extension
	 * field to find the certificate in the cache. If the request
	 * contains no subject name, assume the name of this host. This
	 * is for backwards compatibility. Private certificates are
	 * never sent.
	 *
	 * There may be several certificates matching the request. First
	 * choice is a self-signed trusted certificate; second choice is
	 * any certificate signed by another host. There is no third
	 * choice. 
	 */
	case CRYPTO_CERT | CRYPTO_RESP:
		vallen = ntohl(ep->vallen);	/* Must be <64k */
		if (vallen == 0 || vallen > MAXHOSTNAME ||
		    len - VALUE_LEN < vallen) {
			rval = XEVNT_LEN;
			break;
		}

		/*
		 * Find all public valid certificates with matching
		 * subject. If a self-signed, trusted certificate is
		 * found, use that certificate. If not, use the last non
		 * self-signed certificate.
		 */
		memcpy(certname, ep->pkt, vallen);
		certname[vallen] = '\0';
		xp = yp = NULL;
		for (cp = cinfo; cp != NULL; cp = cp->link) {
			if (cp->flags & (CERT_PRIV | CERT_ERROR))
				continue;

			if (strcmp(certname, cp->subject) != 0)
				continue;

			if (strcmp(certname, cp->issuer) != 0)
				yp = cp;
			else if (cp ->flags & CERT_TRUST)
				xp = cp;
			continue;
		}

		/*
		 * Be careful who you trust. If the certificate is not
		 * found, return an empty response. Note that we dont
		 * enforce lifetimes here.
		 *
		 * The timestamp and filestamp are taken from the
		 * certificate value structure. For all certificates the
		 * timestamp is the latest signature update time. For
		 * host and imported certificates the filestamp is the
		 * creation epoch. For signed certificates the filestamp
		 * is the creation epoch of the trusted certificate at
		 * the root of the certificate trail. In principle, this
		 * allows strong checking for signature masquerade.
		 */
		if (xp == NULL)
			xp = yp;
		if (xp == NULL)
			break;

		if (tstamp == 0)
			break;

		len = crypto_send(fp, &xp->cert, start);
		break;

	/*
	 * Send challenge in Schnorr (IFF) identity scheme.
	 */
	case CRYPTO_IFF:
		if (peer == NULL)
			break;		/* hack attack */

		if ((rval = crypto_alice(peer, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send response in Schnorr (IFF) identity scheme.
	 */
	case CRYPTO_IFF | CRYPTO_RESP:
		if ((rval = crypto_bob(ep, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send challenge in Guillou-Quisquater (GQ) identity scheme.
	 */
	case CRYPTO_GQ:
		if (peer == NULL)
			break;		/* hack attack */

		if ((rval = crypto_alice2(peer, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send response in Guillou-Quisquater (GQ) identity scheme.
	 */
	case CRYPTO_GQ | CRYPTO_RESP:
		if ((rval = crypto_bob2(ep, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send challenge in MV identity scheme.
	 */
	case CRYPTO_MV:
		if (peer == NULL)
			break;		/* hack attack */

		if ((rval = crypto_alice3(peer, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send response in MV identity scheme.
	 */
	case CRYPTO_MV | CRYPTO_RESP:
		if ((rval = crypto_bob3(ep, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send certificate sign response. The integrity of the request
	 * certificate has already been verified on the receive side.
	 * Sign the response using the local server key. Use the
	 * filestamp from the request and use the timestamp as the
	 * current time. Light the error bit if the certificate is
	 * invalid or contains an unverified signature.
	 */
	case CRYPTO_SIGN | CRYPTO_RESP:
		if ((rval = cert_sign(ep, &vtemp)) == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Send public key and signature. Use the values from the public
	 * key.
	 */
	case CRYPTO_COOK:
		len = crypto_send(fp, &pubkey, start);
		break;

	/*
	 * Encrypt and send cookie and signature. Light the error bit if
	 * anything goes wrong.
	 */
	case CRYPTO_COOK | CRYPTO_RESP:
		vallen = ntohl(ep->vallen);	/* Must be <64k */
		if (   vallen == 0
		    || (vallen >= MAX_VALLEN)
		    || (opcode & 0x0000ffff)  < VALUE_LEN + vallen) {
			rval = XEVNT_LEN;
			break;
		}
		if (peer == NULL)
			tcookie = cookie;
		else
			tcookie = peer->hcookie;
		if ((rval = crypto_encrypt((const u_char *)ep->pkt, vallen, &tcookie, &vtemp))
		    == XEVNT_OK) {
			len = crypto_send(fp, &vtemp, start);
			value_free(&vtemp);
		}
		break;

	/*
	 * Find peer and send autokey data and signature in broadcast
	 * server and symmetric modes. Use the values in the autokey
	 * structure. If no association is found, either the server has
	 * restarted with new associations or some perp has replayed an
	 * old message, in which case light the error bit.
	 */
	case CRYPTO_AUTO | CRYPTO_RESP:
		if (peer == NULL) {
			if ((peer = findpeerbyassoc(associd)) == NULL) {
				rval = XEVNT_ERR;
				break;
			}
		}
		peer->flags &= ~FLAG_ASSOC;
		len = crypto_send(fp, &peer->sndval, start);
		break;

	/*
	 * Send leapseconds values and signature. Use the values from
	 * the tai structure. If no table has been loaded, just send an
	 * empty request.
	 */
	case CRYPTO_LEAP | CRYPTO_RESP:
		len = crypto_send(fp, &tai_leap, start);
		break;

	/*
	 * Default - Send a valid command for unknown requests; send
	 * an error response for unknown resonses.
	 */
	default:
		if (opcode & CRYPTO_RESP)
			rval = XEVNT_ERR;
	}

	/*
	 * In case of error, flame the log. If a request, toss the
	 * puppy; if a response, return so the sender can flame, too.
	 */
	if (rval != XEVNT_OK) {
		u_int32	uint32;

		uint32 = CRYPTO_ERROR;
		opcode |= uint32;
		fp->opcode |= htonl(uint32);
		snprintf(statstr, sizeof(statstr),
		    "%04x %d %02x %s", opcode, associd, rval,
		    eventstr(rval));
		record_crypto_stats(srcadr_sin, statstr);
		DPRINTF(1, ("crypto_xmit: %s\n", statstr));
		if (!(opcode & CRYPTO_RESP))
			return (0);
	}
	DPRINTF(1, ("crypto_xmit: flags 0x%x offset %d len %d code 0x%x associd %d\n",
		    crypto_flags, start, len, opcode >> 16, associd));
	return (len);
}