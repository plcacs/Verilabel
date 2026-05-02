int ssh_scp_init(ssh_scp scp)
{
    int rc;
    char execbuffer[1024] = {0};
    char *quoted_location = NULL;
    size_t quoted_location_len = 0;
    size_t scp_location_len;

    if (scp == NULL) {
        return SSH_ERROR;
    }

    if (scp->state != SSH_SCP_NEW) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "ssh_scp_init called under invalid state");
        return SSH_ERROR;
    }

    if (scp->location == NULL) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "Invalid scp context: location is NULL");
        return SSH_ERROR;
    }

    SSH_LOG(SSH_LOG_PROTOCOL, "Initializing scp session %s %son location '%s'",
            scp->mode == SSH_SCP_WRITE?"write":"read",
            scp->recursive ? "recursive " : "",
            scp->location);

    scp->channel = ssh_channel_new(scp->session);
    if (scp->channel == NULL) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "Channel creation failed for scp");
        scp->state = SSH_SCP_ERROR;
        return SSH_ERROR;
    }

    rc = ssh_channel_open_session(scp->channel);
    if (rc == SSH_ERROR) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "Failed to open channel for scp");
        scp->state = SSH_SCP_ERROR;
        return SSH_ERROR;
    }

    /* In the worst case, each character would be replaced by 3 plus the string
     * terminator '\0' */
    scp_location_len = strlen(scp->location);
    quoted_location_len = ((size_t)3 * scp_location_len) + 1;
    /* Paranoia check */
    if (quoted_location_len < scp_location_len) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "Buffer overflow detected");
        scp->state = SSH_SCP_ERROR;
        return SSH_ERROR;
    }

    quoted_location = (char *)calloc(1, quoted_location_len);
    if (quoted_location == NULL) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "Failed to allocate memory for quoted location");
        scp->state = SSH_SCP_ERROR;
        return SSH_ERROR;
    }

    rc = ssh_quote_file_name(scp->location, quoted_location,
                             quoted_location_len);
    if (rc <= 0) {
        ssh_set_error(scp->session, SSH_FATAL,
                      "Failed to single quote command location");
        SAFE_FREE(quoted_location);
        scp->state = SSH_SCP_ERROR;
        return SSH_ERROR;
    }

    if (scp->mode == SSH_SCP_WRITE) {
        snprintf(execbuffer, sizeof(execbuffer), "scp -t %s %s",
                scp->recursive ? "-r" : "", quoted_location);
    } else {
        snprintf(execbuffer, sizeof(execbuffer), "scp -f %s %s",
                scp->recursive ? "-r" : "", quoted_location);
    }

    SAFE_FREE(quoted_location);

    SSH_LOG(SSH_LOG_DEBUG, "Executing command: %s", execbuffer);

    rc = ssh_channel_request_exec(scp->channel, execbuffer);
    if (rc == SSH_ERROR){
        ssh_set_error(scp->session, SSH_FATAL,
                      "Failed executing command: %s", execbuffer);
        scp->state = SSH_SCP_ERROR;
        return SSH_ERROR;
    }

    if (scp->mode == SSH_SCP_WRITE) {
        rc = ssh_scp_response(scp, NULL);
        if (rc != 0) {
            return SSH_ERROR;
        }
    } else {
        ssh_channel_write(scp->channel, "", 1);
    }

    if (scp->mode == SSH_SCP_WRITE) {
        scp->state = SSH_SCP_WRITE_INITED;
    } else {
        scp->state = SSH_SCP_READ_INITED;
    }

    return SSH_OK;
}