keyfetch_done(isc_task_t *task, isc_event_t *event) {
	isc_result_t result, eresult;
	dns_fetchevent_t *devent;
	dns_keyfetch_t *kfetch;
	dns_zone_t *zone;
	isc_mem_t *mctx = NULL;
	dns_keytable_t *secroots = NULL;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;
	bool alldone = false;
	bool commit = false;
	dns_name_t *keyname;
	dns_rdata_t sigrr = DNS_RDATA_INIT;
	dns_rdata_t dnskeyrr = DNS_RDATA_INIT;
	dns_rdata_t keydatarr = DNS_RDATA_INIT;
	dns_rdata_rrsig_t sig;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_keydata_t keydata;
	bool initializing;
	char namebuf[DNS_NAME_FORMATSIZE];
	unsigned char key_buf[4096];
	isc_buffer_t keyb;
	dst_key_t *dstkey;
	isc_stdtime_t now;
	int pending = 0;
	bool secure = false, initial = false;
	bool free_needed;

	UNUSED(task);
	INSIST(event != NULL && event->ev_type == DNS_EVENT_FETCHDONE);
	INSIST(event->ev_arg != NULL);

	kfetch = event->ev_arg;
	zone = kfetch->zone;
	isc_mem_attach(zone->mctx, &mctx);
	keyname = dns_fixedname_name(&kfetch->name);

	devent = (dns_fetchevent_t *) event;
	eresult = devent->result;

	/* Free resources which are not of interest */
	if (devent->node != NULL) {
		dns_db_detachnode(devent->db, &devent->node);
	}
	if (devent->db != NULL) {
		dns_db_detach(&devent->db);
	}
	isc_event_free(&event);
	dns_resolver_destroyfetch(&kfetch->fetch);

	LOCK_ZONE(zone);
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING) || zone->view == NULL) {
		goto cleanup;
	}

	isc_stdtime_get(&now);
	dns_name_format(keyname, namebuf, sizeof(namebuf));

	result = dns_view_getsecroots(zone->view, &secroots);
	INSIST(result == ISC_R_SUCCESS);

	dns_diff_init(mctx, &diff);

	CHECK(dns_db_newversion(kfetch->db, &ver));

	zone->refreshkeycount--;
	alldone = (zone->refreshkeycount == 0);

	if (alldone) {
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESHING);
	}

	dnssec_log(zone, ISC_LOG_DEBUG(3),
		   "Returned from key fetch in keyfetch_done() for '%s': %s",
		   namebuf, dns_result_totext(eresult));

	/* Fetch failed */
	if (eresult != ISC_R_SUCCESS ||
	    !dns_rdataset_isassociated(&kfetch->dnskeyset))
	{
		dnssec_log(zone, ISC_LOG_WARNING,
			   "Unable to fetch DNSKEY set '%s': %s",
			   namebuf, dns_result_totext(eresult));
		CHECK(minimal_update(kfetch, ver, &diff));
		goto done;
	}

	/* No RRSIGs found */
	if (!dns_rdataset_isassociated(&kfetch->dnskeysigset)) {
		dnssec_log(zone, ISC_LOG_WARNING,
			   "No DNSKEY RRSIGs found for '%s': %s",
			   namebuf, dns_result_totext(eresult));
		CHECK(minimal_update(kfetch, ver, &diff));
		goto done;
	}

	/*
	 * Clear any cached trust level, as we need to run validation
	 * over again; trusted keys might have changed.
	 */
	kfetch->dnskeyset.trust = kfetch->dnskeysigset.trust = dns_trust_none;

	/*
	 * Validate the dnskeyset against the current trusted keys.
	 */
	for (result = dns_rdataset_first(&kfetch->dnskeysigset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->dnskeysigset))
	{
		dns_keynode_t *keynode = NULL;

		dns_rdata_reset(&sigrr);
		dns_rdataset_current(&kfetch->dnskeysigset, &sigrr);
		result = dns_rdata_tostruct(&sigrr, &sig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		result = dns_keytable_find(secroots, keyname, &keynode);
		while (result == ISC_R_SUCCESS) {
			dns_keynode_t *nextnode = NULL;
			dns_fixedname_t fixed;
			dns_fixedname_init(&fixed);

			dstkey = dns_keynode_key(keynode);
			if (dstkey == NULL) {
				/* fail_secure() was called */
				break;
			}

			if (dst_key_alg(dstkey) == sig.algorithm &&
			    dst_key_id(dstkey) == sig.keyid)
			{
				result = dns_dnssec_verify(keyname,
							   &kfetch->dnskeyset,
							   dstkey, false,
							   0,
							   zone->view->mctx,
							   &sigrr,
							   dns_fixedname_name(
							   &fixed));

				dnssec_log(zone, ISC_LOG_DEBUG(3),
					   "Verifying DNSKEY set for zone "
					   "'%s' using key %d/%d: %s",
					   namebuf, sig.keyid, sig.algorithm,
					   dns_result_totext(result));

				if (result == ISC_R_SUCCESS) {
					kfetch->dnskeyset.trust =
						dns_trust_secure;
					kfetch->dnskeysigset.trust =
						dns_trust_secure;
					secure = true;
					initial = dns_keynode_initial(keynode);
					dns_keynode_trust(keynode);
					break;
				}
			}

			result = dns_keytable_nextkeynode(secroots,
							  keynode, &nextnode);
			dns_keytable_detachkeynode(secroots, &keynode);
			keynode = nextnode;
		}

		if (keynode != NULL) {
			dns_keytable_detachkeynode(secroots, &keynode);
		}

		if (secure) {
			break;
		}
	}

	/*
	 * If we were not able to verify the answer using the current
	 * trusted keys then all we can do is look at any revoked keys.
	 */
	if (!secure) {
		dnssec_log(zone, ISC_LOG_INFO,
			   "DNSKEY set for zone '%s' could not be verified "
			   "with current keys", namebuf);
	}

	/*
	 * First scan keydataset to find keys that are not in dnskeyset
	 *   - Missing keys which are not scheduled for removal,
	 *     log a warning
	 *   - Missing keys which are scheduled for removal and
	 *     the remove hold-down timer has completed should
	 *     be removed from the key zone
	 *   - Missing keys whose acceptance timers have not yet
	 *     completed, log a warning and reset the acceptance
	 *     timer to 30 days in the future
	 *   - All keys not being removed have their refresh timers
	 *     updated
	 */
	initializing = true;
	for (result = dns_rdataset_first(&kfetch->keydataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->keydataset))
	{
		dns_keytag_t keytag;

		dns_rdata_reset(&keydatarr);
		dns_rdataset_current(&kfetch->keydataset, &keydatarr);
		result = dns_rdata_tostruct(&keydatarr, &keydata, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		dns_keydata_todnskey(&keydata, &dnskey, NULL);
		result = compute_tag(keyname, &dnskey, mctx, &keytag);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		/*
		 * If any keydata record has a nonzero add holddown, then
		 * there was a pre-existing trust anchor for this domain;
		 * that means we are *not* initializing it and shouldn't
		 * automatically trust all the keys we find at the zone apex.
		 */
		initializing = initializing && (keydata.addhd == 0);

		if (! matchkey(&kfetch->dnskeyset, &keydatarr)) {
			bool deletekey = false;

			if (!secure) {
				if (keydata.removehd != 0 &&
				    keydata.removehd <= now)
				{
					deletekey = true;
				}
			} else if (keydata.addhd == 0) {
				deletekey = true;
			} else if (keydata.addhd > now) {
				dnssec_log(zone, ISC_LOG_INFO,
					   "Pending key %d for zone %s "
					   "unexpectedly missing "
					   "restarting 30-day acceptance "
					   "timer", keytag, namebuf);
				if (keydata.addhd < now + dns_zone_mkey_month) {
					keydata.addhd =
						now + dns_zone_mkey_month;
				}
				keydata.refresh = refresh_time(kfetch, false);
			} else if (keydata.removehd == 0) {
				dnssec_log(zone, ISC_LOG_INFO,
					   "Active key %d for zone %s "
					   "unexpectedly missing",
					   keytag, namebuf);
				keydata.refresh = now + dns_zone_mkey_hour;
			} else if (keydata.removehd <= now) {
				deletekey = true;
				dnssec_log(zone, ISC_LOG_INFO,
					   "Revoked key %d for zone %s "
					   "missing: deleting from "
					   "managed keys database",
					   keytag, namebuf);
			} else {
				keydata.refresh = refresh_time(kfetch, false);
			}

			if (secure || deletekey) {
				/* Delete old version */
				CHECK(update_one_rr(kfetch->db, ver, &diff,
						    DNS_DIFFOP_DEL, keyname, 0,
						    &keydatarr));
			}

			if (!secure || deletekey) {
				continue;
			}

			dns_rdata_reset(&keydatarr);
			isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
			dns_rdata_fromstruct(&keydatarr, zone->rdclass,
					     dns_rdatatype_keydata,
					     &keydata, &keyb);

			/* Insert updated version */
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_ADD, keyname, 0,
					    &keydatarr));

			set_refreshkeytimer(zone, &keydata, now, false);
		}
	}

	/*
	 * Next scan dnskeyset:
	 *   - If new keys are found (i.e., lacking a match in keydataset)
	 *     add them to the key zone and set the acceptance timer
	 *     to 30 days in the future (or to immediately if we've
	 *     determined that we're initializing the zone for the
	 *     first time)
	 *   - Previously-known keys that have been revoked
	 *     must be scheduled for removal from the key zone (or,
	 *     if they hadn't been accepted as trust anchors yet
	 *     anyway, removed at once)
	 *   - Previously-known unrevoked keys whose acceptance timers
	 *     have completed are promoted to trust anchors
	 *   - All keys not being removed have their refresh
	 *     timers updated
	 */
	for (result = dns_rdataset_first(&kfetch->dnskeyset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->dnskeyset))
	{
		bool revoked = false;
		bool newkey = false;
		bool updatekey = false;
		bool deletekey = false;
		bool trustkey = false;
		dns_keytag_t keytag;

		dns_rdata_reset(&dnskeyrr);
		dns_rdataset_current(&kfetch->dnskeyset, &dnskeyrr);
		result = dns_rdata_tostruct(&dnskeyrr, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		/* Skip ZSK's */
		if ((dnskey.flags & DNS_KEYFLAG_KSK) == 0) {
			continue;
		}

		result = compute_tag(keyname, &dnskey, mctx, &keytag);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		revoked = ((dnskey.flags & DNS_KEYFLAG_REVOKE) != 0);

		if (matchkey(&kfetch->keydataset, &dnskeyrr)) {
			dns_rdata_reset(&keydatarr);
			dns_rdataset_current(&kfetch->keydataset, &keydatarr);
			result = dns_rdata_tostruct(&keydatarr, &keydata, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);

			if (revoked && revocable(kfetch, &keydata)) {
				if (keydata.addhd > now) {
					/*
					 * Key wasn't trusted yet, and now
					 * it's been revoked?  Just remove it
					 */
					deletekey = true;
					dnssec_log(zone, ISC_LOG_INFO,
						   "Pending key %d for "
						   "zone %s is now revoked: "
						   "deleting from the "
						   "managed keys database",
						   keytag, namebuf);
				} else if (keydata.removehd == 0) {
					/*
					 * Remove key from secroots.
					 */
					dns_view_untrust(zone->view, keyname,
							 &dnskey, mctx);

					/* If initializing, delete now */
					if (keydata.addhd == 0) {
						deletekey = true;
					} else {
						keydata.removehd = now +
							dns_zone_mkey_month;
						keydata.flags |=
							DNS_KEYFLAG_REVOKE;
					}

					dnssec_log(zone, ISC_LOG_INFO,
						   "Trusted key %d for "
						   "zone %s is now revoked",
						   keytag, namebuf);
				} else if (keydata.removehd < now) {
					/* Scheduled for removal */
					deletekey = true;

					dnssec_log(zone, ISC_LOG_INFO,
						   "Revoked key %d for "
						   "zone %s removal timer "
						   "complete: deleting from "
						   "the managed keys database",
						   keytag, namebuf);
				}
			} else if (revoked && keydata.removehd == 0) {
				dnssec_log(zone, ISC_LOG_WARNING,
					   "Active key %d for zone "
					   "%s is revoked but "
					   "did not self-sign; "
					   "ignoring", keytag, namebuf);
				continue;
			} else if (secure) {
				if (keydata.removehd != 0) {
					/*
					 * Key isn't revoked--but it
					 * seems it used to be.
					 * Remove it now and add it
					 * back as if it were a fresh key,
					 * with a 30-day acceptance timer.
					 */
					deletekey = true;
					newkey = true;
					keydata.removehd = 0;
					keydata.addhd =
						now + dns_zone_mkey_month;

					dnssec_log(zone, ISC_LOG_INFO,
						   "Revoked key %d for "
						   "zone %s has returned: "
						   "starting 30-day "
						   "acceptance timer",
						   keytag, namebuf);
				} else if (keydata.addhd > now) {
					pending++;
				} else if (keydata.addhd == 0) {
					keydata.addhd = now;
				}

				if (keydata.addhd <= now) {
					trustkey = true;
					dnssec_log(zone, ISC_LOG_INFO,
						   "Key %d for zone %s "
						   "is now trusted (%s)",
						   keytag, namebuf,
						   initial
						    ? "initializing key "
						       "verified"
						    : "acceptance timer "
						       "complete");
				}
			} else if (keydata.addhd > now) {
				/*
				 * Not secure, and key is pending:
				 * reset the acceptance timer
				 */
				pending++;
				keydata.addhd = now + dns_zone_mkey_month;
				dnssec_log(zone, ISC_LOG_INFO,
					   "Pending key %d "
					   "for zone %s was "
					   "not validated: restarting "
					   "30-day acceptance timer",
					   keytag, namebuf);
			}

			if (!deletekey && !newkey) {
				updatekey = true;
			}
		} else if (secure) {
			/*
			 * Key wasn't in the key zone but it's
			 * revoked now anyway, so just skip it
			 */
			if (revoked) {
				continue;
			}

			/* Key wasn't in the key zone: add it */
			newkey = true;

			if (initializing) {
				dnssec_log(zone, ISC_LOG_WARNING,
					   "Initializing automatic trust "
					   "anchor management for zone '%s'; "
					   "DNSKEY ID %d is now trusted, "
					   "waiving the normal 30-day "
					   "waiting period.",
					   namebuf, keytag);
				trustkey = true;
			} else {
				dnssec_log(zone, ISC_LOG_INFO,
					   "New key %d observed "
					   "for zone '%s': "
					   "starting 30-day "
					   "acceptance timer",
					   keytag, namebuf);
			}
		} else {
			/*
			 * No previously known key, and the key is not
			 * secure, so skip it.
			 */
			continue;
		}

		/* Delete old version */
		if (deletekey || !newkey) {
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_DEL, keyname, 0,
					    &keydatarr));
		}

		if (updatekey) {
			/* Set refresh timer */
			keydata.refresh = refresh_time(kfetch, false);
			dns_rdata_reset(&keydatarr);
			isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
			dns_rdata_fromstruct(&keydatarr, zone->rdclass,
					     dns_rdatatype_keydata,
					     &keydata, &keyb);

			/* Insert updated version */
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_ADD, keyname, 0,
					    &keydatarr));
		} else if (newkey) {
			/* Convert DNSKEY to KEYDATA */
			result = dns_rdata_tostruct(&dnskeyrr, &dnskey, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			dns_keydata_fromdnskey(&keydata, &dnskey, 0, 0, 0,
					       NULL);
			keydata.addhd = initializing
					 ? now : now + dns_zone_mkey_month;
			keydata.refresh = refresh_time(kfetch, false);
			dns_rdata_reset(&keydatarr);
			isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
			dns_rdata_fromstruct(&keydatarr, zone->rdclass,
					     dns_rdatatype_keydata,
					     &keydata, &keyb);

			/* Insert into key zone */
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_ADD, keyname, 0,
					    &keydatarr));
		}

		if (trustkey) {
			/* Trust this key. */
			result = dns_rdata_tostruct(&dnskeyrr, &dnskey, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			trust_key(zone, keyname, &dnskey, false, mctx);
		}

		if (secure && !deletekey) {
			INSIST(newkey || updatekey);
			set_refreshkeytimer(zone, &keydata, now, false);
		}
	}

	/*
	 * RFC5011 says, "A trust point that has all of its trust anchors
	 * revoked is considered deleted and is treated as if the trust
	 * point was never configured."  But if someone revoked their
	 * active key before the standby was trusted, that would mean the
	 * zone would suddenly be nonsecured.  We avoid this by checking to
	 * see if there's pending keydata.  If so, we put a null key in
	 * the security roots; then all queries to the zone will fail.
	 */
	if (pending != 0) {
		fail_secure(zone, keyname);
	}

 done:
	if (!ISC_LIST_EMPTY(diff.tuples)) {
		/* Write changes to journal file. */
		CHECK(update_soa_serial(kfetch->db, ver, &diff, mctx,
					zone->updatemethod));
		CHECK(zone_journal(zone, &diff, NULL, "keyfetch_done"));
		commit = true;

		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
		zone_needdump(zone, 30);
	} else if (result == ISC_R_NOMORE) {
		/*
		 * If "updatekey" was true for all keys found in the DNSKEY
		 * response and the previous update of those keys happened
		 * during the same second (only possible if a key refresh was
		 * externally triggered), it may happen that all relevant
		 * update_one_rr() calls will return ISC_R_SUCCESS, but
		 * diff.tuples will remain empty.  Reset result to
		 * ISC_R_SUCCESS to prevent a bogus warning from being logged.
		 */
		result = ISC_R_SUCCESS;
	}

 failure:
	if (result != ISC_R_SUCCESS) {
		dnssec_log(zone, ISC_LOG_ERROR,
			   "error during managed-keys processing (%s): "
			   "DNSSEC validation may be at risk",
			   isc_result_totext(result));
	}
	dns_diff_clear(&diff);
	if (ver != NULL) {
		dns_db_closeversion(kfetch->db, &ver, commit);
	}

 cleanup:
	dns_db_detach(&kfetch->db);

	INSIST(zone->irefs > 0);
	zone->irefs--;
	kfetch->zone = NULL;

	if (dns_rdataset_isassociated(&kfetch->keydataset)) {
		dns_rdataset_disassociate(&kfetch->keydataset);
	}
	if (dns_rdataset_isassociated(&kfetch->dnskeyset)) {
		dns_rdataset_disassociate(&kfetch->dnskeyset);
	}
	if (dns_rdataset_isassociated(&kfetch->dnskeysigset)) {
		dns_rdataset_disassociate(&kfetch->dnskeysigset);
	}

	dns_name_free(keyname, mctx);
	isc_mem_put(mctx, kfetch, sizeof(dns_keyfetch_t));
	isc_mem_detach(&mctx);

	if (secroots != NULL) {
		dns_keytable_detach(&secroots);
	}

	free_needed = exit_check(zone);
	UNLOCK_ZONE(zone);
	if (free_needed) {
		zone_free(zone);
	}

	INSIST(ver == NULL);
}