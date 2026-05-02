static void do_client_file_xfer(VirtioPort *vport,
                                VDAgentMessage *message_header,
                                uint8_t *data)
{
    uint32_t msg_type, id;
    UdscsConnection *conn;

    switch (message_header->type) {
    case VD_AGENT_FILE_XFER_START: {
        VDAgentFileXferStartMessage *s = (VDAgentFileXferStartMessage *)data;
        if (!active_session_conn) {
            send_file_xfer_status(vport,
               "Could not find an agent connection belonging to the "
               "active session, cancelling client file-xfer request %u",
               s->id, VD_AGENT_FILE_XFER_STATUS_VDAGENT_NOT_CONNECTED, NULL, 0);
            return;
        } else if (session_info_session_is_locked(session_info)) {
            syslog(LOG_DEBUG, "Session is locked, skipping file-xfer-start");
            send_file_xfer_status(vport,
               "User's session is locked and cannot start file transfer. "
               "Cancelling client file-xfer request %u",
               s->id, VD_AGENT_FILE_XFER_STATUS_SESSION_LOCKED, NULL, 0);
            return;
        } else if (g_hash_table_size(active_xfers) >= MAX_ACTIVE_TRANSFERS) {
            VDAgentFileXferStatusError error = {
                GUINT32_TO_LE(VD_AGENT_FILE_XFER_STATUS_ERROR_GLIB_IO),
                GUINT32_TO_LE(G_IO_ERROR_TOO_MANY_OPEN_FILES),
            };
            size_t detail_size = sizeof(error);
            if (!VD_AGENT_HAS_CAPABILITY(capabilities, capabilities_size,
                                         VD_AGENT_CAP_FILE_XFER_DETAILED_ERRORS)) {
                detail_size = 0;
            }
            send_file_xfer_status(vport,
               "Too many transfers ongoing. "
               "Cancelling client file-xfer request %u",
               s->id, VD_AGENT_FILE_XFER_STATUS_ERROR, (void*) &error, detail_size);
            return;
        }
        msg_type = VDAGENTD_FILE_XFER_START;
        id = s->id;
        // associate the id with the active connection
        g_hash_table_insert(active_xfers, GUINT_TO_POINTER(id), active_session_conn);
        break;
    }
    case VD_AGENT_FILE_XFER_STATUS: {
        VDAgentFileXferStatusMessage *s = (VDAgentFileXferStatusMessage *)data;
        msg_type = VDAGENTD_FILE_XFER_STATUS;
        id = s->id;
        break;
    }
    case VD_AGENT_FILE_XFER_DATA: {
        VDAgentFileXferDataMessage *d = (VDAgentFileXferDataMessage *)data;
        msg_type = VDAGENTD_FILE_XFER_DATA;
        id = d->id;
        break;
    }
    default:
        g_return_if_reached(); /* quiet uninitialized variable warning */
    }

    conn = g_hash_table_lookup(active_xfers, GUINT_TO_POINTER(id));
    if (!conn) {
        if (debug)
            syslog(LOG_DEBUG, "Could not find file-xfer %u (cancelled?)", id);
        return;
    }
    udscs_write(conn, msg_type, 0, 0, data, message_header->size);

    // client told that transfer is ended, agents too stop the transfer
    // and release resources
    if (message_header->type == VD_AGENT_FILE_XFER_STATUS) {
        g_hash_table_remove(active_xfers, GUINT_TO_POINTER(id));
    }
}