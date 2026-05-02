wsrep_cb_status_t wsrep_sst_donate_cb (void* app_ctx, void* recv_ctx,
                                       const void* msg, size_t msg_len,
                                       const wsrep_gtid_t* current_gtid,
                                       const char* state, size_t state_len,
                                       bool bypass)
{
  /* This will be reset when sync callback is called.
   * Should we set wsrep_ready to FALSE here too? */
  local_status.set(WSREP_MEMBER_DONOR);

  const char* method = (char*)msg;
  size_t method_len  = strlen (method);
  const char* data   = method + method_len + 1;

  char uuid_str[37];
  wsrep_uuid_print (&current_gtid->uuid, uuid_str, sizeof(uuid_str));

  wsp::env env(NULL);
  if (env.error())
  {
    WSREP_ERROR("wsrep_sst_donate_cb(): env var ctor failed: %d", -env.error());
    return WSREP_CB_FAILURE;
  }

  int ret;
  if ((ret= sst_append_auth_env(env, sst_auth_real)))
  {
    WSREP_ERROR("wsrep_sst_donate_cb(): appending auth env failed: %d", ret);
    return WSREP_CB_FAILURE;
  }

  if (!strcmp (WSREP_SST_MYSQLDUMP, method))
  {
    ret = sst_donate_mysqldump(data, &current_gtid->uuid, uuid_str,
                               current_gtid->seqno, bypass, env());
  }
  else
  {
    ret = sst_donate_other(method, data, uuid_str,
                           current_gtid->seqno, bypass, env());
  }

  return (ret >= 0 ? WSREP_CB_SUCCESS : WSREP_CB_FAILURE);
}