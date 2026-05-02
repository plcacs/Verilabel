negotiate_handshake_newstyle_options (void)
{
  GET_CONN;
  struct nbd_new_option new_option;
  size_t nr_options;
  bool list_seen = false;
  uint64_t version;
  uint32_t option;
  uint32_t optlen;
  struct nbd_export_name_option_reply handshake_finish;
  const char *optname;
  uint64_t exportsize;
  struct backend *b;

  for (nr_options = MAX_NR_OPTIONS; nr_options > 0; --nr_options) {
    CLEANUP_FREE char *data = NULL;

    if (conn_recv_full (&new_option, sizeof new_option,
                        "reading option: conn->recv: %m") == -1)
      return -1;

    version = be64toh (new_option.version);
    if (version != NBD_NEW_VERSION) {
      nbdkit_error ("unknown option version %" PRIx64
                    ", expecting %" PRIx64,
                    version, NBD_NEW_VERSION);
      return -1;
    }

    /* There is a maximum option length we will accept, regardless
     * of the option type.
     */
    optlen = be32toh (new_option.optlen);
    if (optlen > MAX_REQUEST_SIZE) {
      nbdkit_error ("client option data too long (%" PRIu32 ")", optlen);
      return -1;
    }
    data = malloc (optlen + 1); /* Allowing a trailing NUL helps some uses */
    if (data == NULL) {
      nbdkit_error ("malloc: %m");
      return -1;
    }

    option = be32toh (new_option.option);
    optname = name_of_nbd_opt (option);

    /* If the client lacks fixed newstyle support, it should only send
     * NBD_OPT_EXPORT_NAME.
     */
    if (!(conn->cflags & NBD_FLAG_FIXED_NEWSTYLE) &&
        option != NBD_OPT_EXPORT_NAME) {
      if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID))
        return -1;
      continue;
    }

    /* In --tls=require / FORCEDTLS mode the only options allowed
     * before TLS negotiation are NBD_OPT_ABORT and NBD_OPT_STARTTLS.
     */
    if (tls == 2 && !conn->using_tls &&
        !(option == NBD_OPT_ABORT || option == NBD_OPT_STARTTLS)) {
      if (send_newstyle_option_reply (option, NBD_REP_ERR_TLS_REQD))
        return -1;
      continue;
    }

    switch (option) {
    case NBD_OPT_EXPORT_NAME:
      if (conn_recv_full (data, optlen,
                          "read: %s: %m", name_of_nbd_opt (option)) == -1)
        return -1;
      if (check_export_name (option, data, optlen, optlen) == -1)
        return -1;

      /* We have to finish the handshake by sending handshake_finish.
       * On failure, we have to disconnect.
       */
      if (finish_newstyle_options (&exportsize, data, optlen) == -1)
        return -1;

      memset (&handshake_finish, 0, sizeof handshake_finish);
      handshake_finish.exportsize = htobe64 (exportsize);
      handshake_finish.eflags = htobe16 (conn->eflags);

      if (conn->send (&handshake_finish,
                      (conn->cflags & NBD_FLAG_NO_ZEROES)
                      ? offsetof (struct nbd_export_name_option_reply, zeroes)
                      : sizeof handshake_finish, 0) == -1) {
        nbdkit_error ("write: %s: %m", optname);
        return -1;
      }
      break;

    case NBD_OPT_ABORT:
      if (send_newstyle_option_reply (option, NBD_REP_ACK) == -1)
        return -1;
      debug ("client sent %s to abort the connection",
             name_of_nbd_opt (option));
      return -1;

    case NBD_OPT_LIST:
      if (optlen != 0) {
        if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
            == -1)
          return -1;
        if (conn_recv_full (data, optlen,
                            "read: %s: %m", name_of_nbd_opt (option)) == -1)
          return -1;
        continue;
      }

      if (list_seen) {
        debug ("newstyle negotiation: %s: export list already advertised",
               name_of_nbd_opt (option));
        if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID) == -1)
          return -1;
        continue;
      }
      else {
        /* Send back the exportname list. */
        debug ("newstyle negotiation: %s: advertising exports",
               name_of_nbd_opt (option));
        if (send_newstyle_option_reply_exportnames (option, &nr_options) == -1)
          return -1;
        list_seen = true;
      }
      break;

    case NBD_OPT_STARTTLS:
      if (optlen != 0) {
        if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
            == -1)
          return -1;
        if (conn_recv_full (data, optlen,
                            "read: %s: %m", name_of_nbd_opt (option)) == -1)
          return -1;
        continue;
      }

      if (tls == 0) {           /* --tls=off (NOTLS mode). */
#ifdef HAVE_GNUTLS
#define NO_TLS_REPLY NBD_REP_ERR_POLICY
#else
#define NO_TLS_REPLY NBD_REP_ERR_UNSUP
#endif
        if (send_newstyle_option_reply (option, NO_TLS_REPLY) == -1)
          return -1;
      }
      else /* --tls=on or --tls=require */ {
        /* We can't upgrade to TLS twice on the same connection. */
        if (conn->using_tls) {
          if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID) == -1)
            return -1;
          continue;
        }

        /* We have to send the (unencrypted) reply before starting
         * the handshake.
         */
        if (send_newstyle_option_reply (option, NBD_REP_ACK) == -1)
          return -1;

        /* Upgrade the connection to TLS.  Also performs access control. */
        if (crypto_negotiate_tls (conn->sockin, conn->sockout) == -1)
          return -1;
        conn->using_tls = true;
        debug ("using TLS on this connection");
        /* Wipe out any cached state. */
        conn->structured_replies = false;
        free (conn->exportname_from_set_meta_context);
        conn->exportname_from_set_meta_context = NULL;
        conn->meta_context_base_allocation = false;
        for_each_backend (b) {
          free (conn->default_exportname[b->i]);
          conn->default_exportname[b->i] = NULL;
        }
      }
      break;

    case NBD_OPT_INFO:
    case NBD_OPT_GO:
      if (conn_recv_full (data, optlen, "read: %s: %m", optname) == -1)
        return -1;

      if (optlen < 6) { /* 32 bit export length + 16 bit nr info */
        debug ("newstyle negotiation: %s option length < 6", optname);

        if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
            == -1)
          return -1;
        continue;
      }

      {
        uint32_t exportnamelen;
        uint16_t nrinfos;
        uint16_t info;
        size_t i;

        /* Validate the name length and number of INFO requests. */
        memcpy (&exportnamelen, &data[0], 4);
        exportnamelen = be32toh (exportnamelen);
        if (exportnamelen > optlen-6 /* NB optlen >= 6, see above */) {
          debug ("newstyle negotiation: %s: export name too long", optname);
          if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
              == -1)
            return -1;
          continue;
        }
        memcpy (&nrinfos, &data[exportnamelen+4], 2);
        nrinfos = be16toh (nrinfos);
        if (optlen != 4 + exportnamelen + 2 + 2*nrinfos) {
          debug ("newstyle negotiation: %s: "
                 "number of information requests incorrect", optname);
          if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
              == -1)
            return -1;
          continue;
        }

        /* As with NBD_OPT_EXPORT_NAME we print the export name and
         * save it in the connection.  If an earlier
         * NBD_OPT_SET_META_CONTEXT used an export name, it must match
         * or else we drop the support for that context.
         */
        if (check_export_name (option, &data[4], exportnamelen,
                               optlen - 6) == -1) {
          if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
              == -1)
            return -1;
          continue;
        }

        /* The spec is confusing, but it is required that we send back
         * NBD_INFO_EXPORT, even if the client did not request it!
         * qemu client in particular does not request this, but will
         * fail if we don't send it.  Note that if .open fails, but we
         * succeed at .close, then we merely return an error to the
         * client and let them try another NBD_OPT, rather than
         * disconnecting.
         */
        if (finish_newstyle_options (&exportsize,
                                     &data[4], exportnamelen) == -1) {
          if (conn->top_context) {
            if (backend_finalize (conn->top_context) == -1)
              return -1;
            backend_close (conn->top_context);
            conn->top_context = NULL;
          }
          if (send_newstyle_option_reply (option, NBD_REP_ERR_UNKNOWN) == -1)
            return -1;
          continue;
        }

        if (send_newstyle_option_reply_info_export (option,
                                                    NBD_REP_INFO,
                                                    NBD_INFO_EXPORT,
                                                    exportsize) == -1)
          return -1;

        /* For now we send NBD_INFO_NAME and NBD_INFO_DESCRIPTION if
         * requested, and ignore all other info requests (including
         * NBD_INFO_EXPORT if it was requested, because we replied
         * already above).
         */
        for (i = 0; i < nrinfos; ++i) {
          memcpy (&info, &data[4 + exportnamelen + 2 + i*2], 2);
          info = be16toh (info);
          switch (info) {
          case NBD_INFO_EXPORT: /* ignore - reply sent above */ break;
          case NBD_INFO_NAME:
            {
              const char *name = &data[4];
              size_t namelen = exportnamelen;

              if (exportnamelen == 0) {
                name = backend_default_export (top, read_only);
                if (!name) {
                  debug ("newstyle negotiation: %s: "
                         "NBD_INFO_NAME: no name to send", optname);
                  break;
                }
                namelen = -1;
              }
              if (send_newstyle_option_reply_info_str (option,
                                                       NBD_REP_INFO,
                                                       NBD_INFO_NAME,
                                                       name, namelen) == -1)
                return -1;
            }
            break;
          case NBD_INFO_DESCRIPTION:
            {
              const char *desc = backend_export_description (conn->top_context);

              if (!desc) {
                debug ("newstyle negotiation: %s: "
                       "NBD_INFO_DESCRIPTION: no description to send",
                       optname);
                break;
              }
              if (send_newstyle_option_reply_info_str (option,
                                                       NBD_REP_INFO,
                                                       NBD_INFO_DESCRIPTION,
                                                       desc, -1) == -1)
                return -1;
            }
            break;
          default:
            debug ("newstyle negotiation: %s: "
                   "ignoring NBD_INFO_* request %u (%s)",
                   optname, (unsigned) info, name_of_nbd_info (info));
            break;
          }
        }
      }

      /* Unlike NBD_OPT_EXPORT_NAME, NBD_OPT_GO sends back an ACK
       * or ERROR packet.  If this was NBD_OPT_LIST, call .close.
       */
      if (send_newstyle_option_reply (option, NBD_REP_ACK) == -1)
        return -1;

      if (option == NBD_OPT_INFO) {
        if (backend_finalize (conn->top_context) == -1)
          return -1;
        backend_close (conn->top_context);
        conn->top_context = NULL;
      }

      break;

    case NBD_OPT_STRUCTURED_REPLY:
      if (optlen != 0) {
        if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
            == -1)
          return -1;
        if (conn_recv_full (data, optlen,
                            "read: %s: %m", name_of_nbd_opt (option)) == -1)
          return -1;
        continue;
      }

      debug ("newstyle negotiation: %s: client requested structured replies",
             name_of_nbd_opt (option));

      if (no_sr) {
        /* Must fail with ERR_UNSUP for qemu 4.2 to remain happy;
         * but failing with ERR_POLICY would have been nicer.
         */
        if (send_newstyle_option_reply (option, NBD_REP_ERR_UNSUP) == -1)
          return -1;
        debug ("newstyle negotiation: %s: structured replies are disabled",
               name_of_nbd_opt (option));
        break;
      }

      if (send_newstyle_option_reply (option, NBD_REP_ACK) == -1)
        return -1;

      conn->structured_replies = true;
      break;

    case NBD_OPT_LIST_META_CONTEXT:
    case NBD_OPT_SET_META_CONTEXT:
      {
        uint32_t opt_index;
        uint32_t exportnamelen;
        uint32_t nr_queries;
        uint32_t querylen;
        const char *what;

        if (conn_recv_full (data, optlen, "read: %s: %m", optname) == -1)
          return -1;

        /* Note that we support base:allocation whether or not the plugin
         * supports can_extents.
         */
        if (!conn->structured_replies) {
          if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
              == -1)
            return -1;
          continue;
        }

        /* Minimum length of the option payload is:
         *   32 bit export name length followed by empty export name
         * + 32 bit number of queries followed by no queries
         * = 8 bytes.
         */
        what = "optlen < 8";
        if (optlen < 8) {
        opt_meta_invalid_option_len:
          debug ("newstyle negotiation: %s: invalid option length: %s",
                 optname, what);

          if (send_newstyle_option_reply (option, NBD_REP_ERR_INVALID)
              == -1)
            return -1;
          continue;
        }

        memcpy (&exportnamelen, &data[0], 4);
        exportnamelen = be32toh (exportnamelen);
        what = "validating export name";
        if (check_export_name (option, &data[4], exportnamelen,
                               optlen - 8) == -1)
          goto opt_meta_invalid_option_len;

        /* Remember the export name: the NBD spec says that if the client
         * later uses NBD_OPT_GO on a different export, then the context
         * returned here is not usable.
         */
        if (option == NBD_OPT_SET_META_CONTEXT) {
          conn->exportname_from_set_meta_context =
            strndup (&data[4], exportnamelen);
          if (conn->exportname_from_set_meta_context == NULL) {
            nbdkit_error ("malloc: %m");
            return -1;
          }
        }

        opt_index = 4 + exportnamelen;

        /* Read the number of queries. */
        what = "reading number of queries";
        if (opt_index+4 > optlen)
          goto opt_meta_invalid_option_len;
        memcpy (&nr_queries, &data[opt_index], 4);
        nr_queries = be32toh (nr_queries);
        opt_index += 4;

        /* for LIST: nr_queries == 0 means return all meta contexts
         * for SET: nr_queries == 0 means reset all contexts
         */
        debug ("newstyle negotiation: %s: %s count: %d", optname,
               option == NBD_OPT_LIST_META_CONTEXT ? "query" : "set",
               nr_queries);
        if (option == NBD_OPT_SET_META_CONTEXT)
          conn->meta_context_base_allocation = false;
        if (nr_queries == 0) {
          if (option == NBD_OPT_LIST_META_CONTEXT) {
            if (send_newstyle_option_reply_meta_context (option,
                                                         NBD_REP_META_CONTEXT,
                                                         0, "base:allocation")
                == -1)
              return -1;
          }

          if (send_newstyle_option_reply (option, NBD_REP_ACK) == -1)
            return -1;
        }
        else {
          /* Read and answer each query. */
          while (nr_queries > 0) {
            what = "reading query string length";
            if (opt_index+4 > optlen)
              goto opt_meta_invalid_option_len;
            memcpy (&querylen, &data[opt_index], 4);
            querylen = be32toh (querylen);
            opt_index += 4;
            what = "reading query string";
            if (check_string (option, &data[opt_index], querylen,
                              optlen - opt_index, "meta context query") == -1)
              goto opt_meta_invalid_option_len;

            debug ("newstyle negotiation: %s: %s %.*s",
                   optname,
                   option == NBD_OPT_LIST_META_CONTEXT ? "query" : "set",
                   (int) querylen, &data[opt_index]);

            /* For LIST, "base:" returns all supported contexts in the
             * base namespace.  We only support "base:allocation".
             */
            if (option == NBD_OPT_LIST_META_CONTEXT &&
                querylen == 5 &&
                strncmp (&data[opt_index], "base:", 5) == 0) {
              if (send_newstyle_option_reply_meta_context
                  (option, NBD_REP_META_CONTEXT,
                   0, "base:allocation") == -1)
                return -1;
            }
            /* "base:allocation" requested by name. */
            else if (querylen == 15 &&
                     strncmp (&data[opt_index], "base:allocation", 15) == 0) {
              if (send_newstyle_option_reply_meta_context
                  (option, NBD_REP_META_CONTEXT,
                   option == NBD_OPT_SET_META_CONTEXT
                   ? base_allocation_id : 0,
                   "base:allocation") == -1)
                return -1;
              if (option == NBD_OPT_SET_META_CONTEXT)
                conn->meta_context_base_allocation = true;
            }
            /* Every other query must be ignored. */

            opt_index += querylen;
            nr_queries--;
          }
          if (send_newstyle_option_reply (option, NBD_REP_ACK) == -1)
            return -1;
        }
        debug ("newstyle negotiation: %s: reply complete", optname);
      }
      break;

    default:
      /* Unknown option. */
      if (send_newstyle_option_reply (option, NBD_REP_ERR_UNSUP) == -1)
        return -1;
      if (conn_recv_full (data, optlen,
                          "reading unknown option data: conn->recv: %m") == -1)
        return -1;
    }

    /* Note, since it's not very clear from the protocol doc, that the
     * client must send NBD_OPT_EXPORT_NAME or NBD_OPT_GO last, and
     * that ends option negotiation.
     */
    if (option == NBD_OPT_EXPORT_NAME || option == NBD_OPT_GO)
      break;
  }

  if (nr_options == 0) {
    nbdkit_error ("client spent too much time negotiating without selecting "
                  "an export");
    return -1;
  }

  /* In --tls=require / FORCEDTLS mode, we must have upgraded to TLS
   * by the time we finish option negotiation.  If not, give up.
   */
  if (tls == 2 && !conn->using_tls) {
    nbdkit_error ("non-TLS client tried to connect in --tls=require mode");
    return -1;
  }

  return 0;
}