int sudo_process_init(TALLOC_CTX *mem_ctx,
                      struct tevent_context *ev,
                      struct confdb_ctx *cdb)
{
    struct resp_ctx *rctx;
    struct sss_cmd_table *sudo_cmds;
    struct sudo_ctx *sudo_ctx;
    struct be_conn *iter;
    int ret;
    int max_retries;

    sudo_cmds = get_sudo_cmds();
    ret = sss_process_init(mem_ctx, ev, cdb,
                           sudo_cmds,
                           SSS_SUDO_SOCKET_NAME, -1, NULL, -1,
                           CONFDB_SUDO_CONF_ENTRY,
                           SSS_SUDO_SBUS_SERVICE_NAME,
                           SSS_SUDO_SBUS_SERVICE_VERSION,
                           &monitor_sudo_methods,
                           "SUDO",
                           NULL,
                           sss_connection_setup,
                           &rctx);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE, "sss_process_init() failed\n");
        return ret;
    }

    sudo_ctx = talloc_zero(rctx, struct sudo_ctx);
    if (!sudo_ctx) {
        DEBUG(SSSDBG_FATAL_FAILURE, "fatal error initializing sudo_ctx\n");
        ret = ENOMEM;
        goto fail;
    }

    sudo_ctx->rctx = rctx;
    sudo_ctx->rctx->pvt_ctx = sudo_ctx;

    sss_ncache_prepopulate(sudo_ctx->rctx->ncache, sudo_ctx->rctx->cdb, rctx);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE,
              "failed to set ncache for sudo's filter_users\n");
        goto fail;
    }

    /* Enable automatic reconnection to the Data Provider */
    ret = confdb_get_int(sudo_ctx->rctx->cdb,
                         CONFDB_SUDO_CONF_ENTRY,
                         CONFDB_SERVICE_RECON_RETRIES,
                         3, &max_retries);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE,
              "Failed to set up automatic reconnection\n");
        goto fail;
    }

    for (iter = sudo_ctx->rctx->be_conns; iter; iter = iter->next) {
        sbus_reconnect_init(iter->conn, max_retries,
                            sudo_dp_reconnect_init, iter);
    }

    /* Get sudo_timed option */
    ret = confdb_get_bool(sudo_ctx->rctx->cdb,
                          CONFDB_SUDO_CONF_ENTRY, CONFDB_SUDO_TIMED,
                          CONFDB_DEFAULT_SUDO_TIMED,
                          &sudo_ctx->timed);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Error reading from confdb (%d) [%s]\n",
              ret, strerror(ret));
        goto fail;
    }

    /* Get sudo_inverse_order option */
    ret = confdb_get_bool(sudo_ctx->rctx->cdb,
                          CONFDB_SUDO_CONF_ENTRY, CONFDB_SUDO_INVERSE_ORDER,
                          CONFDB_DEFAULT_SUDO_INVERSE_ORDER,
                          &sudo_ctx->inverse_order);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Error reading from confdb (%d) [%s]\n",
              ret, strerror(ret));
        goto fail;
    }

    /* Get sudo_inverse_order option */
    ret = confdb_get_int(sudo_ctx->rctx->cdb,
                         CONFDB_SUDO_CONF_ENTRY, CONFDB_SUDO_THRESHOLD,
                         CONFDB_DEFAULT_SUDO_THRESHOLD,
                         &sudo_ctx->threshold);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE, "Error reading from confdb (%d) [%s]\n",
              ret, strerror(ret));
        goto fail;
    }

    ret = schedule_get_domains_task(rctx, rctx->ev, rctx, NULL);
    if (ret != EOK) {
        DEBUG(SSSDBG_FATAL_FAILURE, "schedule_get_domains_tasks failed.\n");
        goto fail;
    }

    DEBUG(SSSDBG_TRACE_FUNC, "SUDO Initialization complete\n");

    return EOK;

fail:
    talloc_free(rctx);
    return ret;
}