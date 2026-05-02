ssh_scp ssh_scp_new(ssh_session session, int mode, const char *location)
{
    ssh_scp scp = NULL;

    if (session == NULL) {
        goto error;
    }

    scp = (ssh_scp)calloc(1, sizeof(struct ssh_scp_struct));
    if (scp == NULL) {
        ssh_set_error(session, SSH_FATAL,
                      "Error allocating memory for ssh_scp");
        goto error;
    }

    if ((mode & ~SSH_SCP_RECURSIVE) != SSH_SCP_WRITE &&
        (mode & ~SSH_SCP_RECURSIVE) != SSH_SCP_READ)
    {
        ssh_set_error(session, SSH_FATAL,
                      "Invalid mode %d for ssh_scp_new()", mode);
        goto error;
    }

    scp->location = strdup(location);
    if (scp->location == NULL) {
        ssh_set_error(session, SSH_FATAL,
                      "Error allocating memory for ssh_scp");
        goto error;
    }

    scp->session = session;
    scp->mode = mode & ~SSH_SCP_RECURSIVE;
    scp->recursive = (mode & SSH_SCP_RECURSIVE) != 0;
    scp->channel = NULL;
    scp->state = SSH_SCP_NEW;

    return scp;

error:
    ssh_scp_free(scp);
    return NULL;
}