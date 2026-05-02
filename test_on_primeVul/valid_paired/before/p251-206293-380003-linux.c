

static int
iscsi_if_recv_msg(struct sk_buff *skb, struct nlmsghdr *nlh, uint32_t *group)
{
	int err = 0;
	u32 portid;
	struct iscsi_uevent *ev = nlmsg_data(nlh);
	struct iscsi_transport *transport = NULL;
	struct iscsi_internal *priv;
	struct iscsi_cls_session *session;
	struct iscsi_cls_conn *conn;
	struct iscsi_endpoint *ep = NULL;

	if (!netlink_capable(skb, CAP_SYS_ADMIN))
		return -EPERM;

	if (nlh->nlmsg_type == ISCSI_UEVENT_PATH_UPDATE)
		*group = ISCSI_NL_GRP_UIP;
	else
		*group = ISCSI_NL_GRP_ISCSID;

	priv = iscsi_if_transport_lookup(iscsi_ptr(ev->transport_handle));
	if (!priv)
		return -EINVAL;
	transport = priv->iscsi_transport;

	if (!try_module_get(transport->owner))
		return -EINVAL;

	portid = NETLINK_CB(skb).portid;

	switch (nlh->nlmsg_type) {
	case ISCSI_UEVENT_CREATE_SESSION:
		err = iscsi_if_create_session(priv, ep, ev,
					      portid,
					      ev->u.c_session.initial_cmdsn,
					      ev->u.c_session.cmds_max,
					      ev->u.c_session.queue_depth);
		break;
	case ISCSI_UEVENT_CREATE_BOUND_SESSION:
		ep = iscsi_lookup_endpoint(ev->u.c_bound_session.ep_handle);
		if (!ep) {
			err = -EINVAL;
			break;
		}

		err = iscsi_if_create_session(priv, ep, ev,
					portid,
					ev->u.c_bound_session.initial_cmdsn,
					ev->u.c_bound_session.cmds_max,
					ev->u.c_bound_session.queue_depth);
		break;
	case ISCSI_UEVENT_DESTROY_SESSION:
		session = iscsi_session_lookup(ev->u.d_session.sid);
		if (!session)
			err = -EINVAL;
		else if (iscsi_session_has_conns(ev->u.d_session.sid))
			err = -EBUSY;
		else
			transport->destroy_session(session);
		break;
	case ISCSI_UEVENT_DESTROY_SESSION_ASYNC:
		session = iscsi_session_lookup(ev->u.d_session.sid);
		if (!session)
			err = -EINVAL;
		else if (iscsi_session_has_conns(ev->u.d_session.sid))
			err = -EBUSY;
		else {
			unsigned long flags;

			/* Prevent this session from being found again */
			spin_lock_irqsave(&sesslock, flags);
			list_del_init(&session->sess_list);
			spin_unlock_irqrestore(&sesslock, flags);

			queue_work(iscsi_destroy_workq, &session->destroy_work);
		}
		break;
	case ISCSI_UEVENT_UNBIND_SESSION:
		session = iscsi_session_lookup(ev->u.d_session.sid);
		if (session)
			scsi_queue_work(iscsi_session_to_shost(session),
					&session->unbind_work);
		else
			err = -EINVAL;
		break;
	case ISCSI_UEVENT_CREATE_CONN:
		err = iscsi_if_create_conn(transport, ev);
		break;
	case ISCSI_UEVENT_DESTROY_CONN:
		err = iscsi_if_destroy_conn(transport, ev);
		break;
	case ISCSI_UEVENT_BIND_CONN:
		session = iscsi_session_lookup(ev->u.b_conn.sid);
		conn = iscsi_conn_lookup(ev->u.b_conn.sid, ev->u.b_conn.cid);

		if (conn && conn->ep)
			iscsi_if_ep_disconnect(transport, conn->ep->id);

		if (!session || !conn) {
			err = -EINVAL;
			break;
		}

		mutex_lock(&conn_mutex);
		ev->r.retcode =	transport->bind_conn(session, conn,
						ev->u.b_conn.transport_eph,
						ev->u.b_conn.is_leading);
		mutex_unlock(&conn_mutex);

		if (ev->r.retcode || !transport->ep_connect)
			break;

		ep = iscsi_lookup_endpoint(ev->u.b_conn.transport_eph);
		if (ep) {
			ep->conn = conn;

			mutex_lock(&conn->ep_mutex);
			conn->ep = ep;
			mutex_unlock(&conn->ep_mutex);
		} else
			iscsi_cls_conn_printk(KERN_ERR, conn,
					      "Could not set ep conn "
					      "binding\n");
		break;
	case ISCSI_UEVENT_SET_PARAM:
		err = iscsi_set_param(transport, ev);
		break;
	case ISCSI_UEVENT_START_CONN:
		conn = iscsi_conn_lookup(ev->u.start_conn.sid, ev->u.start_conn.cid);
		if (conn) {
			mutex_lock(&conn_mutex);
			ev->r.retcode = transport->start_conn(conn);
			if (!ev->r.retcode)
				conn->state = ISCSI_CONN_UP;
			mutex_unlock(&conn_mutex);
		}
		else
			err = -EINVAL;
		break;
	case ISCSI_UEVENT_STOP_CONN:
		conn = iscsi_conn_lookup(ev->u.stop_conn.sid, ev->u.stop_conn.cid);
		if (conn)
			iscsi_if_stop_conn(conn, ev->u.stop_conn.flag);
		else
			err = -EINVAL;
		break;
	case ISCSI_UEVENT_SEND_PDU:
		conn = iscsi_conn_lookup(ev->u.send_pdu.sid, ev->u.send_pdu.cid);
		if (conn) {
			mutex_lock(&conn_mutex);
			ev->r.retcode =	transport->send_pdu(conn,
				(struct iscsi_hdr*)((char*)ev + sizeof(*ev)),
				(char*)ev + sizeof(*ev) + ev->u.send_pdu.hdr_size,
				ev->u.send_pdu.data_size);
			mutex_unlock(&conn_mutex);
		}
		else
			err = -EINVAL;
		break;
	case ISCSI_UEVENT_GET_STATS:
		err = iscsi_if_get_stats(transport, nlh);
		break;
	case ISCSI_UEVENT_TRANSPORT_EP_CONNECT:
	case ISCSI_UEVENT_TRANSPORT_EP_POLL:
	case ISCSI_UEVENT_TRANSPORT_EP_DISCONNECT:
	case ISCSI_UEVENT_TRANSPORT_EP_CONNECT_THROUGH_HOST:
		err = iscsi_if_transport_ep(transport, ev, nlh->nlmsg_type);
		break;
	case ISCSI_UEVENT_TGT_DSCVR:
		err = iscsi_tgt_dscvr(transport, ev);
		break;
	case ISCSI_UEVENT_SET_HOST_PARAM:
		err = iscsi_set_host_param(transport, ev);
		break;
	case ISCSI_UEVENT_PATH_UPDATE:
		err = iscsi_set_path(transport, ev);
		break;
	case ISCSI_UEVENT_SET_IFACE_PARAMS:
		err = iscsi_set_iface_params(transport, ev,
					     nlmsg_attrlen(nlh, sizeof(*ev)));
		break;
	case ISCSI_UEVENT_PING:
		err = iscsi_send_ping(transport, ev);
		break;
	case ISCSI_UEVENT_GET_CHAP:
		err = iscsi_get_chap(transport, nlh);
		break;
	case ISCSI_UEVENT_DELETE_CHAP:
		err = iscsi_delete_chap(transport, ev);
		break;
	case ISCSI_UEVENT_SET_FLASHNODE_PARAMS:
		err = iscsi_set_flashnode_param(transport, ev,
						nlmsg_attrlen(nlh,
							      sizeof(*ev)));
		break;
	case ISCSI_UEVENT_NEW_FLASHNODE:
		err = iscsi_new_flashnode(transport, ev,
					  nlmsg_attrlen(nlh, sizeof(*ev)));
		break;
	case ISCSI_UEVENT_DEL_FLASHNODE:
		err = iscsi_del_flashnode(transport, ev);
		break;
	case ISCSI_UEVENT_LOGIN_FLASHNODE:
		err = iscsi_login_flashnode(transport, ev);
		break;
	case ISCSI_UEVENT_LOGOUT_FLASHNODE:
		err = iscsi_logout_flashnode(transport, ev);
		break;
	case ISCSI_UEVENT_LOGOUT_FLASHNODE_SID:
		err = iscsi_logout_flashnode_sid(transport, ev);
		break;
	case ISCSI_UEVENT_SET_CHAP:
		err = iscsi_set_chap(transport, ev,
				     nlmsg_attrlen(nlh, sizeof(*ev)));
		break;
	case ISCSI_UEVENT_GET_HOST_STATS:
		err = iscsi_get_host_stats(transport, nlh);
		break;
	default:
		err = -ENOSYS;
		break;
	}
