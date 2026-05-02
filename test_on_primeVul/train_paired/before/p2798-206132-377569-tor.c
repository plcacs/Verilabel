connection_ap_handshake_rewrite_and_attach(entry_connection_t *conn,
                                           origin_circuit_t *circ,
                                           crypt_path_t *cpath)
{
  socks_request_t *socks = conn->socks_request;
  const or_options_t *options = get_options();
  connection_t *base_conn = ENTRY_TO_CONN(conn);
  time_t now = time(NULL);
  rewrite_result_t rr;

  /* First we'll do the rewrite part.  Let's see if we get a reasonable
   * answer.
   */
  memset(&rr, 0, sizeof(rr));
  connection_ap_handshake_rewrite(conn,&rr);

  if (rr.should_close) {
    /* connection_ap_handshake_rewrite told us to close the connection:
     * either because it sent back an answer, or because it sent back an
     * error */
    connection_mark_unattached_ap(conn, rr.end_reason);
    if (END_STREAM_REASON_DONE == (rr.end_reason & END_STREAM_REASON_MASK))
      return 0;
    else
      return -1;
  }

  const time_t map_expires = rr.map_expires;
  const int automap = rr.automap;
  const addressmap_entry_source_t exit_source = rr.exit_source;

  /* Now see whether the hostname is bogus.  This could happen because of an
   * onion hostname whose format we don't recognize. */
  hostname_type_t addresstype;
  if (!parse_extended_hostname(socks->address, &addresstype)) {
    control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                escaped(socks->address));
    if (addresstype == BAD_HOSTNAME) {
      conn->socks_request->socks_extended_error_code = SOCKS5_HS_BAD_ADDRESS;
    }
    connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
    return -1;
  }

  /* If this is a .exit hostname, strip off the .name.exit part, and
   * see whether we're willing to connect there, and and otherwise handle the
   * .exit address.
   *
   * We'll set chosen_exit_name and/or close the connection as appropriate.
   */
  if (addresstype == EXIT_HOSTNAME) {
    /* If StrictNodes is not set, then .exit overrides ExcludeNodes but
     * not ExcludeExitNodes. */
    routerset_t *excludeset = options->StrictNodes ?
      options->ExcludeExitNodesUnion_ : options->ExcludeExitNodes;
    const node_t *node = NULL;

    /* If this .exit was added by an AUTOMAP, then it came straight from
     * a user.  That's not safe. */
    if (exit_source == ADDRMAPSRC_AUTOMAP) {
      /* Whoops; this one is stale.  It must have gotten added earlier?
       * (Probably this is not possible, since AllowDotExit no longer
       * exists.) */
      log_warn(LD_APP,"Stale automapped address for '%s.exit'. Refusing.",
               safe_str_client(socks->address));
      control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                  escaped(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      tor_assert_nonfatal_unreached();
      return -1;
    }

    /* Double-check to make sure there are no .exits coming from
     * impossible/weird sources. */
    if (exit_source == ADDRMAPSRC_DNS || exit_source == ADDRMAPSRC_NONE) {
      /* It shouldn't be possible to get a .exit address from any of these
       * sources. */
      log_warn(LD_BUG,"Address '%s.exit', with impossible source for the "
               ".exit part. Refusing.",
               safe_str_client(socks->address));
      control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                  escaped(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }

    tor_assert(!automap);

    /* Now, find the character before the .(name) part.
     * (The ".exit" part got stripped off by "parse_extended_hostname").
     *
     * We're going to put the exit name into conn->chosen_exit_name, and
     * look up a node correspondingly. */
    char *s = strrchr(socks->address,'.');
    if (s) {
      /* The address was of the form "(stuff).(name).exit */
      if (s[1] != '\0') {
        /* Looks like a real .exit one. */
        conn->chosen_exit_name = tor_strdup(s+1);
        node = node_get_by_nickname(conn->chosen_exit_name, 0);

        if (exit_source == ADDRMAPSRC_TRACKEXIT) {
          /* We 5 tries before it expires the addressmap */
          conn->chosen_exit_retries = TRACKHOSTEXITS_RETRIES;
        }
        *s = 0;
      } else {
        /* Oops, the address was (stuff)..exit.  That's not okay. */
        log_warn(LD_APP,"Malformed exit address '%s.exit'. Refusing.",
                 safe_str_client(socks->address));
        control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                    escaped(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
        return -1;
      }
    } else {
      /* It looks like they just asked for "foo.exit".  That's a special
       * form that means (foo's address).foo.exit. */

      conn->chosen_exit_name = tor_strdup(socks->address);
      node = node_get_by_nickname(conn->chosen_exit_name, 0);
      if (node) {
        *socks->address = 0;
        node_get_address_string(node, socks->address, sizeof(socks->address));
      }
    }

    /* Now make sure that the chosen exit exists... */
    if (!node) {
      log_warn(LD_APP,
               "Unrecognized relay in exit address '%s.exit'. Refusing.",
               safe_str_client(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }
    /* ...and make sure that it isn't excluded. */
    if (routerset_contains_node(excludeset, node)) {
      log_warn(LD_APP,
               "Excluded relay in exit address '%s.exit'. Refusing.",
               safe_str_client(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }
    /* XXXX-1090 Should we also allow foo.bar.exit if ExitNodes is set and
       Bar is not listed in it?  I say yes, but our revised manpage branch
       implies no. */
  }

  /* Now, we handle everything that isn't a .onion address. */
  if (addresstype != ONION_V2_HOSTNAME && addresstype != ONION_V3_HOSTNAME) {
    /* Not a hidden-service request.  It's either a hostname or an IP,
     * possibly with a .exit that we stripped off.  We're going to check
     * if we're allowed to connect/resolve there, and then launch the
     * appropriate request. */

    /* Check for funny characters in the address. */
    if (address_is_invalid_destination(socks->address, 1)) {
      control_event_client_status(LOG_WARN, "SOCKS_BAD_HOSTNAME HOSTNAME=%s",
                                  escaped(socks->address));
      log_warn(LD_APP,
               "Destination '%s' seems to be an invalid hostname. Failing.",
               safe_str_client(socks->address));
      connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
      return -1;
    }

    /* socks->address is a non-onion hostname or IP address.
     * If we can't do any non-onion requests, refuse the connection.
     * If we have a hostname but can't do DNS, refuse the connection.
     * If we have an IP address, but we can't use that address family,
     * refuse the connection.
     *
     * If we can do DNS requests, and we can use at least one address family,
     * then we have to resolve the address first. Then we'll know if it
     * resolves to a usable address family. */

    /* First, check if all non-onion traffic is disabled */
    if (!conn->entry_cfg.dns_request && !conn->entry_cfg.ipv4_traffic
        && !conn->entry_cfg.ipv6_traffic) {
        log_warn(LD_APP, "Refusing to connect to non-hidden-service hostname "
                 "or IP address %s because Port has OnionTrafficOnly set (or "
                 "NoDNSRequest, NoIPv4Traffic, and NoIPv6Traffic).",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
    }

    /* Then check if we have a hostname or IP address, and whether DNS or
     * the IP address family are permitted.  Reject if not. */
    tor_addr_t dummy_addr;
    int socks_family = tor_addr_parse(&dummy_addr, socks->address);
    /* family will be -1 for a non-onion hostname that's not an IP */
    if (socks_family == -1) {
      if (!conn->entry_cfg.dns_request) {
        log_warn(LD_APP, "Refusing to connect to hostname %s "
                 "because Port has NoDNSRequest set.",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
      }
    } else if (socks_family == AF_INET) {
      if (!conn->entry_cfg.ipv4_traffic) {
        log_warn(LD_APP, "Refusing to connect to IPv4 address %s because "
                 "Port has NoIPv4Traffic set.",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
      }
    } else if (socks_family == AF_INET6) {
      if (!conn->entry_cfg.ipv6_traffic) {
        log_warn(LD_APP, "Refusing to connect to IPv6 address %s because "
                 "Port has NoIPv6Traffic set.",
                 safe_str_client(socks->address));
        connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
        return -1;
      }
    } else {
      tor_assert_nonfatal_unreached_once();
    }

    /* See if this is a hostname lookup that we can answer immediately.
     * (For example, an attempt to look up the IP address for an IP address.)
     */
    if (socks->command == SOCKS_COMMAND_RESOLVE) {
      tor_addr_t answer;
      /* Reply to resolves immediately if we can. */
      if (tor_addr_parse(&answer, socks->address) >= 0) {/* is it an IP? */
        /* remember _what_ is supposed to have been resolved. */
        strlcpy(socks->address, rr.orig_address, sizeof(socks->address));
        connection_ap_handshake_socks_resolved_addr(conn, &answer, -1,
                                                    map_expires);
        connection_mark_unattached_ap(conn,
                                END_STREAM_REASON_DONE |
                                END_STREAM_REASON_FLAG_ALREADY_SOCKS_REPLIED);
        return 0;
      }
      tor_assert(!automap);
      rep_hist_note_used_resolve(now); /* help predict this next time */
    } else if (socks->command == SOCKS_COMMAND_CONNECT) {
      /* Now see if this is a connect request that we can reject immediately */

      tor_assert(!automap);
      /* Don't allow connections to port 0. */
      if (socks->port == 0) {
        log_notice(LD_APP,"Application asked to connect to port 0. Refusing.");
        connection_mark_unattached_ap(conn, END_STREAM_REASON_TORPROTOCOL);
        return -1;
      }
      /* You can't make connections to internal addresses, by default.
       * Exceptions are begindir requests (where the address is meaningless),
       * or cases where you've hand-configured a particular exit, thereby
       * making the local address meaningful. */
      if (options->ClientRejectInternalAddresses &&
          !conn->use_begindir && !conn->chosen_exit_name && !circ) {
        /* If we reach this point then we don't want to allow internal
         * addresses.  Check if we got one. */
        tor_addr_t addr;
        if (tor_addr_hostname_is_local(socks->address) ||
            (tor_addr_parse(&addr, socks->address) >= 0 &&
             tor_addr_is_internal(&addr, 0))) {
          /* If this is an explicit private address with no chosen exit node,
           * then we really don't want to try to connect to it.  That's
           * probably an error. */
          if (conn->is_transparent_ap) {
#define WARN_INTRVL_LOOP 300
            static ratelim_t loop_warn_limit = RATELIM_INIT(WARN_INTRVL_LOOP);
            char *m;
            if ((m = rate_limit_log(&loop_warn_limit, approx_time()))) {
              log_warn(LD_NET,
                       "Rejecting request for anonymous connection to private "
                       "address %s on a TransPort or NATDPort.  Possible loop "
                       "in your NAT rules?%s", safe_str_client(socks->address),
                       m);
              tor_free(m);
            }
          } else {
#define WARN_INTRVL_PRIV 300
            static ratelim_t priv_warn_limit = RATELIM_INIT(WARN_INTRVL_PRIV);
            char *m;
            if ((m = rate_limit_log(&priv_warn_limit, approx_time()))) {
              log_warn(LD_NET,
                       "Rejecting SOCKS request for anonymous connection to "
                       "private address %s.%s",
                       safe_str_client(socks->address),m);
              tor_free(m);
            }
          }
          connection_mark_unattached_ap(conn, END_STREAM_REASON_PRIVATE_ADDR);
          return -1;
        }
      } /* end "if we should check for internal addresses" */

      /* Okay.  We're still doing a CONNECT, and it wasn't a private
       * address.  Here we do special handling for literal IP addresses,
       * to see if we should reject this preemptively, and to set up
       * fields in conn->entry_cfg to tell the exit what AF we want. */
      {
        tor_addr_t addr;
        /* XXX Duplicate call to tor_addr_parse. */
        if (tor_addr_parse(&addr, socks->address) >= 0) {
          /* If we reach this point, it's an IPv4 or an IPv6 address. */
          sa_family_t family = tor_addr_family(&addr);

          if ((family == AF_INET && ! conn->entry_cfg.ipv4_traffic) ||
              (family == AF_INET6 && ! conn->entry_cfg.ipv6_traffic)) {
            /* You can't do an IPv4 address on a v6-only socks listener,
             * or vice versa. */
            log_warn(LD_NET, "Rejecting SOCKS request for an IP address "
                     "family that this listener does not support.");
            connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
            return -1;
          } else if (family == AF_INET6 && socks->socks_version == 4) {
            /* You can't make a socks4 request to an IPv6 address. Socks4
             * doesn't support that. */
            log_warn(LD_NET, "Rejecting SOCKS4 request for an IPv6 address.");
            connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
            return -1;
          } else if (socks->socks_version == 4 &&
                     !conn->entry_cfg.ipv4_traffic) {
            /* You can't do any kind of Socks4 request when IPv4 is forbidden.
             *
             * XXX raise this check outside the enclosing block? */
            log_warn(LD_NET, "Rejecting SOCKS4 request on a listener with "
                     "no IPv4 traffic supported.");
            connection_mark_unattached_ap(conn, END_STREAM_REASON_ENTRYPOLICY);
            return -1;
          } else if (family == AF_INET6) {
            /* Tell the exit: we won't accept any ipv4 connection to an IPv6
             * address. */
            conn->entry_cfg.ipv4_traffic = 0;
          } else if (family == AF_INET) {
            /* Tell the exit: we won't accept any ipv6 connection to an IPv4
             * address. */
            conn->entry_cfg.ipv6_traffic = 0;
          }
        }
      }

      /* we never allow IPv6 answers on socks4. (TODO: Is this smart?) */
      if (socks->socks_version == 4)
        conn->entry_cfg.ipv6_traffic = 0;

      /* Still handling CONNECT. Now, check for exit enclaves.  (Which we
       * don't do on BEGIN_DIR, or when there is a chosen exit.)
       *
       * TODO: Should we remove this?  Exit enclaves are nutty and don't
       * work very well
       */
      if (!conn->use_begindir && !conn->chosen_exit_name && !circ) {
        /* see if we can find a suitable enclave exit */
        const node_t *r =
          router_find_exact_exit_enclave(socks->address, socks->port);
        if (r) {
          log_info(LD_APP,
                   "Redirecting address %s to exit at enclave router %s",
                   safe_str_client(socks->address), node_describe(r));
          /* use the hex digest, not nickname, in case there are two
             routers with this nickname */
          conn->chosen_exit_name =
            tor_strdup(hex_str(r->identity, DIGEST_LEN));
          conn->chosen_exit_optional = 1;
        }
      }

      /* Still handling CONNECT: warn or reject if it's using a dangerous
       * port. */
      if (!conn->use_begindir && !conn->chosen_exit_name && !circ)
        if (consider_plaintext_ports(conn, socks->port) < 0)
          return -1;

      /* Remember the port so that we will predict that more requests
         there will happen in the future. */
      if (!conn->use_begindir) {
        /* help predict this next time */
        rep_hist_note_used_port(now, socks->port);
      }
    } else if (socks->command == SOCKS_COMMAND_RESOLVE_PTR) {
      rep_hist_note_used_resolve(now); /* help predict this next time */
      /* no extra processing needed */
    } else {
      /* We should only be doing CONNECT, RESOLVE, or RESOLVE_PTR! */
      tor_fragile_assert();
    }

    /* Okay. At this point we've set chosen_exit_name if needed, rewritten the
     * address, and decided not to reject it for any number of reasons. Now
     * mark the connection as waiting for a circuit, and try to attach it!
     */
    base_conn->state = AP_CONN_STATE_CIRCUIT_WAIT;

    /* If we were given a circuit to attach to, try to attach. Otherwise,
     * try to find a good one and attach to that. */
    int rv;
    if (circ) {
      rv = connection_ap_handshake_attach_chosen_circuit(conn, circ, cpath);
    } else {
      /* We'll try to attach it at the next event loop, or whenever
       * we call connection_ap_attach_pending() */
      connection_ap_mark_as_pending_circuit(conn);
      rv = 0;
    }

    /* If the above function returned 0 then we're waiting for a circuit.
     * if it returned 1, we're attached.  Both are okay.  But if it returned
     * -1, there was an error, so make sure the connection is marked, and
     * return -1. */
    if (rv < 0) {
      if (!base_conn->marked_for_close)
        connection_mark_unattached_ap(conn, END_STREAM_REASON_CANT_ATTACH);
      return -1;
    }

    return 0;
  } else {
    /* If we get here, it's a request for a .onion address! */
    tor_assert(addresstype == ONION_V2_HOSTNAME ||
               addresstype == ONION_V3_HOSTNAME);
    tor_assert(!automap);
    return connection_ap_handle_onion(conn, socks, circ, addresstype);
  }

  return 0; /* unreached but keeps the compiler happy */
}