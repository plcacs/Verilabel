static Array HHVM_METHOD(Memcache, getextendedstats,
                         const String& /*type*/ /* = null_string */,
                         int /*slabid*/ /* = 0 */, int /*limit*/ /* = 100 */) {
  auto data = Native::data<MemcacheData>(this_);
  memcached_return_t ret;
  memcached_stat_st *stats;

  stats = memcached_stat(&data->m_memcache, nullptr, &ret);
  if (ret != MEMCACHED_SUCCESS) {
    return Array();
  }

  int server_count = memcached_server_count(&data->m_memcache);

  Array return_val;

  for (int server_id = 0; server_id < server_count; server_id++) {
    memcached_stat_st *stat;
    char stats_key[30] = {0};
    size_t key_len;

    LMCD_SERVER_POSITION_INSTANCE_TYPE instance =
      memcached_server_instance_by_position(&data->m_memcache, server_id);
    const char *hostname = LMCD_SERVER_HOSTNAME(instance);
    in_port_t port = LMCD_SERVER_PORT(instance);

    stat = stats + server_id;

    Array server_stats = memcache_build_stats(&data->m_memcache, stat, &ret);
    if (ret != MEMCACHED_SUCCESS) {
      continue;
    }

    key_len = snprintf(stats_key, sizeof(stats_key), "%s:%d", hostname, port);

    return_val.set(String(stats_key, key_len, CopyString), server_stats);
  }

  free(stats);
  return return_val;
}