networkstatus_parse_vote_from_string(const char *s, const char **eos_out,
                                     networkstatus_type_t ns_type)
{
  smartlist_t *tokens = smartlist_create();
  smartlist_t *rs_tokens = NULL, *footer_tokens = NULL;
  networkstatus_voter_info_t *voter = NULL;
  networkstatus_t *ns = NULL;
  digests_t ns_digests;
  const char *cert, *end_of_header, *end_of_footer, *s_dup = s;
  directory_token_t *tok;
  int ok;
  struct in_addr in;
  int i, inorder, n_signatures = 0;
  memarea_t *area = NULL, *rs_area = NULL;
  consensus_flavor_t flav = FLAV_NS;

  tor_assert(s);

  if (eos_out)
    *eos_out = NULL;

  if (router_get_networkstatus_v3_hashes(s, &ns_digests)) {
    log_warn(LD_DIR, "Unable to compute digest of network-status");
    goto err;
  }

  area = memarea_new();
  end_of_header = find_start_of_next_routerstatus(s);
  if (tokenize_string(area, s, end_of_header, tokens,
                      (ns_type == NS_TYPE_CONSENSUS) ?
                      networkstatus_consensus_token_table :
                      networkstatus_token_table, 0)) {
    log_warn(LD_DIR, "Error tokenizing network-status vote header");
    goto err;
  }

  ns = tor_malloc_zero(sizeof(networkstatus_t));
  memcpy(&ns->digests, &ns_digests, sizeof(ns_digests));

  tok = find_by_keyword(tokens, K_NETWORK_STATUS_VERSION);
  tor_assert(tok);
  if (tok->n_args > 1) {
    int flavor = networkstatus_parse_flavor_name(tok->args[1]);
    if (flavor < 0) {
      log_warn(LD_DIR, "Can't parse document with unknown flavor %s",
               escaped(tok->args[1]));
      goto err;
    }
    ns->flavor = flav = flavor;
  }
  if (flav != FLAV_NS && ns_type != NS_TYPE_CONSENSUS) {
    log_warn(LD_DIR, "Flavor found on non-consensus networkstatus.");
    goto err;
  }

  if (ns_type != NS_TYPE_CONSENSUS) {
    const char *end_of_cert = NULL;
    if (!(cert = strstr(s, "\ndir-key-certificate-version")))
      goto err;
    ++cert;
    ns->cert = authority_cert_parse_from_string(cert, &end_of_cert);
    if (!ns->cert || !end_of_cert || end_of_cert > end_of_header)
      goto err;
  }

  tok = find_by_keyword(tokens, K_VOTE_STATUS);
  tor_assert(tok->n_args);
  if (!strcmp(tok->args[0], "vote")) {
    ns->type = NS_TYPE_VOTE;
  } else if (!strcmp(tok->args[0], "consensus")) {
    ns->type = NS_TYPE_CONSENSUS;
  } else if (!strcmp(tok->args[0], "opinion")) {
    ns->type = NS_TYPE_OPINION;
  } else {
    log_warn(LD_DIR, "Unrecognized vote status %s in network-status",
             escaped(tok->args[0]));
    goto err;
  }
  if (ns_type != ns->type) {
    log_warn(LD_DIR, "Got the wrong kind of v3 networkstatus.");
    goto err;
  }

  if (ns->type == NS_TYPE_VOTE || ns->type == NS_TYPE_OPINION) {
    tok = find_by_keyword(tokens, K_PUBLISHED);
    if (parse_iso_time(tok->args[0], &ns->published))
      goto err;

    ns->supported_methods = smartlist_create();
    tok = find_opt_by_keyword(tokens, K_CONSENSUS_METHODS);
    if (tok) {
      for (i=0; i < tok->n_args; ++i)
        smartlist_add(ns->supported_methods, tor_strdup(tok->args[i]));
    } else {
      smartlist_add(ns->supported_methods, tor_strdup("1"));
    }
  } else {
    tok = find_opt_by_keyword(tokens, K_CONSENSUS_METHOD);
    if (tok) {
      ns->consensus_method = (int)tor_parse_long(tok->args[0], 10, 1, INT_MAX,
                                                 &ok, NULL);
      if (!ok)
        goto err;
    } else {
      ns->consensus_method = 1;
    }
  }

  tok = find_by_keyword(tokens, K_VALID_AFTER);
  if (parse_iso_time(tok->args[0], &ns->valid_after))
    goto err;

  tok = find_by_keyword(tokens, K_FRESH_UNTIL);
  if (parse_iso_time(tok->args[0], &ns->fresh_until))
    goto err;

  tok = find_by_keyword(tokens, K_VALID_UNTIL);
  if (parse_iso_time(tok->args[0], &ns->valid_until))
    goto err;

  tok = find_by_keyword(tokens, K_VOTING_DELAY);
  tor_assert(tok->n_args >= 2);
  ns->vote_seconds =
    (int) tor_parse_long(tok->args[0], 10, 0, INT_MAX, &ok, NULL);
  if (!ok)
    goto err;
  ns->dist_seconds =
    (int) tor_parse_long(tok->args[1], 10, 0, INT_MAX, &ok, NULL);
  if (!ok)
    goto err;
  if (ns->valid_after + MIN_VOTE_INTERVAL > ns->fresh_until) {
    log_warn(LD_DIR, "Vote/consensus freshness interval is too short");
    goto err;
  }
  if (ns->valid_after + MIN_VOTE_INTERVAL*2 > ns->valid_until) {
    log_warn(LD_DIR, "Vote/consensus liveness interval is too short");
    goto err;
  }
  if (ns->vote_seconds < MIN_VOTE_SECONDS) {
    log_warn(LD_DIR, "Vote seconds is too short");
    goto err;
  }
  if (ns->dist_seconds < MIN_DIST_SECONDS) {
    log_warn(LD_DIR, "Dist seconds is too short");
    goto err;
  }

  if ((tok = find_opt_by_keyword(tokens, K_CLIENT_VERSIONS))) {
    ns->client_versions = tor_strdup(tok->args[0]);
  }
  if ((tok = find_opt_by_keyword(tokens, K_SERVER_VERSIONS))) {
    ns->server_versions = tor_strdup(tok->args[0]);
  }

  tok = find_by_keyword(tokens, K_KNOWN_FLAGS);
  ns->known_flags = smartlist_create();
  inorder = 1;
  for (i = 0; i < tok->n_args; ++i) {
    smartlist_add(ns->known_flags, tor_strdup(tok->args[i]));
    if (i>0 && strcmp(tok->args[i-1], tok->args[i])>= 0) {
      log_warn(LD_DIR, "%s >= %s", tok->args[i-1], tok->args[i]);
      inorder = 0;
    }
  }
  if (!inorder) {
    log_warn(LD_DIR, "known-flags not in order");
    goto err;
  }

  tok = find_opt_by_keyword(tokens, K_PARAMS);
  if (tok) {
    inorder = 1;
    ns->net_params = smartlist_create();
    for (i = 0; i < tok->n_args; ++i) {
      int ok=0;
      char *eq = strchr(tok->args[i], '=');
      if (!eq) {
        log_warn(LD_DIR, "Bad element '%s' in params", escaped(tok->args[i]));
        goto err;
      }
      tor_parse_long(eq+1, 10, INT32_MIN, INT32_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_DIR, "Bad element '%s' in params", escaped(tok->args[i]));
        goto err;
      }
      if (i > 0 && strcmp(tok->args[i-1], tok->args[i]) >= 0) {
        log_warn(LD_DIR, "%s >= %s", tok->args[i-1], tok->args[i]);
        inorder = 0;
      }
      smartlist_add(ns->net_params, tor_strdup(tok->args[i]));
    }
    if (!inorder) {
      log_warn(LD_DIR, "params not in order");
      goto err;
    }
  }

  ns->voters = smartlist_create();

  SMARTLIST_FOREACH_BEGIN(tokens, directory_token_t *, _tok) {
    tok = _tok;
    if (tok->tp == K_DIR_SOURCE) {
      tor_assert(tok->n_args >= 6);

      if (voter)
        smartlist_add(ns->voters, voter);
      voter = tor_malloc_zero(sizeof(networkstatus_voter_info_t));
      voter->sigs = smartlist_create();
      if (ns->type != NS_TYPE_CONSENSUS)
        memcpy(voter->vote_digest, ns_digests.d[DIGEST_SHA1], DIGEST_LEN);

      voter->nickname = tor_strdup(tok->args[0]);
      if (strlen(tok->args[1]) != HEX_DIGEST_LEN ||
          base16_decode(voter->identity_digest, sizeof(voter->identity_digest),
                        tok->args[1], HEX_DIGEST_LEN) < 0) {
        log_warn(LD_DIR, "Error decoding identity digest %s in "
                 "network-status vote.", escaped(tok->args[1]));
        goto err;
      }
      if (ns->type != NS_TYPE_CONSENSUS &&
          tor_memneq(ns->cert->cache_info.identity_digest,
                 voter->identity_digest, DIGEST_LEN)) {
        log_warn(LD_DIR,"Mismatch between identities in certificate and vote");
        goto err;
      }
      voter->address = tor_strdup(tok->args[2]);
      if (!tor_inet_aton(tok->args[3], &in)) {
        log_warn(LD_DIR, "Error decoding IP address %s in network-status.",
                 escaped(tok->args[3]));
        goto err;
      }
      voter->addr = ntohl(in.s_addr);
      voter->dir_port = (uint16_t)
        tor_parse_long(tok->args[4], 10, 0, 65535, &ok, NULL);
      if (!ok)
        goto err;
      voter->or_port = (uint16_t)
        tor_parse_long(tok->args[5], 10, 0, 65535, &ok, NULL);
      if (!ok)
        goto err;
    } else if (tok->tp == K_CONTACT) {
      if (!voter || voter->contact) {
        log_warn(LD_DIR, "contact element is out of place.");
        goto err;
      }
      voter->contact = tor_strdup(tok->args[0]);
    } else if (tok->tp == K_VOTE_DIGEST) {
      tor_assert(ns->type == NS_TYPE_CONSENSUS);
      tor_assert(tok->n_args >= 1);
      if (!voter || ! tor_digest_is_zero(voter->vote_digest)) {
        log_warn(LD_DIR, "vote-digest element is out of place.");
        goto err;
      }
      if (strlen(tok->args[0]) != HEX_DIGEST_LEN ||
        base16_decode(voter->vote_digest, sizeof(voter->vote_digest),
                      tok->args[0], HEX_DIGEST_LEN) < 0) {
        log_warn(LD_DIR, "Error decoding vote digest %s in "
                 "network-status consensus.", escaped(tok->args[0]));
        goto err;
      }
    }
  } SMARTLIST_FOREACH_END(_tok);
  if (voter) {
    smartlist_add(ns->voters, voter);
    voter = NULL;
  }
  if (smartlist_len(ns->voters) == 0) {
    log_warn(LD_DIR, "Missing dir-source elements in a vote networkstatus.");
    goto err;
  } else if (ns->type != NS_TYPE_CONSENSUS && smartlist_len(ns->voters) != 1) {
    log_warn(LD_DIR, "Too many dir-source elements in a vote networkstatus.");
    goto err;
  }

  if (ns->type != NS_TYPE_CONSENSUS &&
      (tok = find_opt_by_keyword(tokens, K_LEGACY_DIR_KEY))) {
    int bad = 1;
    if (strlen(tok->args[0]) == HEX_DIGEST_LEN) {
      networkstatus_voter_info_t *voter = smartlist_get(ns->voters, 0);
      if (base16_decode(voter->legacy_id_digest, DIGEST_LEN,
                        tok->args[0], HEX_DIGEST_LEN)<0)
        bad = 1;
      else
        bad = 0;
    }
    if (bad) {
      log_warn(LD_DIR, "Invalid legacy key digest %s on vote.",
               escaped(tok->args[0]));
    }
  }

  /* Parse routerstatus lines. */
  rs_tokens = smartlist_create();
  rs_area = memarea_new();
  s = end_of_header;
  ns->routerstatus_list = smartlist_create();

  while (!strcmpstart(s, "r ")) {
    if (ns->type != NS_TYPE_CONSENSUS) {
      vote_routerstatus_t *rs = tor_malloc_zero(sizeof(vote_routerstatus_t));
      if (routerstatus_parse_entry_from_string(rs_area, &s, rs_tokens, ns,
                                               rs, 0, 0))
        smartlist_add(ns->routerstatus_list, rs);
      else {
        tor_free(rs->version);
        tor_free(rs);
      }
    } else {
      routerstatus_t *rs;
      if ((rs = routerstatus_parse_entry_from_string(rs_area, &s, rs_tokens,
                                                     NULL, NULL,
                                                     ns->consensus_method,
                                                     flav)))
        smartlist_add(ns->routerstatus_list, rs);
    }
  }
  for (i = 1; i < smartlist_len(ns->routerstatus_list); ++i) {
    routerstatus_t *rs1, *rs2;
    if (ns->type != NS_TYPE_CONSENSUS) {
      vote_routerstatus_t *a = smartlist_get(ns->routerstatus_list, i-1);
      vote_routerstatus_t *b = smartlist_get(ns->routerstatus_list, i);
      rs1 = &a->status; rs2 = &b->status;
    } else {
      rs1 = smartlist_get(ns->routerstatus_list, i-1);
      rs2 = smartlist_get(ns->routerstatus_list, i);
    }
    if (fast_memcmp(rs1->identity_digest, rs2->identity_digest, DIGEST_LEN)
        >= 0) {
      log_warn(LD_DIR, "Vote networkstatus entries not sorted by identity "
               "digest");
      goto err;
    }
  }

  /* Parse footer; check signature. */
  footer_tokens = smartlist_create();
  if ((end_of_footer = strstr(s, "\nnetwork-status-version ")))
    ++end_of_footer;
  else
    end_of_footer = s + strlen(s);
  if (tokenize_string(area,s, end_of_footer, footer_tokens,
                      networkstatus_vote_footer_token_table, 0)) {
    log_warn(LD_DIR, "Error tokenizing network-status vote footer.");
    goto err;
  }

  {
    int found_sig = 0;
    SMARTLIST_FOREACH_BEGIN(footer_tokens, directory_token_t *, _tok) {
      tok = _tok;
      if (tok->tp == K_DIRECTORY_SIGNATURE)
        found_sig = 1;
      else if (found_sig) {
        log_warn(LD_DIR, "Extraneous token after first directory-signature");
        goto err;
      }
    } SMARTLIST_FOREACH_END(_tok);
  }

  if ((tok = find_opt_by_keyword(footer_tokens, K_DIRECTORY_FOOTER))) {
    if (tok != smartlist_get(footer_tokens, 0)) {
      log_warn(LD_DIR, "Misplaced directory-footer token");
      goto err;
    }
  }

  tok = find_opt_by_keyword(footer_tokens, K_BW_WEIGHTS);
  if (tok) {
    ns->weight_params = smartlist_create();
    for (i = 0; i < tok->n_args; ++i) {
      int ok=0;
      char *eq = strchr(tok->args[i], '=');
      if (!eq) {
        log_warn(LD_DIR, "Bad element '%s' in weight params",
                 escaped(tok->args[i]));
        goto err;
      }
      tor_parse_long(eq+1, 10, INT32_MIN, INT32_MAX, &ok, NULL);
      if (!ok) {
        log_warn(LD_DIR, "Bad element '%s' in params", escaped(tok->args[i]));
        goto err;
      }
      smartlist_add(ns->weight_params, tor_strdup(tok->args[i]));
    }
  }

  SMARTLIST_FOREACH_BEGIN(footer_tokens, directory_token_t *, _tok) {
    char declared_identity[DIGEST_LEN];
    networkstatus_voter_info_t *v;
    document_signature_t *sig;
    const char *id_hexdigest = NULL;
    const char *sk_hexdigest = NULL;
    digest_algorithm_t alg = DIGEST_SHA1;
    tok = _tok;
    if (tok->tp != K_DIRECTORY_SIGNATURE)
      continue;
    tor_assert(tok->n_args >= 2);
    if (tok->n_args == 2) {
      id_hexdigest = tok->args[0];
      sk_hexdigest = tok->args[1];
    } else {
      const char *algname = tok->args[0];
      int a;
      id_hexdigest = tok->args[1];
      sk_hexdigest = tok->args[2];
      a = crypto_digest_algorithm_parse_name(algname);
      if (a<0) {
        log_warn(LD_DIR, "Unknown digest algorithm %s; skipping",
                 escaped(algname));
        continue;
      }
      alg = a;
    }

    if (!tok->object_type ||
        strcmp(tok->object_type, "SIGNATURE") ||
        tok->object_size < 128 || tok->object_size > 512) {
      log_warn(LD_DIR, "Bad object type or length on directory-signature");
      goto err;
    }

    if (strlen(id_hexdigest) != HEX_DIGEST_LEN ||
        base16_decode(declared_identity, sizeof(declared_identity),
                      id_hexdigest, HEX_DIGEST_LEN) < 0) {
      log_warn(LD_DIR, "Error decoding declared identity %s in "
               "network-status vote.", escaped(id_hexdigest));
      goto err;
    }
    if (!(v = networkstatus_get_voter_by_id(ns, declared_identity))) {
      log_warn(LD_DIR, "ID on signature on network-status vote does not match "
               "any declared directory source.");
      goto err;
    }
    sig = tor_malloc_zero(sizeof(document_signature_t));
    memcpy(sig->identity_digest, v->identity_digest, DIGEST_LEN);
    sig->alg = alg;
    if (strlen(sk_hexdigest) != HEX_DIGEST_LEN ||
        base16_decode(sig->signing_key_digest, sizeof(sig->signing_key_digest),
                      sk_hexdigest, HEX_DIGEST_LEN) < 0) {
      log_warn(LD_DIR, "Error decoding declared signing key digest %s in "
               "network-status vote.", escaped(sk_hexdigest));
      tor_free(sig);
      goto err;
    }

    if (ns->type != NS_TYPE_CONSENSUS) {
      if (tor_memneq(declared_identity, ns->cert->cache_info.identity_digest,
                 DIGEST_LEN)) {
        log_warn(LD_DIR, "Digest mismatch between declared and actual on "
                 "network-status vote.");
        tor_free(sig);
        goto err;
      }
    }

    if (voter_get_sig_by_algorithm(v, sig->alg)) {
      /* We already parsed a vote with this algorithm from this voter. Use the
         first one. */
      log_fn(LOG_PROTOCOL_WARN, LD_DIR, "We received a networkstatus "
             "that contains two votes from the same voter with the same "
             "algorithm. Ignoring the second vote.");
      tor_free(sig);
      continue;
    }

    if (ns->type != NS_TYPE_CONSENSUS) {
      if (check_signature_token(ns_digests.d[DIGEST_SHA1], DIGEST_LEN,
                                tok, ns->cert->signing_key, 0,
                                "network-status vote")) {
        tor_free(sig);
        goto err;
      }
      sig->good_signature = 1;
    } else {
      if (tok->object_size >= INT_MAX || tok->object_size >= SIZE_T_CEILING) {
        tor_free(sig);
        goto err;
      }
      sig->signature = tor_memdup(tok->object_body, tok->object_size);
      sig->signature_len = (int) tok->object_size;
    }
    smartlist_add(v->sigs, sig);

    ++n_signatures;
  } SMARTLIST_FOREACH_END(_tok);

  if (! n_signatures) {
    log_warn(LD_DIR, "No signatures on networkstatus vote.");
    goto err;
  } else if (ns->type == NS_TYPE_VOTE && n_signatures != 1) {
    log_warn(LD_DIR, "Received more than one signature on a "
             "network-status vote.");
    goto err;
  }

  if (eos_out)
    *eos_out = end_of_footer;

  goto done;
 err:
  dump_desc(s_dup, "v3 networkstatus");
  networkstatus_vote_free(ns);
  ns = NULL;
 done:
  if (tokens) {
    SMARTLIST_FOREACH(tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(tokens);
  }
  if (voter) {
    if (voter->sigs) {
      SMARTLIST_FOREACH(voter->sigs, document_signature_t *, sig,
                        document_signature_free(sig));
      smartlist_free(voter->sigs);
    }
    tor_free(voter->nickname);
    tor_free(voter->address);
    tor_free(voter->contact);
    tor_free(voter);
  }
  if (rs_tokens) {
    SMARTLIST_FOREACH(rs_tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(rs_tokens);
  }
  if (footer_tokens) {
    SMARTLIST_FOREACH(footer_tokens, directory_token_t *, t, token_clear(t));
    smartlist_free(footer_tokens);
  }
  if (area) {
    DUMP_AREA(area, "v3 networkstatus");
    memarea_drop_all(area);
  }
  if (rs_area)
    memarea_drop_all(rs_area);

  return ns;
}