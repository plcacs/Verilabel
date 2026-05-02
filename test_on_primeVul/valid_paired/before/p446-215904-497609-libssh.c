int ssh_bind_accept_fd(ssh_bind sshbind, ssh_session session, socket_t fd){
    int i, rc;

    if (session == NULL){
        ssh_set_error(sshbind, SSH_FATAL,"session is null");
        return SSH_ERROR;
    }

    session->server = 1;
    session->version = 2;

    /* copy options */
    for (i = 0; i < 10; ++i) {
      if (sshbind->wanted_methods[i]) {
        session->opts.wanted_methods[i] = strdup(sshbind->wanted_methods[i]);
        if (session->opts.wanted_methods[i] == NULL) {
          return SSH_ERROR;
        }
      }
    }

    if (sshbind->bindaddr == NULL)
      session->opts.bindaddr = NULL;
    else {
      SAFE_FREE(session->opts.bindaddr);
      session->opts.bindaddr = strdup(sshbind->bindaddr);
      if (session->opts.bindaddr == NULL) {
        return SSH_ERROR;
      }
    }

    session->common.log_verbosity = sshbind->common.log_verbosity;
    if(sshbind->banner != NULL)
    	session->opts.custombanner = strdup(sshbind->banner);
    ssh_socket_free(session->socket);
    session->socket = ssh_socket_new(session);
    if (session->socket == NULL) {
      /* perhaps it may be better to copy the error from session to sshbind */
      ssh_set_error_oom(sshbind);
      return SSH_ERROR;
    }
    ssh_socket_set_fd(session->socket, fd);
    ssh_socket_get_poll_handle_out(session->socket);

    /* We must try to import any keys that could be imported in case
     * we are not using ssh_bind_listen (which is the other place
     * where keys can be imported) on this ssh_bind and are instead
     * only using ssh_bind_accept_fd to manage sockets ourselves.
     */
    rc = ssh_bind_import_keys(sshbind);
    if (rc != SSH_OK) {
      return SSH_ERROR;
    }

#ifdef HAVE_ECC
    if (sshbind->ecdsa) {
        session->srv.ecdsa_key = ssh_key_dup(sshbind->ecdsa);
        if (session->srv.ecdsa_key == NULL) {
          ssh_set_error_oom(sshbind);
          return SSH_ERROR;
        }
    }
#endif
    if (sshbind->dsa) {
        session->srv.dsa_key = ssh_key_dup(sshbind->dsa);
        if (session->srv.dsa_key == NULL) {
          ssh_set_error_oom(sshbind);
          return SSH_ERROR;
        }
    }
    if (sshbind->rsa) {
        session->srv.rsa_key = ssh_key_dup(sshbind->rsa);
        if (session->srv.rsa_key == NULL) {
          ssh_set_error_oom(sshbind);
          return SSH_ERROR;
        }
    }
    return SSH_OK;
}