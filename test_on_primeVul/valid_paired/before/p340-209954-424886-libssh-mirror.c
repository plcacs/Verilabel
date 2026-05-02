sftp_client_message sftp_get_client_message(sftp_session sftp) {
  ssh_session session = sftp->session;
  sftp_packet packet;
  sftp_client_message msg;
  ssh_buffer payload;
  int rc;

  msg = malloc(sizeof (struct sftp_client_message_struct));
  if (msg == NULL) {
    ssh_set_error_oom(session);
    return NULL;
  }
  ZERO_STRUCTP(msg);

  packet = sftp_packet_read(sftp);
  if (packet == NULL) {
    ssh_set_error_oom(session);
    sftp_client_message_free(msg);
    return NULL;
  }

  payload = packet->payload;
  msg->type = packet->type;
  msg->sftp = sftp;

  /* take a copy of the whole packet */
  msg->complete_message = ssh_buffer_new();
  if (msg->complete_message == NULL) {
      ssh_set_error_oom(session);
      sftp_client_message_free(msg);
      return NULL;
  }

  ssh_buffer_add_data(msg->complete_message,
                      ssh_buffer_get(payload),
                      ssh_buffer_get_len(payload));

  ssh_buffer_get_u32(payload, &msg->id);

  switch(msg->type) {
    case SSH_FXP_CLOSE:
    case SSH_FXP_READDIR:
      msg->handle = ssh_buffer_get_ssh_string(payload);
      if (msg->handle == NULL) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_READ:
      rc = ssh_buffer_unpack(payload,
                             "Sqd",
                             &msg->handle,
                             &msg->offset,
                             &msg->len);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_WRITE:
      rc = ssh_buffer_unpack(payload,
                             "SqS",
                             &msg->handle,
                             &msg->offset,
                             &msg->data);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_REMOVE:
    case SSH_FXP_RMDIR:
    case SSH_FXP_OPENDIR:
    case SSH_FXP_READLINK:
    case SSH_FXP_REALPATH:
      rc = ssh_buffer_unpack(payload,
                             "s",
                             &msg->filename);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_RENAME:
    case SSH_FXP_SYMLINK:
      rc = ssh_buffer_unpack(payload,
                             "sS",
                             &msg->filename,
                             &msg->data);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_MKDIR:
    case SSH_FXP_SETSTAT:
      rc = ssh_buffer_unpack(payload,
                             "s",
                             &msg->filename);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      msg->attr = sftp_parse_attr(sftp, payload, 0);
      if (msg->attr == NULL) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_FSETSTAT:
      msg->handle = ssh_buffer_get_ssh_string(payload);
      if (msg->handle == NULL) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      msg->attr = sftp_parse_attr(sftp, payload, 0);
      if (msg->attr == NULL) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_LSTAT:
    case SSH_FXP_STAT:
      rc = ssh_buffer_unpack(payload,
                             "s",
                             &msg->filename);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      if(sftp->version > 3) {
        ssh_buffer_unpack(payload, "d", &msg->flags);
      }
      break;
    case SSH_FXP_OPEN:
      rc = ssh_buffer_unpack(payload,
                             "sd",
                             &msg->filename,
                             &msg->flags);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      msg->attr = sftp_parse_attr(sftp, payload, 0);
      if (msg->attr == NULL) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_FSTAT:
      rc = ssh_buffer_unpack(payload,
                             "S",
                             &msg->handle);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }
      break;
    case SSH_FXP_EXTENDED:
      rc = ssh_buffer_unpack(payload,
                             "s",
                             &msg->submessage);
      if (rc != SSH_OK) {
        ssh_set_error_oom(session);
        sftp_client_message_free(msg);
        return NULL;
      }

      if (strcmp(msg->submessage, "hardlink@openssh.com") == 0 ||
          strcmp(msg->submessage, "posix-rename@openssh.com") == 0) {
        rc = ssh_buffer_unpack(payload,
                               "sS",
                               &msg->filename,
                               &msg->data);
        if (rc != SSH_OK) {
          ssh_set_error_oom(session);
          sftp_client_message_free(msg);
          return NULL;
        }
      }
      break;
    default:
      ssh_set_error(sftp->session, SSH_FATAL,
                    "Received unhandled sftp message %d", msg->type);
      sftp_client_message_free(msg);
      return NULL;
  }

  return msg;
}