void ndpi_search_oracle(struct ndpi_detection_module_struct *ndpi_struct, struct ndpi_flow_struct *flow)
{
  struct ndpi_packet_struct *packet = &flow->packet;
  u_int16_t dport = 0, sport = 0;

  NDPI_LOG_DBG(ndpi_struct, "search ORACLE\n");

  if(packet->tcp != NULL) {
    sport = ntohs(packet->tcp->source), dport = ntohs(packet->tcp->dest);
    NDPI_LOG_DBG2(ndpi_struct, "calculating ORACLE over tcp\n");
    /* Oracle Database 9g,10g,11g */
    if ((dport == 1521 || sport == 1521)
	&&  (((packet->payload[0] == 0x07) && (packet->payload[1] == 0xff) && (packet->payload[2] == 0x00))
	     || ((packet->payload_packet_len >= 232) && ((packet->payload[0] == 0x00) || (packet->payload[0] == 0x01)) 
	     && (packet->payload[1] != 0x00)
	     && (packet->payload[2] == 0x00)
		 && (packet->payload[3] == 0x00)))) {
      NDPI_LOG_INFO(ndpi_struct, "found oracle\n");
      ndpi_int_oracle_add_connection(ndpi_struct, flow);
    } else if (packet->payload_packet_len == 213 && packet->payload[0] == 0x00 &&
               packet->payload[1] == 0xd5 && packet->payload[2] == 0x00 &&
               packet->payload[3] == 0x00 ) {
      NDPI_LOG_INFO(ndpi_struct, "found oracle\n");
      ndpi_int_oracle_add_connection(ndpi_struct, flow);
    }
  } else {
    NDPI_EXCLUDE_PROTO(ndpi_struct, flow);
  }
}