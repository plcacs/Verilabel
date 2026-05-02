int wsrep_sst_donate(const std::string &msg, const wsrep::gtid &current_gtid,
                     const bool bypass) {
  /* This will be reset when sync callback is called.
   * Should we set wsrep_ready to false here too? */
  local_status.set(wsrep::server_state::s_donor);

  const char *method = msg.data();
  size_t method_len = strlen(method);
  const char *data = method + method_len + 1;

  wsp::env env(NULL);
  if (env.error()) {
    WSREP_ERROR("wsrep_sst_donate_cb(): env var ctor failed: %d", -env.error());
    return WSREP_CB_FAILURE;
  }

#if 0
  /* Wait for wsrep-SE to initialize that also signals
  completion of init_server_component which is important before we initiate
  any meangiful action especially DONOR action from this node. */
  while (!wsrep_is_SE_initialized()) {
    sleep(1);
    THD *applier_thd = static_cast<THD *>(recv_ctx);
    if (applier_thd->killed == THD::KILL_CONNECTION) return WSREP_CB_FAILURE;
  }
#endif

  int ret;
  ret = sst_donate_other(method, data, current_gtid, bypass, env());

  return (ret >= 0 ? WSREP_CB_SUCCESS : WSREP_CB_FAILURE);
}