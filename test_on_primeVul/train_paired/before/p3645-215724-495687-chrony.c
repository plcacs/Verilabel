PKL_ReplyLength(CMD_Reply *r)
{
  int type;
  type = ntohs(r->reply);
  /* Note that reply type codes start from 1, not 0 */
  if (type < 1 || type >= N_REPLY_TYPES) {
    return 0;
  } else {
    switch (type) {
      case RPY_NULL:
        return offsetof(CMD_Reply, data.null.EOR);
      case RPY_N_SOURCES:
        return offsetof(CMD_Reply, data.n_sources.EOR);
      case RPY_SOURCE_DATA:
        return offsetof(CMD_Reply, data.source_data.EOR);
      case RPY_MANUAL_TIMESTAMP:
        return offsetof(CMD_Reply, data.manual_timestamp.EOR);
      case RPY_TRACKING:
        return offsetof(CMD_Reply, data.tracking.EOR);
      case RPY_SOURCESTATS:
        return offsetof(CMD_Reply, data.sourcestats.EOR);
      case RPY_RTC:
        return offsetof(CMD_Reply, data.rtc.EOR);
      case RPY_SUBNETS_ACCESSED :
        {
          unsigned long ns = ntohl(r->data.subnets_accessed.n_subnets);
          if (r->status == htons(STT_SUCCESS)) {
            return (offsetof(CMD_Reply, data.subnets_accessed.subnets) +
                    ns * sizeof(RPY_SubnetsAccessed_Subnet));
          } else {
            return offsetof(CMD_Reply, data);
          }
        }
      case RPY_CLIENT_ACCESSES:
        {
          unsigned long nc = ntohl(r->data.client_accesses.n_clients);
          if (r->status == htons(STT_SUCCESS)) {
            return (offsetof(CMD_Reply, data.client_accesses.clients) +
                    nc * sizeof(RPY_ClientAccesses_Client));
          } else {
            return offsetof(CMD_Reply, data);
          }
        }
      case RPY_CLIENT_ACCESSES_BY_INDEX:
        {
          unsigned long nc = ntohl(r->data.client_accesses_by_index.n_clients);
          if (r->status == htons(STT_SUCCESS)) {
            return (offsetof(CMD_Reply, data.client_accesses_by_index.clients) +
                    nc * sizeof(RPY_ClientAccesses_Client));
          } else {
            return offsetof(CMD_Reply, data);
          }
        }
      case RPY_MANUAL_LIST:
        {
          unsigned long ns = ntohl(r->data.manual_list.n_samples);
          if (r->status == htons(STT_SUCCESS)) {
            return (offsetof(CMD_Reply, data.manual_list.samples) +
                    ns * sizeof(RPY_ManualListSample));
          } else {
            return offsetof(CMD_Reply, data);
          }
        }
      case RPY_ACTIVITY:
        return offsetof(CMD_Reply, data.activity.EOR);
        
      default:
        assert(0);
    }
  }

  return 0;
}