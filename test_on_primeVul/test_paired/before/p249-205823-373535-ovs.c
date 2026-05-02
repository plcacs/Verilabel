ipf_extract_frags_from_batch(struct ipf *ipf, struct dp_packet_batch *pb,
                             ovs_be16 dl_type, uint16_t zone, long long now,
                             uint32_t hash_basis)
{
    const size_t pb_cnt = dp_packet_batch_size(pb);
    int pb_idx; /* Index in a packet batch. */
    struct dp_packet *pkt;

    DP_PACKET_BATCH_REFILL_FOR_EACH (pb_idx, pb_cnt, pkt, pb) {
        if (OVS_UNLIKELY((dl_type == htons(ETH_TYPE_IP) &&
                          ipf_is_valid_v4_frag(ipf, pkt))
                          ||
                          (dl_type == htons(ETH_TYPE_IPV6) &&
                          ipf_is_valid_v6_frag(ipf, pkt)))) {

            ovs_mutex_lock(&ipf->ipf_lock);
            if (!ipf_handle_frag(ipf, pkt, dl_type, zone, now, hash_basis)) {
                dp_packet_batch_refill(pb, pkt, pb_idx);
            }
            ovs_mutex_unlock(&ipf->ipf_lock);
        } else {
            dp_packet_batch_refill(pb, pkt, pb_idx);
        }
    }
}