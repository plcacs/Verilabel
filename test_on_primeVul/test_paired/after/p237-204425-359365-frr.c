bgp_capability_msg_parse (struct peer *peer, u_char *pnt, bgp_size_t length)
{
  u_char *end;
  struct capability_mp_data mpc;
  struct capability_header *hdr;
  u_char action;
  struct bgp *bgp;
  afi_t afi;
  safi_t safi;

  bgp = peer->bgp;
  end = pnt + length;

  while (pnt < end)
    {      
      /* We need at least action, capability code and capability length. */
      if (pnt + 3 > end)
        {
          zlog_info ("%s Capability length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }
      action = *pnt;
      hdr = (struct capability_header *)(pnt + 1);
      
      /* Action value check.  */
      if (action != CAPABILITY_ACTION_SET
	  && action != CAPABILITY_ACTION_UNSET)
        {
          zlog_info ("%s Capability Action Value error %d",
		     peer->host, action);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      if (BGP_DEBUG (normal, NORMAL))
	zlog_debug ("%s CAPABILITY has action: %d, code: %u, length %u",
		   peer->host, action, hdr->code, hdr->length);

      /* Capability length check. */
      if ((pnt + hdr->length + 3) > end)
        {
          zlog_info ("%s Capability length error", peer->host);
          bgp_notify_send (peer, BGP_NOTIFY_CEASE, 0);
          return -1;
        }

      /* Fetch structure to the byte stream. */
      memcpy (&mpc, pnt + 3, sizeof (struct capability_mp_data));

      /* We know MP Capability Code. */
      if (hdr->code == CAPABILITY_CODE_MP)
        {
	  afi = ntohs (mpc.afi);
	  safi = mpc.safi;

          /* Ignore capability when override-capability is set. */
          if (CHECK_FLAG (peer->flags, PEER_FLAG_OVERRIDE_CAPABILITY))
	    continue;
          
          if (!bgp_afi_safi_valid_indices (afi, &safi))
            {
              if (BGP_DEBUG (normal, NORMAL))
                zlog_debug ("%s Dynamic Capability MP_EXT afi/safi invalid",
                            peer->host, afi, safi);
              continue;
            }
          
	  /* Address family check.  */
          if (BGP_DEBUG (normal, NORMAL))
            zlog_debug ("%s CAPABILITY has %s MP_EXT CAP for afi/safi: %u/%u",
                       peer->host,
                       action == CAPABILITY_ACTION_SET 
                       ? "Advertising" : "Removing",
                       ntohs(mpc.afi) , mpc.safi);
              
          if (action == CAPABILITY_ACTION_SET)
            {
              peer->afc_recv[afi][safi] = 1;
              if (peer->afc[afi][safi])
                {
                  peer->afc_nego[afi][safi] = 1;
                  bgp_announce_route (peer, afi, safi);
                }
            }
          else
            {
              peer->afc_recv[afi][safi] = 0;
              peer->afc_nego[afi][safi] = 0;

              if (peer_active_nego (peer))
                bgp_clear_route (peer, afi, safi);
              else
                BGP_EVENT_ADD (peer, BGP_Stop);
            }
        }
      else
        {
          zlog_warn ("%s unrecognized capability code: %d - ignored",
                     peer->host, hdr->code);
        }
      pnt += hdr->length + 3;
    }
  return 0;
}