CtPtr ProtocolV1::handle_connect_message_2() {
  ldout(cct, 20) << __func__ << dendl;

  ldout(cct, 20) << __func__ << " accept got peer connect_seq "
                 << connect_msg.connect_seq << " global_seq "
                 << connect_msg.global_seq << dendl;

  connection->set_peer_type(connect_msg.host_type);
  connection->policy = messenger->get_policy(connect_msg.host_type);

  ldout(cct, 10) << __func__ << " accept of host_type " << connect_msg.host_type
                 << ", policy.lossy=" << connection->policy.lossy
                 << " policy.server=" << connection->policy.server
                 << " policy.standby=" << connection->policy.standby
                 << " policy.resetcheck=" << connection->policy.resetcheck
		 << " features 0x" << std::hex << (uint64_t)connect_msg.features
		 << std::dec
                 << dendl;

  ceph_msg_connect_reply reply;
  bufferlist authorizer_reply;

  // FIPS zeroization audit 20191115: this memset is not security related.
  memset(&reply, 0, sizeof(reply));
  reply.protocol_version =
      messenger->get_proto_version(connection->peer_type, false);

  // mismatch?
  ldout(cct, 10) << __func__ << " accept my proto " << reply.protocol_version
                 << ", their proto " << connect_msg.protocol_version << dendl;

  if (connect_msg.protocol_version != reply.protocol_version) {
    return send_connect_message_reply(CEPH_MSGR_TAG_BADPROTOVER, reply,
                                      authorizer_reply);
  }

  // require signatures for cephx?
  if (connect_msg.authorizer_protocol == CEPH_AUTH_CEPHX) {
    if (connection->peer_type == CEPH_ENTITY_TYPE_OSD ||
        connection->peer_type == CEPH_ENTITY_TYPE_MDS ||
        connection->peer_type == CEPH_ENTITY_TYPE_MGR) {
      if (cct->_conf->cephx_require_signatures ||
          cct->_conf->cephx_cluster_require_signatures) {
        ldout(cct, 10)
            << __func__
            << " using cephx, requiring MSG_AUTH feature bit for cluster"
            << dendl;
        connection->policy.features_required |= CEPH_FEATURE_MSG_AUTH;
      }
    } else {
      if (cct->_conf->cephx_require_signatures ||
          cct->_conf->cephx_service_require_signatures) {
        ldout(cct, 10)
            << __func__
            << " using cephx, requiring MSG_AUTH feature bit for service"
            << dendl;
        connection->policy.features_required |= CEPH_FEATURE_MSG_AUTH;
      }
    }
    if (cct->_conf->cephx_service_require_version >= 2) {
      connection->policy.features_required |= CEPH_FEATURE_CEPHX_V2;
    }
  }

  uint64_t feat_missing =
      connection->policy.features_required & ~(uint64_t)connect_msg.features;
  if (feat_missing) {
    ldout(cct, 1) << __func__ << " peer missing required features " << std::hex
                  << feat_missing << std::dec << dendl;
    return send_connect_message_reply(CEPH_MSGR_TAG_FEATURES, reply,
                                      authorizer_reply);
  }

  bufferlist auth_bl_copy = authorizer_buf;
  connection->lock.unlock();
  ldout(cct,10) << __func__ << " authorizor_protocol "
		<< connect_msg.authorizer_protocol
		<< " len " << auth_bl_copy.length()
		<< dendl;
  bool authorizer_valid;
  bool need_challenge = HAVE_FEATURE(connect_msg.features, CEPHX_V2);
  bool had_challenge = (bool)authorizer_challenge;
  if (!messenger->ms_deliver_verify_authorizer(
          connection, connection->peer_type, connect_msg.authorizer_protocol,
          auth_bl_copy, authorizer_reply, authorizer_valid, session_key,
	  nullptr /* connection_secret */,
          need_challenge ? &authorizer_challenge : nullptr) ||
      !authorizer_valid) {
    connection->lock.lock();
    if (state != ACCEPTING_WAIT_CONNECT_MSG_AUTH) {
      ldout(cct, 1) << __func__
		    << " state changed while accept, it must be mark_down"
		    << dendl;
      ceph_assert(state == CLOSED);
      return _fault();
    }

    if (need_challenge && !had_challenge && authorizer_challenge) {
      ldout(cct, 10) << __func__ << ": challenging authorizer" << dendl;
      ceph_assert(authorizer_reply.length());
      return send_connect_message_reply(CEPH_MSGR_TAG_CHALLENGE_AUTHORIZER,
                                        reply, authorizer_reply);
    } else {
      ldout(cct, 0) << __func__ << ": got bad authorizer, auth_reply_len="
                    << authorizer_reply.length() << dendl;
      session_security.reset();
      return send_connect_message_reply(CEPH_MSGR_TAG_BADAUTHORIZER, reply,
                                        authorizer_reply);
    }
  }

  // We've verified the authorizer for this AsyncConnection, so set up the
  // session security structure.  PLR
  ldout(cct, 10) << __func__ << " accept setting up session_security." << dendl;

  // existing?
  AsyncConnectionRef existing = messenger->lookup_conn(*connection->peer_addrs);

  connection->inject_delay();

  connection->lock.lock();
  if (state != ACCEPTING_WAIT_CONNECT_MSG_AUTH) {
    ldout(cct, 1) << __func__
                  << " state changed while accept, it must be mark_down"
                  << dendl;
    ceph_assert(state == CLOSED);
    return _fault();
  }

  if (existing == connection) {
    existing = nullptr;
  }
  if (existing && existing->protocol->proto_type != 1) {
    ldout(cct,1) << __func__ << " existing " << existing << " proto "
		 << existing->protocol.get() << " version is "
		 << existing->protocol->proto_type << ", marking down" << dendl;
    existing->mark_down();
    existing = nullptr;
  }

  if (existing) {
    // There is no possible that existing connection will acquire this
    // connection's lock
    existing->lock.lock();  // skip lockdep check (we are locking a second
                            // AsyncConnection here)

    ldout(cct,10) << __func__ << " existing=" << existing << " exproto="
		  << existing->protocol.get() << dendl;
    ProtocolV1 *exproto = dynamic_cast<ProtocolV1 *>(existing->protocol.get());
    ceph_assert(exproto);
    ceph_assert(exproto->proto_type == 1);

    if (exproto->state == CLOSED) {
      ldout(cct, 1) << __func__ << " existing " << existing
		    << " already closed." << dendl;
      existing->lock.unlock();
      existing = nullptr;

      return open(reply, authorizer_reply);
    }

    if (exproto->replacing) {
      ldout(cct, 1) << __func__
                    << " existing racing replace happened while replacing."
                    << " existing_state="
                    << connection->get_state_name(existing->state) << dendl;
      reply.global_seq = exproto->peer_global_seq;
      existing->lock.unlock();
      return send_connect_message_reply(CEPH_MSGR_TAG_RETRY_GLOBAL, reply,
                                        authorizer_reply);
    }

    if (connect_msg.global_seq < exproto->peer_global_seq) {
      ldout(cct, 10) << __func__ << " accept existing " << existing << ".gseq "
                     << exproto->peer_global_seq << " > "
                     << connect_msg.global_seq << ", RETRY_GLOBAL" << dendl;
      reply.global_seq = exproto->peer_global_seq;  // so we can send it below..
      existing->lock.unlock();
      return send_connect_message_reply(CEPH_MSGR_TAG_RETRY_GLOBAL, reply,
                                        authorizer_reply);
    } else {
      ldout(cct, 10) << __func__ << " accept existing " << existing << ".gseq "
                     << exproto->peer_global_seq
                     << " <= " << connect_msg.global_seq << ", looks ok"
                     << dendl;
    }

    if (existing->policy.lossy) {
      ldout(cct, 0)
          << __func__
          << " accept replacing existing (lossy) channel (new one lossy="
          << connection->policy.lossy << ")" << dendl;
      exproto->session_reset();
      return replace(existing, reply, authorizer_reply);
    }

    ldout(cct, 1) << __func__ << " accept connect_seq "
                  << connect_msg.connect_seq
                  << " vs existing csq=" << exproto->connect_seq
                  << " existing_state="
                  << connection->get_state_name(existing->state) << dendl;

    if (connect_msg.connect_seq == 0 && exproto->connect_seq > 0) {
      ldout(cct, 0)
          << __func__
          << " accept peer reset, then tried to connect to us, replacing"
          << dendl;
      // this is a hard reset from peer
      is_reset_from_peer = true;
      if (connection->policy.resetcheck) {
        exproto->session_reset();  // this resets out_queue, msg_ and
                                   // connect_seq #'s
      }
      return replace(existing, reply, authorizer_reply);
    }

    if (connect_msg.connect_seq < exproto->connect_seq) {
      // old attempt, or we sent READY but they didn't get it.
      ldout(cct, 10) << __func__ << " accept existing " << existing << ".cseq "
                     << exproto->connect_seq << " > " << connect_msg.connect_seq
                     << ", RETRY_SESSION" << dendl;
      reply.connect_seq = exproto->connect_seq + 1;
      existing->lock.unlock();
      return send_connect_message_reply(CEPH_MSGR_TAG_RETRY_SESSION, reply,
                                        authorizer_reply);
    }

    if (connect_msg.connect_seq == exproto->connect_seq) {
      // if the existing connection successfully opened, and/or
      // subsequently went to standby, then the peer should bump
      // their connect_seq and retry: this is not a connection race
      // we need to resolve here.
      if (exproto->state == OPENED || exproto->state == STANDBY) {
        ldout(cct, 10) << __func__ << " accept connection race, existing "
                       << existing << ".cseq " << exproto->connect_seq
                       << " == " << connect_msg.connect_seq
                       << ", OPEN|STANDBY, RETRY_SESSION " << dendl;
        // if connect_seq both zero, dont stuck into dead lock. it's ok to
        // replace
        if (connection->policy.resetcheck && exproto->connect_seq == 0) {
          return replace(existing, reply, authorizer_reply);
        }

        reply.connect_seq = exproto->connect_seq + 1;
        existing->lock.unlock();
        return send_connect_message_reply(CEPH_MSGR_TAG_RETRY_SESSION, reply,
                                          authorizer_reply);
      }

      // connection race?
      if (connection->peer_addrs->legacy_addr() < messenger->get_myaddr_legacy() ||
          existing->policy.server) {
        // incoming wins
        ldout(cct, 10) << __func__ << " accept connection race, existing "
                       << existing << ".cseq " << exproto->connect_seq
                       << " == " << connect_msg.connect_seq
                       << ", or we are server, replacing my attempt" << dendl;
        return replace(existing, reply, authorizer_reply);
      } else {
        // our existing outgoing wins
        ldout(messenger->cct, 10)
            << __func__ << " accept connection race, existing " << existing
            << ".cseq " << exproto->connect_seq
            << " == " << connect_msg.connect_seq << ", sending WAIT" << dendl;
        ceph_assert(connection->peer_addrs->legacy_addr() >
                    messenger->get_myaddr_legacy());
        existing->lock.unlock();
	// make sure we follow through with opening the existing
	// connection (if it isn't yet open) since we know the peer
	// has something to send to us.
	existing->send_keepalive();
        return send_connect_message_reply(CEPH_MSGR_TAG_WAIT, reply,
                                          authorizer_reply);
      }
    }

    ceph_assert(connect_msg.connect_seq > exproto->connect_seq);
    ceph_assert(connect_msg.global_seq >= exproto->peer_global_seq);
    if (connection->policy.resetcheck &&  // RESETSESSION only used by servers;
                                          // peers do not reset each other
        exproto->connect_seq == 0) {
      ldout(cct, 0) << __func__ << " accept we reset (peer sent cseq "
                    << connect_msg.connect_seq << ", " << existing
                    << ".cseq = " << exproto->connect_seq
                    << "), sending RESETSESSION " << dendl;
      existing->lock.unlock();
      return send_connect_message_reply(CEPH_MSGR_TAG_RESETSESSION, reply,
                                        authorizer_reply);
    }

    // reconnect
    ldout(cct, 10) << __func__ << " accept peer sent cseq "
                   << connect_msg.connect_seq << " > " << exproto->connect_seq
                   << dendl;
    return replace(existing, reply, authorizer_reply);
  }  // existing
  else if (!replacing && connect_msg.connect_seq > 0) {
    // we reset, and they are opening a new session
    ldout(cct, 0) << __func__ << " accept we reset (peer sent cseq "
                  << connect_msg.connect_seq << "), sending RESETSESSION"
                  << dendl;
    return send_connect_message_reply(CEPH_MSGR_TAG_RESETSESSION, reply,
                                      authorizer_reply);
  } else {
    // new session
    ldout(cct, 10) << __func__ << " accept new session" << dendl;
    existing = nullptr;
    return open(reply, authorizer_reply);
  }
}