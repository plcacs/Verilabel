new_msg_lsa_change_notify (u_char msgtype,
			   u_int32_t seqnum,
			   struct in_addr ifaddr,
			   struct in_addr area_id,
			   u_char is_self_originated, struct lsa_header *data)
{
  u_char buf[OSPF_API_MAX_MSG_SIZE];
  struct msg_lsa_change_notify *nmsg;
  int len;

  assert (data);

  nmsg = (struct msg_lsa_change_notify *) buf;
  len = ntohs (data->length) + sizeof (struct msg_lsa_change_notify)
    - sizeof (struct lsa_header);
  nmsg->ifaddr = ifaddr;
  nmsg->area_id = area_id;
  nmsg->is_self_originated = is_self_originated;
  memset (&nmsg->pad, 0, sizeof (nmsg->pad));
  memcpy (&nmsg->data, data, ntohs (data->length));

  return msg_new (msgtype, nmsg, seqnum, len);
}